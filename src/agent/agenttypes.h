/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef AGENTTYPES_H
#define AGENTTYPES_H

#include <QList>
#include <QMetaType>
#include <QString>

enum class AgentOperationType { RemoveRange, KeepRange, Advisory };

enum class AgentOperationRisk { Low, Medium, High };

struct AgentEditOperation
{
    QString id;
    QString title;
    QString reason;
    QString toolName;
    AgentOperationType type{AgentOperationType::Advisory};
    AgentOperationRisk risk{AgentOperationRisk::Low};
    double startSeconds{0.0};
    double endSeconds{0.0};
    bool accepted{true};
    bool executable{false};
};

struct AgentEditPlan
{
    QString summary;
    QString error;
    QList<AgentEditOperation> operations;
    double sourceDurationSeconds{0.0};
    double estimatedDurationSeconds{0.0};
    bool requiresModel{false};
    bool requestUndo{false};
    bool modelGenerated{false};
    QString contextId;
    QString providerName;
};

Q_DECLARE_METATYPE(AgentEditOperation)
Q_DECLARE_METATYPE(QList<AgentEditOperation>)
Q_DECLARE_METATYPE(AgentEditPlan)

#endif // AGENTTYPES_H
