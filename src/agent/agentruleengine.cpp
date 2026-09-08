/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "agentruleengine.h"

#include <QRegularExpression>
#include <QtMath>

#include <algorithm>

namespace {

double capturedSeconds(const QRegularExpressionMatch &match, int capture)
{
    bool ok = false;
    const double result = match.captured(capture).toDouble(&ok);
    return ok ? result : -1.0;
}

AgentEditOperation advisory(const QString &id, const QString &title, const QString &reason)
{
    AgentEditOperation operation;
    operation.id = id;
    operation.title = title;
    operation.reason = reason;
    operation.type = AgentOperationType::Advisory;
    operation.risk = AgentOperationRisk::Medium;
    operation.accepted = false;
    operation.executable = false;
    return operation;
}

} // namespace

AgentEditPlan AgentRuleEngine::analyze(const QString &prompt,
                                       double sourceDurationSeconds,
                                       double targetDurationSeconds)
{
    AgentEditPlan plan;
    plan.sourceDurationSeconds = qMax(0.0, sourceDurationSeconds);
    plan.estimatedDurationSeconds = plan.sourceDurationSeconds;

    const QString normalized = prompt.simplified();
    if (normalized.isEmpty()) {
        plan.error = QStringLiteral("请输入剪辑要求。");
        return plan;
    }

    if (normalized.contains(QStringLiteral("撤销"))) {
        plan.summary = QStringLiteral("撤销上一次时间线修改");
        plan.requestUndo = true;
        return plan;
    }

    if (sourceDurationSeconds <= 0.0) {
        plan.error = QStringLiteral("当前时间线为空，请先把素材拖入时间线。");
        return plan;
    }

    int operationIndex = 1;
    const QRegularExpression keepExpression(
        QStringLiteral("保留\\s*(\\d+(?:\\.\\d+)?)\\s*秒?\\s*(?:到|至|-)\\s*(\\d+(?:\\.\\d+)?)\\s*秒?"));
    const auto keepMatch = keepExpression.match(normalized);
    if (keepMatch.hasMatch()) {
        const double start = capturedSeconds(keepMatch, 1);
        const double end = capturedSeconds(keepMatch, 2);
        AgentEditOperation operation;
        operation.id = nextId(operationIndex++);
        operation.title = QStringLiteral("仅保留指定时间范围");
        operation.reason = QStringLiteral("来自本地规则解析，应用前可在播放器试听起点。");
        operation.type = AgentOperationType::KeepRange;
        operation.risk = AgentOperationRisk::Medium;
        operation.startSeconds = start;
        operation.endSeconds = end;
        operation.executable = true;
        plan.operations << operation;
        plan.estimatedDurationSeconds = qMax(0.0, end - start);
    } else {
        const QRegularExpression deleteStartExpression(
            QStringLiteral("删除\\s*开头\\s*(\\d+(?:\\.\\d+)?)\\s*秒"));
        const auto deleteStartMatch = deleteStartExpression.match(normalized);
        if (deleteStartMatch.hasMatch()) {
            const double seconds = capturedSeconds(deleteStartMatch, 1);
            AgentEditOperation operation;
            operation.id = nextId(operationIndex++);
            operation.title = QStringLiteral("删除开头 %1 秒").arg(seconds, 0, 'f', 1);
            operation.reason = QStringLiteral("用户明确指定删除开头内容。");
            operation.type = AgentOperationType::RemoveRange;
            operation.startSeconds = 0.0;
            operation.endSeconds = seconds;
            operation.executable = true;
            plan.operations << operation;
            plan.estimatedDurationSeconds -= seconds;
        }

        const QRegularExpression deleteEndExpression(
            QStringLiteral("删除\\s*结尾\\s*(\\d+(?:\\.\\d+)?)\\s*秒"));
        const auto deleteEndMatch = deleteEndExpression.match(normalized);
        if (deleteEndMatch.hasMatch()) {
            const double seconds = capturedSeconds(deleteEndMatch, 1);
            AgentEditOperation operation;
            operation.id = nextId(operationIndex++);
            operation.title = QStringLiteral("删除结尾 %1 秒").arg(seconds, 0, 'f', 1);
            operation.reason = QStringLiteral("用户明确指定删除结尾内容。");
            operation.type = AgentOperationType::RemoveRange;
            operation.startSeconds = sourceDurationSeconds - seconds;
            operation.endSeconds = sourceDurationSeconds;
            operation.executable = true;
            plan.operations << operation;
            plan.estimatedDurationSeconds -= seconds;
        }
    }

    double requestedTarget = targetDurationSeconds;
    const QRegularExpression targetExpression(
        QStringLiteral("(?:剪成|压缩到|控制在)\\s*(\\d+(?:\\.\\d+)?)\\s*分钟"));
    const auto targetMatch = targetExpression.match(normalized);
    if (targetMatch.hasMatch())
        requestedTarget = capturedSeconds(targetMatch, 1) * 60.0;

    const bool asksForSemanticEdit = normalized.contains(QStringLiteral("剪成"))
                                     || normalized.contains(QStringLiteral("压缩"))
                                     || normalized.contains(QStringLiteral("精彩"))
                                     || normalized.contains(QStringLiteral("重复"))
                                     || normalized.contains(QStringLiteral("停顿"));
    if (asksForSemanticEdit && plan.operations.isEmpty()) {
        plan.requiresModel = true;
        plan.operations << advisory(nextId(operationIndex++),
                                    QStringLiteral("建立长视频文字索引"),
                                    QStringLiteral("需要导入 Whisper 模型后进行离线转录和分章。"));
        plan.operations << advisory(
            nextId(operationIndex++),
            QStringLiteral("生成 %1 的忠实粗剪").arg(
                QString::number(qMax(1.0, requestedTarget) / 60.0, 'f', 1) + QStringLiteral(" 分钟")),
            QStringLiteral("规则模式不会假装理解语义；导入本地规划模型后才会选择内容。"));
        plan.operations << advisory(nextId(operationIndex++),
                                    QStringLiteral("人工确认后写入时间线"),
                                    QStringLiteral("所有模型建议都必须先预览，并作为一次操作应用和撤销。"));
        plan.estimatedDurationSeconds = qMin(sourceDurationSeconds, requestedTarget);
    }

    if (plan.operations.isEmpty()) {
        plan.operations << advisory(
            nextId(operationIndex),
            QStringLiteral("规则模式未识别此要求"),
            QStringLiteral("0.3.0 可直接执行规则；配置 llama.cpp 后可生成结构化语义剪辑方案。"));
    }

    plan.estimatedDurationSeconds = qBound(0.0,
                                           plan.estimatedDurationSeconds,
                                           plan.sourceDurationSeconds);
    QString validationError;
    if (!validate(plan, &validationError)) {
        plan.error = validationError;
        plan.operations.clear();
        return plan;
    }

    const int executableCount = std::count_if(plan.operations.cbegin(),
                                               plan.operations.cend(),
                                               [](const AgentEditOperation &operation) {
                                                   return operation.executable;
                                               });
    if (executableCount > 0) {
        plan.summary = QStringLiteral("已生成 %1 项可执行的本地规则建议").arg(executableCount);
    } else if (plan.requiresModel) {
        plan.summary = QStringLiteral("已生成离线 Agent 分析任务，等待本地模型");
    } else {
        plan.summary = QStringLiteral("当前要求需要调整后再分析");
    }
    return plan;
}

bool AgentRuleEngine::validate(const AgentEditPlan &plan, QString *error)
{
    for (const auto &operation : plan.operations) {
        if (!operation.executable)
            continue;
        if (!qIsFinite(operation.startSeconds) || !qIsFinite(operation.endSeconds)) {
            if (error)
                *error = QStringLiteral("时间范围不是有效数字，未修改时间线。");
            return false;
        }
        if (operation.startSeconds < 0.0 || operation.endSeconds > plan.sourceDurationSeconds) {
            if (error)
                *error = QStringLiteral("时间范围超出当前时间线，未修改时间线。");
            return false;
        }
        if (operation.startSeconds >= operation.endSeconds) {
            if (error)
                *error = QStringLiteral("入点必须早于出点，未修改时间线。");
            return false;
        }
    }
    return true;
}

QString AgentRuleEngine::nextId(int index)
{
    return QStringLiteral("rule-%1").arg(index, 3, 10, QLatin1Char('0'));
}
