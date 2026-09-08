/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "localllamaprovider.h"

#include "agenttoolregistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonValue>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>

namespace {

const auto kExecutableKey = QStringLiteral("shotclip/llamaExecutable");
const auto kModelKey = QStringLiteral("shotclip/plannerModel");
const auto kContextKey = QStringLiteral("shotclip/llamaContextSize");
const auto kThreadsKey = QStringLiteral("shotclip/llamaThreads");
const auto kGpuLayersKey = QStringLiteral("shotclip/llamaGpuLayers");
constexpr qint64 kMaximumCapturedOutput = 4 * 1024 * 1024;

} // namespace

LocalLlamaProvider::LocalLlamaProvider(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(10 * 60 * 1000);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        emit diagnostic(tr("llama.cpp analysis timed out"));
        cancel();
        emit failed(tr("本地模型分析超过 10 分钟，已停止。可以换用更小的 GGUF 模型或减小上下文。"));
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_stdout += m_process->readAllStandardOutput();
        if (m_stdout.size() > kMaximumCapturedOutput)
            m_stdout = m_stdout.right(kMaximumCapturedOutput);
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_stderr += m_process->readAllStandardError();
        if (m_stderr.size() > kMaximumCapturedOutput)
            m_stderr = m_stderr.right(kMaximumCapturedOutput);
    });
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int, QProcess::ExitStatus) { finishRun(); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_timeout.stop();
            emit failed(tr("无法启动 llama.cpp。请确认选择的是 llama-cli 可执行文件。"));
            resetRun();
        }
    });
}

bool LocalLlamaProvider::isConfigured(QString *error) const
{
    if (!QFileInfo(executablePath()).isExecutable()) {
        if (error)
            *error = tr("未找到可执行的 llama-cli。请在本地模型设置中选择。");
        return false;
    }
    if (!QFileInfo(modelPath()).isFile()) {
        if (error)
            *error = tr("未找到 GGUF 规划模型。请在本地模型设置中导入。");
        return false;
    }
    return true;
}

bool LocalLlamaProvider::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

QString LocalLlamaProvider::modelDisplayName() const
{
    return QFileInfo(modelPath()).fileName();
}

void LocalLlamaProvider::analyze(const QString &request,
                                 const QString &editingMode,
                                 int targetDurationSeconds,
                                 const QJsonObject &timelineContext)
{
    if (isRunning()) {
        emit failed(tr("本地模型正在分析。可以先停止当前任务。"));
        return;
    }
    QString configurationError;
    if (!isConfigured(&configurationError)) {
        emit failed(configurationError);
        return;
    }
    m_contextId = timelineContext.value(QStringLiteral("context_id")).toString();
    m_timelineDuration = timelineContext.value(QStringLiteral("duration_seconds")).toDouble();
    if (m_contextId.isEmpty() || m_timelineDuration <= 0.0) {
        emit failed(tr("当前时间线上下文无效，无法交给模型分析。"));
        return;
    }

    const QString tempDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_promptFile.reset(new QTemporaryFile(tempDirectory + QStringLiteral("/shotclip-agent-XXXXXX.txt")));
    m_promptFile->setAutoRemove(true);
    if (!m_promptFile->open()) {
        emit failed(tr("无法创建本地模型提示词临时文件。"));
        resetRun();
        return;
    }
    const QString prompt = AgentToolRegistry::buildPrompt(request,
                                                          editingMode,
                                                          targetDurationSeconds,
                                                          timelineContext);
    if (m_promptFile->write(prompt.toUtf8()) < 0 || !m_promptFile->flush()) {
        emit failed(tr("无法写入本地模型提示词临时文件。"));
        resetRun();
        return;
    }
    m_promptFile->close();

    m_stdout.clear();
    m_stderr.clear();
    m_cancelled = false;
    QStringList arguments;
    arguments << QStringLiteral("-m") << modelPath()
              << QStringLiteral("-f") << m_promptFile->fileName()
              << QStringLiteral("-n") << QStringLiteral("2048")
              << QStringLiteral("-c") << QString::number(contextSize())
              << QStringLiteral("-t") << QString::number(threadCount())
              << QStringLiteral("-ngl") << QString::number(gpuLayers())
              << QStringLiteral("--temp") << QStringLiteral("0.2")
              << QStringLiteral("--offline")
              << QStringLiteral("--single-turn")
              << QStringLiteral("--no-display-prompt")
              << QStringLiteral("--no-show-timings")
              << QStringLiteral("--simple-io")
              << QStringLiteral("-j") << AgentToolRegistry::jsonSchema();
    emit diagnostic(tr("启动本地 llama.cpp；上下文 %1，线程 %2，GPU 层 %3。")
                        .arg(contextSize())
                        .arg(threadCount())
                        .arg(gpuLayers()));
    emit runningChanged(true);
    m_timeout.start();
    m_process->start(executablePath(), arguments, QIODevice::ReadOnly);
}

