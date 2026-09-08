/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef AGENTTOOLREGISTRY_H
#define AGENTTOOLREGISTRY_H

#include "agenttypes.h"

#include <QJsonObject>
#include <QString>

class AgentToolRegistry
{
public:
    static QString jsonSchema();
    static QString buildPrompt(const QString &userRequest,
                               const QString &editingMode,
                               int targetDurationSeconds,
                               const QJsonObject &timelineContext);
    static AgentEditPlan parsePlan(const QString &modelOutput,
                                   double timelineDurationSeconds,
                                   const QString &expectedContextId);
    static QStringList allowedEditTools();

private:
    static QString extractJsonObject(const QString &text);
};

#endif
