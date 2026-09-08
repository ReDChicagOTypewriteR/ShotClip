/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef AGENTDOCK_H
#define AGENTDOCK_H

#include "agent/agenttypes.h"

#include <QDockWidget>
#include <QJsonObject>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QTimeEdit;
class QComboBox;
class QToolButton;
class LocalLlamaProvider;

class AgentDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit AgentDock(QWidget *parent = nullptr);
    void setTimelineContext(double durationSeconds,
                            double positionSeconds,
                            bool hasTimeline,
                            const QJsonObject &structuredContext = QJsonObject());
    void applyFinished(bool success, const QString &message);
    void setRecoveryStatus(bool success, const QString &path);
    void refreshLocalModelStatus();

signals:
    void applyRequested(const QList<AgentEditOperation> &operations, const QString &contextId);
    void seekRequested(double seconds);
    void undoRequested();
    void transcribeRequested();
    void showSubtitlesRequested();
    void recoveryPointRequested();

private slots:
    void analyzePrompt();
    void applyAccepted();
    void acceptSelected();
    void rejectSelected();
    void previewSelected();
    void showModelSettings();
    void toggleFloatingMode();
    void updateWindowModeControls(bool floating);

private:
    void setPromptAndAnalyze(const QString &prompt);
    void showPlan(const AgentEditPlan &plan);
    void appendLog(const QString &message);
    void updateApplyState();
    static QString formatTime(double seconds);
    static QString riskText(AgentOperationRisk risk);

    QLabel *m_statusLabel;
    QLabel *m_contextLabel;
    QLabel *m_transcriptionStatusLabel;
    QLabel *m_plannerStatusLabel;
    QLabel *m_recoveryStatusLabel;
    QLabel *m_summaryLabel;
    QPlainTextEdit *m_promptEdit;
    QTimeEdit *m_targetDurationEdit;
    QComboBox *m_editingModeCombo;
    QListWidget *m_planList;
    QPushButton *m_analyzeButton;
    QPushButton *m_applyButton;
    QPlainTextEdit *m_logEdit;
    QToolButton *m_logToggle;
    QToolButton *m_floatButton;
    LocalLlamaProvider *m_llamaProvider;
    AgentEditPlan m_plan;
    QJsonObject m_timelineContext;
    QString m_contextId;
    double m_durationSeconds{0.0};
    double m_positionSeconds{0.0};
    bool m_hasTimeline{false};
};

#endif // AGENTDOCK_H
