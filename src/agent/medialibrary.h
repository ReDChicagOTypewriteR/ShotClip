// Copyright (c) 2026 AleXJokeR. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MEDIALIBRARY_H
#define MEDIALIBRARY_H
#include <QObject>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <QLockFile>
#include <memory>

// A local, resumable source-media index. Never mutates an editing timeline.
class MediaLibrary : public QObject
{
    Q_OBJECT
public:
    explicit MediaLibrary(const QString &directory, QObject *parent = nullptr);
    ~MediaLibrary() override;
    QJsonArray items() const { return m_items; }
    QString error() const { return m_error; }
    bool running() const { return m_running; }
    void addFiles(const QStringList &paths);
    void start(const QString &ffprobe, const QString &ffmpeg, const QString &whisper,
               const QString &model, bool gpu);
    void pause();
    void retry();
    QJsonArray search(const QString &query, int limit = 500) const;
    static QString fingerprint(const QString &path);
    static QJsonArray parseSrt(const QString &text, double offset, double length, QString *error);
signals:
    void changed();
    void progress(const QString &message);
private:
    bool save();
    void next();
    void extract();
    void launch(const QString &program, const QStringList &args);
    void finished(int code, QProcess::ExitStatus status);
    void fail(const QString &message);
    void update(const QJsonObject &item);
    QString m_directory, m_error, m_probe, m_ffmpeg, m_whisper, m_model, m_modelKey;
    QJsonArray m_items;
    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_output, m_diagnostics;
    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<QLockFile> m_lock;
    int m_current{-1};
    enum Stage { Probe, Extract, Transcribe } m_stage{Probe};
    bool m_running{false}, m_gpu{false}, m_cpuRetry{false};
};
#endif
