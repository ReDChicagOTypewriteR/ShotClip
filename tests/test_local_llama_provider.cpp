/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "agent/localllamaprovider.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestLocalLlamaProvider : public QObject
{
    Q_OBJECT

private slots:
    void runsLocalProcessAndParsesToolPlan()
    {
#ifdef Q_OS_WIN
        QSKIP("The fake POSIX llama-cli fixture is tested on Unix hosts.");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString executable = directory.filePath(QStringLiteral("llama-cli"));
        QFile script(executable);
        QVERIFY(script.open(QIODevice::WriteOnly));
        const QByteArray body =
            "#!/bin/sh\n"
            "printf '%s' '{\"version\":\"1.0\",\"context_id\":\"ctx-provider-1\","
            "\"summary\":\"local plan\",\"tool_calls\":[{\"id\":\"call-1\","
            "\"name\":\"timeline.ripple_delete_range\",\"arguments\":{" 
            "\"start_seconds\":0,\"end_seconds\":2},\"reason\":\"silent start\"}]}'\n";
        QCOMPARE(script.write(body), body.size());
        script.close();
        QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));

        const QString model = directory.filePath(QStringLiteral("planner.gguf"));
        QFile modelFile(model);
        QVERIFY(modelFile.open(QIODevice::WriteOnly));
        QVERIFY(modelFile.write("GGUF-test") > 0);
        modelFile.close();

        LocalLlamaProvider::setExecutablePath(executable);
        LocalLlamaProvider::setModelPath(model);
        LocalLlamaProvider provider;
        QSignalSpy readySpy(&provider, &LocalLlamaProvider::planReady);
        QSignalSpy failedSpy(&provider, &LocalLlamaProvider::failed);
        QJsonObject context{{QStringLiteral("context_id"), QStringLiteral("ctx-provider-1")},
                            {QStringLiteral("duration_seconds"), 10.0}};
        provider.analyze(QStringLiteral("remove silence"),
                         QStringLiteral("faithful"),
                         8,
                         context);
        QVERIFY(readySpy.wait(5000));
        QCOMPARE(failedSpy.count(), 0);
        const auto plan = qvariant_cast<AgentEditPlan>(readySpy.takeFirst().at(0));
        QCOMPARE(plan.operations.size(), 1);
        QCOMPARE(plan.operations.first().startSeconds, 0.0);
        QCOMPARE(plan.operations.first().endSeconds, 2.0);
#endif
    }
};

// QProcess delivery depends on a QCoreApplication event dispatcher.
QTEST_GUILESS_MAIN(TestLocalLlamaProvider)

#include "test_local_llama_provider.moc"
