/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef AGENTRULEENGINE_H
#define AGENTRULEENGINE_H

#include "agenttypes.h"

class AgentRuleEngine
{
public:
    static AgentEditPlan analyze(const QString &prompt,
                                 double sourceDurationSeconds,
                                 double targetDurationSeconds);
    static bool validate(const AgentEditPlan &plan, QString *error = nullptr);

private:
    static QString nextId(int index);
};

#endif // AGENTRULEENGINE_H
