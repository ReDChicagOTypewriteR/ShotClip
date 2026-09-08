/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "agent/agentruleengine.h"

#include <QtTest>

class TestAgentRuleEngine : public QObject
{
    Q_OBJECT

private slots:
    void deletesStartAndEnd()
    {
        const auto plan = AgentRuleEngine::analyze(
            QStringLiteral("删除开头 5 秒，并删除结尾 3 秒"), 100.0, 60.0);
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.operations.size(), 2);
        QCOMPARE(plan.operations.at(0).type, AgentOperationType::RemoveRange);
        QCOMPARE(plan.operations.at(0).startSeconds, 0.0);
        QCOMPARE(plan.operations.at(0).endSeconds, 5.0);
        QCOMPARE(plan.operations.at(1).startSeconds, 97.0);
        QCOMPARE(plan.operations.at(1).endSeconds, 100.0);
        QCOMPARE(plan.estimatedDurationSeconds, 92.0);
    }

    void keepsExplicitRange()
    {
        const auto plan = AgentRuleEngine::analyze(
            QStringLiteral("保留 10 秒到 30 秒"), 80.0, 600.0);
        QVERIFY(plan.error.isEmpty());
        QCOMPARE(plan.operations.size(), 1);
        QCOMPARE(plan.operations.first().type, AgentOperationType::KeepRange);
        QCOMPARE(plan.operations.first().startSeconds, 10.0);
        QCOMPARE(plan.operations.first().endSeconds, 30.0);
        QCOMPARE(plan.estimatedDurationSeconds, 20.0);
    }

    void rejectsOutOfBoundsRange()
    {
        const auto plan = AgentRuleEngine::analyze(
            QStringLiteral("保留 10 秒到 130 秒"), 80.0, 600.0);
        QVERIFY(!plan.error.isEmpty());
        QVERIFY(plan.operations.isEmpty());
    }

    void requestsUndo()
    {
        const auto plan = AgentRuleEngine::analyze(QStringLiteral("撤销上一次修改"), 80.0, 600.0);
        QVERIFY(plan.requestUndo);
        QVERIFY(plan.operations.isEmpty());
    }

    void defersSemanticEditUntilModelIsAvailable()
    {
        const auto plan = AgentRuleEngine::analyze(
            QStringLiteral("把演讲剪成 10 分钟，删除重复内容"), 3600.0, 600.0);
        QVERIFY(plan.error.isEmpty());
        QVERIFY(plan.requiresModel);
        QCOMPARE(plan.operations.size(), 3);
        for (const auto &operation : plan.operations)
            QVERIFY(!operation.executable);
        QCOMPARE(plan.estimatedDurationSeconds, 600.0);
    }

    void requiresTimelineForExecutableRule()
    {
        const auto plan = AgentRuleEngine::analyze(QStringLiteral("删除开头 5 秒"), 0.0, 600.0);
        QVERIFY(!plan.error.isEmpty());
        QVERIFY(plan.operations.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestAgentRuleEngine)

#include "test_agent_rule_engine.moc"
