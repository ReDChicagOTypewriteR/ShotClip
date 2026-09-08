/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "timelinesafetycommand.h"

#include "models/multitrackmodel.h"

TimelineSafetyCommand::TimelineSafetyCommand(MultitrackModel &model,
                                             MarkersModel &markers,
                                             int startFrame,
                                             int lengthFrames,
                                             QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_model(model)
    , m_markers(markers)
    , m_startFrame(startFrame)
    , m_lengthFrames(lengthFrames)
    , m_undoHelper(model)
    , m_beforeMarkers(markers.getMarkers())
{
    setText(QObject::tr("ShotClip safe ripple delete"));
    m_undoHelper.setHints(UndoHelper::RestoreTracks);
    m_afterMarkers = adjustedMarkers(m_beforeMarkers);
}

void TimelineSafetyCommand::redo()
{
    m_undoHelper.recordBeforeState();
    m_succeeded = m_model.rippleDeleteRange(m_startFrame, m_lengthFrames);
    m_undoHelper.recordAfterState();
    if (m_succeeded)
        m_markers.doReplace(m_afterMarkers);
}

void TimelineSafetyCommand::undo()
{
    if (!m_succeeded)
        return;
    m_undoHelper.undoChanges();
    m_markers.doReplace(m_beforeMarkers);
}

QList<Markers::Marker> TimelineSafetyCommand::adjustedMarkers(
    const QList<Markers::Marker> &markers) const
{
    QList<Markers::Marker> result;
    const int endFrame = m_startFrame + m_lengthFrames;
    for (auto marker : markers) {
        if (marker.end < marker.start)
            marker.end = marker.start;
        if (marker.end <= m_startFrame) {
            result << marker;
        } else if (marker.start >= endFrame) {
            marker.start -= m_lengthFrames;
            marker.end -= m_lengthFrames;
            result << marker;
        } else if (marker.start < m_startFrame && marker.end > endFrame) {
            marker.end -= m_lengthFrames;
            result << marker;
        } else if (marker.start < m_startFrame) {
            marker.end = m_startFrame;
            result << marker;
        } else if (marker.end > endFrame) {
            marker.start = m_startFrame;
            marker.end -= m_lengthFrames;
            result << marker;
        }
    }
    return result;
}
