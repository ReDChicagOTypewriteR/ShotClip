// Copyright (c) 2026 AleXJokeR. SPDX-License-Identifier: GPL-3.0-or-later
#include "agent/medialibrary.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

class TestMediaLibrary : public QObject
{
    Q_OBJECT
    static void write(const QString &path, const QByteArray &bytes, bool executable = false)
    {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        if (executable) QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
private slots:
    void timestampsAndInvalidRanges()
    {
        QString error;
        auto result = MediaLibrary::parseSrt("1\r\n00:00:01,000 --> 00:00:02,000\r\n家庭影响\r\n", 300, 5, &error);
        QVERIFY(error.isEmpty()); QCOMPARE(result[0].toObject()["start"].toDouble(), 301.0);
        QCOMPARE(result[0].toObject()["text"].toString(), QString("家庭影响"));
        QVERIFY(MediaLibrary::parseSrt("1\n00:00:03,000 --> 00:00:02,000\n错误", 0, 5, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(MediaLibrary::parseSrt("broken", 0, 5, &error).isEmpty()); QVERIFY(!error.isEmpty());
        QVERIFY(MediaLibrary::parseSrt("1\n00:61:00,000 --> 01:02:00,000\n错误", 0, 4000, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(MediaLibrary::parseSrt("1\n00:00:00,000 --> 00:00:02,000\n甲\n\n2\n00:00:01,000 --> 00:00:03,000\n乙", 0, 5, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(MediaLibrary::parseSrt("", -1, 5, &error).isEmpty()); QVERIFY(!error.isEmpty());
        QVERIFY(MediaLibrary::parseSrt("", 0, 5, &error).isEmpty()); QVERIFY(error.isEmpty());
    }
    void persistenceDuplicateAndLock()
    {
        QTemporaryDir dir; auto path = dir.filePath("中文 素材.mp4"); write(path, "media");
        {
            MediaLibrary library(dir.filePath("cache")); library.addFiles({path, path});
            QCOMPARE(library.items().size(), 1);
            MediaLibrary second(dir.filePath("cache")); QVERIFY(!second.error().isEmpty());
        }
        MediaLibrary reopened(dir.filePath("cache")); QCOMPARE(reopened.items().size(), 1);
        QCOMPARE(reopened.items()[0].toObject()["state"].toString(), QString("pending"));
    }
    void corruptIndexPreserved()
    {
        QTemporaryDir dir; write(dir.filePath("index.json"), "broken");
        MediaLibrary library(dir.path()); QVERIFY(!library.error().isEmpty()); library.addFiles({});
        QFile file(dir.filePath("index.json")); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("broken"));
    }
    void invalidCacheRecordPreserved()
    {
        QTemporaryDir dir;
        const QByteArray broken = "{\"version\":1,\"items\":[{\"id\":\"x\",\"completed\":-1}]}";
        write(dir.filePath("index.json"), broken);
        MediaLibrary library(dir.path()); QVERIFY(!library.error().isEmpty());
        library.retry(); library.addFiles({});
        QFile file(dir.filePath("index.json")); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), broken);
    }
    void missingEngineLeavesQueueIntact()
    {
        QTemporaryDir dir; auto path = dir.filePath("source.wav"); write(path, "media");
        MediaLibrary library(dir.filePath("cache")); library.addFiles({path});
        const auto before = library.items(); QSignalSpy messages(&library, &MediaLibrary::progress);
        library.start({}, {}, {}, {}, false);
        QVERIFY(!library.running()); QCOMPARE(library.items(), before); QVERIFY(!messages.isEmpty());
    }
    void realAudioExtractionAndResumedTranscription()
    {
#ifdef Q_OS_WIN
        QSKIP("POSIX fake whisper fixture; run on macOS/Linux");
#else
        auto ffmpeg = QStandardPaths::findExecutable("ffmpeg"), probe = QStandardPaths::findExecutable("ffprobe");
        if (ffmpeg.isEmpty() || probe.isEmpty()) QSKIP("FFmpeg not installed");
        QTemporaryDir dir; const auto source = dir.filePath("中文 演讲.wav");
        QProcess generate; generate.start(ffmpeg, {"-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=302", "-ar", "16000", source});
        QVERIFY(generate.waitForFinished(30000)); QCOMPARE(generate.exitCode(), 0);
        auto model = dir.filePath("model.bin"), whisper = dir.filePath("whisper"); write(model, "fixture");
        write(whisper, "#!/bin/sh\nwhile [ $# -gt 0 ]; do if [ \"$1\" = '-of' ]; then shift; out=$1; fi; shift; done\nprintf '1\\n00:00:00,000 --> 00:00:01,000\\n家庭影响\\n' > \"$out.srt\"\n", true);
        {
            MediaLibrary library(dir.filePath("cache")); library.addFiles({source});
            connect(&library, &MediaLibrary::changed, &library, [&library] {
                if (library.running() && library.items()[0].toObject()["completed"].toDouble() == 300) library.pause();
            });
            library.start(probe, ffmpeg, whisper, model, false);
            QTRY_VERIFY_WITH_TIMEOUT(!library.running(), 30000);
            QCOMPARE(library.items()[0].toObject()["completed"].toDouble(), 300.0);
        }
        MediaLibrary reopened(dir.filePath("cache")); reopened.start(probe, ffmpeg, whisper, model, false);
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.running(), 30000);
        QCOMPARE(reopened.items()[0].toObject()["state"].toString(), QString("ready"));
        auto hits = reopened.search("家庭"); QCOMPARE(hits.size(), 2);
        QCOMPARE(hits[1].toObject()["start"].toDouble(), 300.0);
        reopened.start(probe, ffmpeg, whisper, model, false);
        QVERIFY(!reopened.running()); QCOMPARE(reopened.search("家庭").size(), 2);
        // Changing model metadata invalidates the transcription, rather than mixing two models.
        write(model, "a different fixture");
        reopened.start(probe, ffmpeg, whisper, model, false);
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.running(), 30000);
        QCOMPARE(reopened.search("家庭").size(), 2);
        // Changed originals are refused before extraction; cached evidence is not applied to new media.
        write(source, "changed source");
        write(model, "third fixture");
        reopened.start(probe, ffmpeg, whisper, model, false);
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.running(), 30000);
        QCOMPARE(reopened.items()[0].toObject()["state"].toString(), QString("failed"));
        QVERIFY(reopened.items()[0].toObject()["error"].toString().contains("改变"));
        reopened.retry(); QCOMPARE(reopened.items()[0].toObject()["state"].toString(), QString("pending"));
#endif
    }
};
QTEST_GUILESS_MAIN(TestMediaLibrary)
#include "test_media_library.moc"
