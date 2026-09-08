/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TIMELINESAFETYCOMMAND_H
#define TIMELINESAFETYCOMMAND_H

#include <QUuid>

#include "commands/undohelper.h"
#include "models/markersmodel.h"

#include <QUndoCommand>

class MultitrackModel;

class TimelineSafetyCommand : public QUndoCommand
{
public:
    TimelineSafetyCommand(MultitrackModel &model,
                          MarkersModel &markers,
                          int startFrame,
                          int lengthFrames,
                          QUndoCommand *parent = nullptr);
    void redo() override;
    void undo() override;
    bool succeeded() const { return m_succeeded; }

private:
    QList<Markers::Marker> adjustedMarkers(const QList<Markers::Marker> &markers) const;

    MultitrackModel &m_model;
    MarkersModel &m_markers;
    int m_startFrame;
    int m_lengthFrames;
    UndoHelper m_undoHelper;
    QList<Markers::Marker> m_beforeMarkers;
    QList<Markers::Marker> m_afterMarkers;
    bool m_succeeded{false};
};

#endif // TIMELINESAFETYCOMMAND_H