void LocalLlamaProvider::cancel()
{
    if (!isRunning())
        return;
    m_cancelled = true;
    m_timeout.stop();
    m_process->terminate();
    QTimer::singleShot(1500, m_process, [process = m_process]() {
        if (process->state() != QProcess::NotRunning)
            process->kill();
    });
}

void LocalLlamaProvider::finishRun()
{
    m_timeout.stop();
    m_stdout += m_process->readAllStandardOutput();
    m_stderr += m_process->readAllStandardError();
    if (m_cancelled) {
        emit diagnostic(tr("本地模型分析已由用户停止。"));
        resetRun();
        return;
    }
    if (m_process->exitStatus() != QProcess::NormalExit || m_process->exitCode() != 0) {
        emit diagnostic(tr("llama.cpp 退出代码：%1").arg(m_process->exitCode()));
        emit failed(tr("llama.cpp 执行失败。请确认使用较新的 llama-cli、有效 GGUF 模型，"
                       "并检查上下文/GPU 层设置。"));
        resetRun();
        return;
    }

    const AgentEditPlan plan = AgentToolRegistry::parsePlan(QString::fromUtf8(m_stdout),
                                                            m_timelineDuration,
                                                            m_contextId);
    if (!plan.error.isEmpty())
        emit failed(plan.error);
    else
        emit planReady(plan);
    resetRun();
}

void LocalLlamaProvider::resetRun()
{
    m_timeout.stop();
    m_promptFile.reset();
    m_stdout.clear();
    m_stderr.clear();
    m_contextId.clear();
    m_timelineDuration = 0.0;
    m_cancelled = false;
    emit runningChanged(false);
}

QString LocalLlamaProvider::executablePath()
{
    const QString configured = QSettings().value(kExecutableKey).toString().trimmed();
    if (!configured.isEmpty())
        return configured;
    QDir applicationDirectory(QCoreApplication::applicationDirPath());
#if defined(Q_OS_WIN)
    return applicationDirectory.absoluteFilePath(QStringLiteral("llama-cli.exe"));
#else
    return applicationDirectory.absoluteFilePath(QStringLiteral("llama-cli"));
#endif
}

QString LocalLlamaProvider::modelPath()
{
    return QSettings().value(kModelKey).toString();
}

int LocalLlamaProvider::contextSize()
{
    return qBound(2048, QSettings().value(kContextKey, 8192).toInt(), 131072);
}

int LocalLlamaProvider::threadCount()
{
    const int ideal = qMax(1, QThread::idealThreadCount() / 2);
    return qBound(1, QSettings().value(kThreadsKey, ideal).toInt(), 128);
}

int LocalLlamaProvider::gpuLayers()
{
    return qBound(0, QSettings().value(kGpuLayersKey, 999).toInt(), 999);
}

void LocalLlamaProvider::setExecutablePath(const QString &path)
{
    if (path.trimmed().isEmpty())
        QSettings().remove(kExecutableKey);
    else
        QSettings().setValue(kExecutableKey, path.trimmed());
}

void LocalLlamaProvider::setModelPath(const QString &path)
{
    QSettings().setValue(kModelKey, path);
}

void LocalLlamaProvider::setContextSize(int value)
{
    QSettings().setValue(kContextKey, qBound(2048, value, 131072));
}

void LocalLlamaProvider::setThreadCount(int value)
{
    QSettings().setValue(kThreadsKey, qBound(1, value, 128));
}

void LocalLlamaProvider::setGpuLayers(int value)
{
    QSettings().setValue(kGpuLayersKey, qBound(0, value, 999));
}
