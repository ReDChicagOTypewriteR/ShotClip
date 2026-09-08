/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "agent/agenttoolregistry.h"

#include <QtTest>

class TestAgentToolRegistry : public QObject
{
    Q_OBJECT

private slots:
    void acceptsStrictFencedJson()
    {
        const QString output = QStringLiteral(
            "```json\n{\"version\":\"1.0\",\"context_id\":\"ctx-12345678\","
            "\"summary\":\"删除无关开场\",\"tool_calls\":[{\"id\":\"call-1\","
            "\"name\":\"timeline.ripple_delete_range\",\"arguments\":{" 
            "\"start_seconds\":0,\"end_seconds\":5},\"reason\":\"开场无讲话\"}]}\n```");
        const auto plan = AgentToolRegistry::parsePlan(output, 100.0, QStringLiteral("ctx-12345678"));
        QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error));
        QVERIFY(plan.modelGenerated);
        QCOMPARE(plan.operations.size(), 1);
        QCOMPARE(plan.operations.first().type, AgentOperationType::RemoveRange);
        QCOMPARE(plan.operations.first().toolName,
                 QStringLiteral("timeline.ripple_delete_range"));
        QVERIFY(!plan.operations.first().accepted);
        QCOMPARE(plan.estimatedDurationSeconds, 95.0);
    }

    void rejectsUnknownTool()
    {
        const QString output = QStringLiteral(
            "{\"version\":\"1.0\",\"context_id\":\"ctx-12345678\","
            "\"summary\":\"run\",\"tool_calls\":[{\"id\":\"call-1\","
            "\"name\":\"system.run_command\",\"arguments\":{" 
            "\"start_seconds\":0,\"end_seconds\":5},\"reason\":\"test\"}]}");
        const auto plan = AgentToolRegistry::parsePlan(output, 100.0, QStringLiteral("ctx-12345678"));
        QVERIFY(!plan.error.isEmpty());
        QVERIFY(plan.operations.isEmpty());
    }

    void rejectsStaleContext()
    {
        const QString output = QStringLiteral(
            "{\"version\":\"1.0\",\"context_id\":\"old-context\","
            "\"summary\":\"delete\",\"tool_calls\":[{\"id\":\"call-1\","
            "\"name\":\"timeline.ripple_delete_range\",\"arguments\":{" 
            "\"start_seconds\":0,\"end_seconds\":5},\"reason\":\"test\"}]}");
        const auto plan = AgentToolRegistry::parsePlan(output, 100.0, QStringLiteral("new-context"));
        QVERIFY(!plan.error.isEmpty());
    }

    void rejectsOverlappingRanges()
    {
        const QString output = QStringLiteral(
            "{\"version\":\"1.0\",\"context_id\":\"ctx-12345678\","
            "\"summary\":\"delete\",\"tool_calls\":["
            "{\"id\":\"a\",\"name\":\"timeline.ripple_delete_range\","
            "\"arguments\":{\"start_seconds\":1,\"end_seconds\":8},\"reason\":\"a\"},"
            "{\"id\":\"b\",\"name\":\"timeline.ripple_delete_range\","
            "\"arguments\":{\"start_seconds\":7,\"end_seconds\":9},\"reason\":\"b\"}]}");
        const auto plan = AgentToolRegistry::parsePlan(output, 100.0, QStringLiteral("ctx-12345678"));
        QVERIFY(!plan.error.isEmpty());
    }

    void rejectsKeepMixedWithDelete()
    {
        const QString output = QStringLiteral(
            "{\"version\":\"1.0\",\"context_id\":\"ctx-12345678\","
            "\"summary\":\"mixed\",\"tool_calls\":["
            "{\"id\":\"a\",\"name\":\"timeline.keep_range\","
            "\"arguments\":{\"start_seconds\":1,\"end_seconds\":8},\"reason\":\"a\"},"
            "{\"id\":\"b\",\"name\":\"timeline.ripple_delete_range\","
            "\"arguments\":{\"start_seconds\":9,\"end_seconds\":10},\"reason\":\"b\"}]}");
        const auto plan = AgentToolRegistry::parsePlan(output, 100.0, QStringLiteral("ctx-12345678"));
        QVERIFY(!plan.error.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestAgentToolRegistry)

#include "test_agent_tool_registry.moc"
