// Copyright (c) 2026 AleXJokeR. SPDX-License-Identifier: GPL-3.0-or-later
#include "medialibrary.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <cmath>

MediaLibrary::MediaLibrary(const QString &directory, QObject *parent)
    : QObject(parent), m_directory(directory)
{
    QDir().mkpath(directory);
    m_lock = std::make_unique<QLockFile>(directory + "/index.lock");
    if (!m_lock->tryLock()) {
        m_error = tr("素材索引正在被另一个 ShotClip 使用，请关闭另一个窗口后重启");
        return;
    }
    QFile file(directory + "/index.json");
    if (file.exists()) {
        QJsonParseError error;
        if (!file.open(QIODevice::ReadOnly)) m_error = tr("无法读取素材索引");
        else {
            auto doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || doc.object()["version"].toInt() != 1
                || !doc.object()["items"].isArray())
                m_error = tr("素材索引损坏或版本不兼容；原文件已保留，请备份后修复 index.json");
            else {
                m_items = doc.object()["items"].toArray();
                QSet<QString> ids;
                for (auto value : m_items) {
                    const auto item = value.toObject();
                    const auto id = item["id"].toString();
                    const auto state = item["state"].toString();
                    const double completed = item["completed"].toDouble(-1);
                    const double duration = item["duration"].toDouble(0);
                    bool valid = !id.isEmpty() && !ids.contains(id) && !item["path"].toString().isEmpty()
                        && !item["fingerprint"].toString().isEmpty() && item["segments"].isArray()
                        && (state == "pending" || state == "ready" || state == "failed" || state == "no_audio")
                        && std::isfinite(completed) && std::isfinite(duration)
                        && completed >= 0 && duration >= completed
                        && (state != "ready" || (duration > 0 && completed == duration));
                    ids.insert(id);
                    double previous = 0;
                    for (auto segmentValue : item["segments"].toArray()) {
                        const auto segment = segmentValue.toObject();
                        const double start = segment["start"].toDouble(-1);
                        const double end = segment["end"].toDouble(-1);
                        valid = valid && std::isfinite(start) && std::isfinite(end) && start >= previous
                            && end > start && end <= completed && !segment["text"].toString().isEmpty()
                            && !segment["id"].toString().isEmpty();
                        previous = end;
                    }
                    if (!valid) {
                        m_error = tr("素材索引包含无效记录；原文件已保留，请备份后修复 index.json");
                        m_items = {}; break;
                    }
                }
            }
        }
    }
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        fail(tr("分析超时；可重试，已完成分段保留"));
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
        if (m_output.size() > 8 * 1024 * 1024) fail(tr("分析输出超出限制"));
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        auto text = QString::fromUtf8(m_process.readAllStandardError());
        m_diagnostics = (m_diagnostics + text.toUtf8()).right(8192);
        auto match = QRegularExpression("progress =\\s*(\\d+)%").match(text);
        if (match.hasMatch()) emit progress(tr("当前分段转录 %1%").arg(match.captured(1)));
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &MediaLibrary::finished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) fail(tr("无法启动本地运行引擎：%1").arg(m_process.errorString()));
    });
}
MediaLibrary::~MediaLibrary() { pause(); }

