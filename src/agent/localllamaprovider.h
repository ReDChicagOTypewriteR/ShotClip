/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LOCALLLAMAPROVIDER_H
#define LOCALLLAMAPROVIDER_H

#include "agenttypes.h"

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>

#include <memory>

class LocalLlamaProvider : public QObject
{
    Q_OBJECT

public:
    explicit LocalLlamaProvider(QObject *parent = nullptr);
    bool isConfigured(QString *error = nullptr) const;
    bool isRunning() const;
    QString modelDisplayName() const;
    void analyze(const QString &request,
                 const QString &editingMode,
                 int targetDurationSeconds,
                 const QJsonObject &timelineContext);
    void cancel();

    static QString executablePath();
    static QString modelPath();
    static int contextSize();
    static int threadCount();
    static int gpuLayers();
    static void setExecutablePath(const QString &path);
    static void setModelPath(const QString &path);
    static void setContextSize(int value);
    static void setThreadCount(int value);
    static void setGpuLayers(int value);

signals:
    void planReady(const AgentEditPlan &plan);
    void failed(const QString &message);
    void runningChanged(bool running);
    void diagnostic(const QString &message);

private:
    void finishRun();
    void resetRun();

    QProcess *m_process;
    QTimer m_timeout;
    std::unique_ptr<QTemporaryFile> m_promptFile;
    QByteArray m_stdout;
    QByteArray m_stderr;
    double m_timelineDuration{0.0};
    QString m_contextId;
    bool m_cancelled{false};
};

#endif