QString MediaLibrary::fingerprint(const QString &path)
{
    QFileInfo info(path);
    if (!info.isFile()) return {};
    // File identity and version, without reading multi-GB sources on the UI thread.
    auto bytes = (info.canonicalFilePath() + "|" + QString::number(info.size()) + "|"
                  + QString::number(info.lastModified().toMSecsSinceEpoch())).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
bool MediaLibrary::save()
{
    if (!m_error.isEmpty()) return false;
    QSaveFile file(m_directory + "/index.json");
    auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"items", m_items}}).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_error = tr("无法保存分析缓存：%1").arg(file.errorString());
        m_running = false;
        emit progress(m_error);
        return false;
    }
    return true;
}
void MediaLibrary::addFiles(const QStringList &paths)
{
    if (!m_error.isEmpty() || m_running) return;
    QSet<QString> fingerprints;
    for (auto value : m_items) fingerprints.insert(value.toObject()["fingerprint"].toString());
    for (const auto &path : paths) {
        QFileInfo info(path);
        auto key = fingerprint(path);
        if (key.isEmpty()) continue;
        if (fingerprints.contains(key)) continue;
        fingerprints.insert(key);
        m_items.append(QJsonObject{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                  {"path", info.canonicalFilePath()}, {"name", info.fileName()},
                                  {"fingerprint", key}, {"state", "pending"}, {"completed", 0},
                                  {"segments", QJsonArray()}});
    }
    save(); emit changed();
}
void MediaLibrary::update(const QJsonObject &item) { m_items[m_current] = item; save(); emit changed(); }
void MediaLibrary::start(const QString &probe, const QString &ffmpeg, const QString &whisper,
                         const QString &model, bool gpu)
{
    if (m_running || !m_error.isEmpty()) { emit progress(m_error); return; }
    for (const auto &exe : {probe, ffmpeg, whisper}) if (!QFileInfo(exe).isExecutable()) {
        emit progress(tr("未找到运行引擎：%1").arg(exe)); return;
    }
    if (!QFileInfo(model).isFile()) { emit progress(tr("请先在 Agent 中导入 Whisper 模型")); return; }
    m_probe = probe; m_ffmpeg = ffmpeg; m_whisper = whisper; m_model = model;
    m_modelKey = fingerprint(model); m_gpu = gpu; m_running = true;
    emit changed();
    for (int i = 0; i < m_items.size(); ++i) {
        auto item = m_items[i].toObject();
        if (item["model"].toString() != m_modelKey && item["state"] != "failed") {
            item["state"] = "pending"; item["completed"] = 0; item["segments"] = QJsonArray();
            m_items[i] = item;
        }
    }
    next();
}
void MediaLibrary::pause()
{
    m_running = false; m_timeout.stop();
    if (m_process.state() != QProcess::NotRunning) { m_process.kill(); m_process.waitForFinished(3000); }
    m_temp.reset(); emit changed();
}
void MediaLibrary::retry()
{
    if (m_running || !m_error.isEmpty()) return;
    for (int i = 0; i < m_items.size(); ++i) {
        auto item = m_items[i].toObject();
        if (item["state"] == "failed") { item["state"] = "pending"; item.remove("error"); m_items[i] = item; }
    }
    save(); emit changed();
}
void MediaLibrary::next()
{
    if (!m_running) return;
    m_current = -1;
    for (int i = 0; i < m_items.size(); ++i) {
        auto state = m_items[i].toObject()["state"].toString();
        if (state == "pending") { m_current = i; break; }
    }
    if (m_current < 0) { m_running = false; m_temp.reset(); emit changed(); emit progress(tr("队列处理结束；失败项可重试")); return; }
    auto item = m_items[m_current].toObject();
    if (fingerprint(item["path"].toString()) != item["fingerprint"].toString()) {
        fail(tr("素材丢失或已改变，请重新导入该文件")); return;
    }
    m_cpuRetry = false;
    m_temp = std::make_unique<QTemporaryDir>();
    if (!m_temp->isValid()) { fail(tr("无法创建音频临时目录")); return; }
    emit progress(tr("读取素材：%1").arg(item["name"].toString()));
    m_stage = Probe;
    launch(m_probe, {"-v", "error", "-show_format", "-show_streams", "-of", "json", item["path"].toString()});
}
void MediaLibrary::launch(const QString &program, const QStringList &args)
{
    if (!m_running) return;
    m_output.clear(); m_diagnostics.clear(); m_timeout.start(30 * 60 * 1000); m_process.start(program, args);
}
void MediaLibrary::extract()
{
    if (!m_running) return;
    auto item = m_items[m_current].toObject();
    const double offset = item["completed"].toDouble();
    emit progress(tr("%1 · 已分析 %2 / %3 秒").arg(item["name"].toString())
                  .arg(offset, 0, 'f', 0).arg(item["duration"].toDouble(), 0, 'f', 0));
    m_stage = Extract;
    launch(m_ffmpeg, {"-nostdin", "-v", "error", "-y", "-ss", QString::number(offset, 'f', 3),
                     "-i", item["path"].toString(), "-t", "300", "-vn", "-ac", "1", "-ar", "16000",
                     "-c:a", "pcm_s16le", m_temp->filePath("chunk.wav")});
}
void MediaLibrary::fail(const QString &message)
{
    if (!m_running) return;
    m_timeout.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.blockSignals(true);
        m_process.kill(); m_process.waitForFinished(3000);
        m_process.blockSignals(false);
    }
    auto item = m_items[m_current].toObject(); item["state"] = "failed"; item["error"] = message;
    update(item); emit progress(message);
    QTimer::singleShot(0, this, &MediaLibrary::next);
}
void MediaLibrary::finished(int code, QProcess::ExitStatus status)
{
    m_timeout.stop(); if (!m_running) return;
    if (code != 0 || status != QProcess::NormalExit) {
        if (m_stage == Transcribe && m_gpu && !m_cpuRetry) {
            m_cpuRetry = true; m_stage = Extract; finished(0, QProcess::NormalExit); return;
        }
        m_diagnostics = (m_diagnostics + m_process.readAllStandardError()).right(8192);
        fail(tr("本地分析失败（退出码 %1）：%2").arg(code)
                 .arg(QString::fromUtf8(m_diagnostics).right(2000))); return;
    }
    m_output += m_process.readAllStandardOutput();
    auto item = m_items[m_current].toObject();
    if (m_stage == Probe) {
        auto doc = QJsonDocument::fromJson(m_output).object();
        const double duration = doc["format"].toObject()["duration"].toVariant().toDouble();
        bool audio = false;
        for (auto stream : doc["streams"].toArray()) {
            auto s = stream.toObject();
            if (s["codec_type"] == "audio") audio = true;
            if (s["codec_type"] == "video") { item["width"] = s["width"]; item["height"] = s["height"]; }
        }
        if (!std::isfinite(duration) || duration <= 0) { fail(tr("素材时长无效")); return; }
        item["duration"] = duration; item["model"] = m_modelKey; item["has_audio"] = audio;
        if (!audio) { item["state"] = "no_audio"; update(item); next(); return; }
        update(item); extract();
    } else if (m_stage == Extract) {
        m_stage = Transcribe;
        // Never mistake output left by a previous chunk or failed GPU run for this result.
        const auto output = m_temp->filePath("transcript.srt");
        if (QFile::exists(output) && !QFile::remove(output)) {
            fail(tr("无法清理上一分段临时字幕")); return;
        }
        QStringList args{"-m", m_model, "-f", m_temp->filePath("chunk.wav"), "-l", "auto",
                         "-osrt", "-of", m_temp->filePath("transcript"), "-pp", "-t", "8"};
        if (!m_gpu || m_cpuRetry) args << "-ng";
        launch(m_whisper, args);
    } else {
        QFile file(m_temp->filePath("transcript.srt"));
        if (!file.open(QIODevice::ReadOnly)) { fail(tr("转录未生成字幕文件")); return; }
        const double offset = item["completed"].toDouble();
        const double length = qMin(300.0, item["duration"].toDouble() - offset);
        QString error;
        auto segments = parseSrt(QString::fromUtf8(file.readAll()), offset, length, &error);
        if (!error.isEmpty()) { fail(error); return; }
        auto all = item["segments"].toArray();
        for (auto value : segments) {
            auto segment = value.toObject();
            segment["id"] = item["id"].toString() + ":" + QString::number(all.size()); all.append(segment);
        }
        item["segments"] = all; item["completed"] = offset + length;
        if (offset + length >= item["duration"].toDouble() - 0.001) item["state"] = "ready";
        update(item);
        if (item["state"] == "ready") next(); else extract();
    }
}
QJsonArray MediaLibrary::parseSrt(const QString &text, double offset, double length, QString *error)
{
    QJsonArray result; error->clear();
    if (!std::isfinite(offset) || !std::isfinite(length) || offset < 0 || length <= 0) {
        *error = tr("转录分段范围无效"); return {};
    }
    auto normalized = text; normalized.replace("\r", ""); normalized.remove(QChar(0xfeff));
    if (normalized.trimmed().isEmpty()) return result;
    const QRegularExpression time("(\\d+):(\\d{2}):(\\d{2})[,.](\\d{3})\\s*-->\\s*(\\d+):(\\d{2}):(\\d{2})[,.](\\d{3})");
    double previous = 0;
    for (auto block : normalized.trimmed().split(QRegularExpression("\\n\\s*\\n"))) {
        auto match = time.match(block);
        if (!match.hasMatch()) { *error = tr("字幕时间格式无效"); return {}; }
        for (int i : {2, 3, 6, 7}) if (match.captured(i).toInt() > 59) {
            *error = tr("字幕时间格式无效"); return {};
        }
        auto seconds = [&match](int i) { return match.captured(i).toDouble() * 3600
            + match.captured(i + 1).toDouble() * 60 + match.captured(i + 2).toDouble()
            + match.captured(i + 3).toDouble() / 1000; };
        double start = seconds(1), end = seconds(5);
        QString content = block.mid(match.capturedEnd()).trimmed();
        if (!std::isfinite(start) || !std::isfinite(end) || start < previous || start >= end
            || start >= length || end > length + 1 || content.isEmpty()) {
            *error = tr("字幕范围越界或重叠"); return {};
        }
        end = qMin(end, length); previous = end;
        result.append(QJsonObject{{"start", offset + start}, {"end", offset + end}, {"text", content}});
    }
    return result;
}
QJsonArray MediaLibrary::search(const QString &query, int limit) const
{
    QJsonArray result;
    if (query.trimmed().isEmpty()) return result;
    for (auto value : m_items) {
        auto item = value.toObject();
        for (auto segment : item["segments"].toArray()) {
            auto hit = segment.toObject();
            if (!hit["text"].toString().contains(query.trimmed(), Qt::CaseInsensitive)) continue;
            hit["path"] = item["path"]; hit["name"] = item["name"]; hit["fingerprint"] = item["fingerprint"];
            result.append(hit); if (result.size() >= limit) return result;
        }
    }
    return result;
}
