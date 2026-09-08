/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "agentdock.h"

#include "Logger.h"
#include "agent/agentruleengine.h"
#include "agent/localllamaprovider.h"
#include "settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QTime>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int OperationIndexRole = Qt::UserRole + 1;

QPushButton *makeQuickButton(const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setMinimumHeight(32);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("quickAction", true);
    return button;
}

} // namespace

AgentDock::AgentDock(QWidget *parent)
    : QDockWidget(tr("AI 剪辑助理"), parent)
    , m_statusLabel(new QLabel(this))
    , m_contextLabel(new QLabel(this))
    , m_transcriptionStatusLabel(new QLabel(this))
    , m_plannerStatusLabel(new QLabel(this))
    , m_recoveryStatusLabel(new QLabel(this))
    , m_summaryLabel(new QLabel(this))
    , m_promptEdit(new QPlainTextEdit(this))
    , m_targetDurationEdit(new QTimeEdit(this))
    , m_editingModeCombo(new QComboBox(this))
    , m_planList(new QListWidget(this))
    , m_analyzeButton(new QPushButton(tr("分析并生成方案"), this))
    , m_applyButton(new QPushButton(tr("应用已接受建议"), this))
    , m_logEdit(new QPlainTextEdit(this))
    , m_logToggle(new QToolButton(this))
    , m_floatButton(new QToolButton(this))
    , m_llamaProvider(new LocalLlamaProvider(this))
{
    setObjectName(QStringLiteral("ShotClipAgentDock"));
    setMinimumWidth(360);
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
                | QDockWidget::DockWidgetFloatable);
    toggleViewAction()->setIcon(
        QIcon::fromTheme(QStringLiteral("speech-to-text"),
                         QIcon(QStringLiteral(":/icons/oxygen/32x32/actions/speech-to-text.png"))));
    toggleViewAction()->setText(tr("AI 助理"));
    toggleViewAction()->setToolTip(tr("显示或隐藏本地 AI 剪辑助理"));

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("ShotClipAgentRoot"));
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto *headerLayout = new QHBoxLayout;
    auto *logo = new QLabel(root);
    logo->setPixmap(QIcon(QStringLiteral(":/shotclip/shotclip-mark.svg")).pixmap(36, 36));
    logo->setFixedSize(40, 40);
    logo->setAccessibleName(tr("ShotClip 标志"));
    headerLayout->addWidget(logo);

    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(1);
    auto *title = new QLabel(QStringLiteral("ShotClip Agent"), root);
    title->setObjectName(QStringLiteral("AgentTitle"));
    auto *version = new QLabel(QStringLiteral("0.3.0 Demo · 结构化本地 Agent"), root);
    version->setObjectName(QStringLiteral("AgentSubtitle"));
    titleLayout->addWidget(title);
    titleLayout->addWidget(version);
    headerLayout->addLayout(titleLayout, 1);

    m_floatButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    m_floatButton->setToolTip(tr("在独立窗口中打开 Agent"));
    m_floatButton->setAccessibleName(tr("浮动或停靠 Agent 面板"));
    m_floatButton->setMinimumSize(36, 36);
    m_floatButton->setCursor(Qt::PointingHandCursor);
    connect(m_floatButton, &QToolButton::clicked, this, &AgentDock::toggleFloatingMode);
    headerLayout->addWidget(m_floatButton);

    auto *settingsButton = new QToolButton(root);
    settingsButton->setIcon(
        QIcon::fromTheme(QStringLiteral("configure"),
                         QIcon(QStringLiteral(":/icons/dark/32x32/server-database.png"))));
    settingsButton->setToolTip(tr("导入本地模型"));
    settingsButton->setAccessibleName(tr("导入本地模型"));
    settingsButton->setMinimumSize(36, 36);
    settingsButton->setCursor(Qt::PointingHandCursor);
    connect(settingsButton, &QToolButton::clicked, this, &AgentDock::showModelSettings);
    headerLayout->addWidget(settingsButton);

    auto *collapseButton = new QToolButton(root);
    collapseButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    collapseButton->setToolTip(tr("收起 Agent 面板（Ctrl+Shift+A 可再次打开）"));
    collapseButton->setAccessibleName(tr("收起 Agent 面板"));
    collapseButton->setMinimumSize(36, 36);
    collapseButton->setCursor(Qt::PointingHandCursor);
    connect(collapseButton, &QToolButton::clicked, this, &QWidget::hide);
    headerLayout->addWidget(collapseButton);
    rootLayout->addLayout(headerLayout);

    m_statusLabel->setObjectName(QStringLiteral("AgentStatus"));
    m_statusLabel->setText(tr("本地规则模式 · 完全离线 · 未加载模型"));
    m_statusLabel->setToolTip(tr("ShotClip 不会连接互联网，也不会在未确认时修改时间线。"));
    rootLayout->addWidget(m_statusLabel);

    m_contextLabel->setObjectName(QStringLiteral("AgentContext"));
    m_contextLabel->setWordWrap(true);
    rootLayout->addWidget(m_contextLabel);

    auto *localToolsGroup = new QGroupBox(tr("本地模型与恢复"), root);
    auto *localToolsLayout = new QVBoxLayout(localToolsGroup);
    localToolsLayout->setSpacing(7);
    m_transcriptionStatusLabel->setObjectName(QStringLiteral("AgentSecondaryStatus"));
    m_transcriptionStatusLabel->setWordWrap(true);
    localToolsLayout->addWidget(m_transcriptionStatusLabel);
    m_plannerStatusLabel->setObjectName(QStringLiteral("AgentSecondaryStatus"));
    m_plannerStatusLabel->setWordWrap(true);
    localToolsLayout->addWidget(m_plannerStatusLabel);
    auto *importModelsButton = new QPushButton(tr("导入本地模型…"), localToolsGroup);
    importModelsButton->setMinimumHeight(36);
    importModelsButton->setToolTip(tr("选择 GGUF 模型即可启用离线 Agent；转录模型为可选项。"));
    connect(importModelsButton, &QPushButton::clicked, this, &AgentDock::showModelSettings);
    localToolsLayout->addWidget(importModelsButton);
    auto *transcriptionActions = new QHBoxLayout;
    auto *transcribeButton = new QPushButton(tr("转录时间线"), localToolsGroup);
    auto *subtitlesButton = new QPushButton(tr("字幕与文稿"), localToolsGroup);
    transcribeButton->setMinimumHeight(34);
    subtitlesButton->setMinimumHeight(34);
    transcribeButton->setToolTip(tr("使用本机 whisper.cpp 将所选音轨转为字幕。"));
    subtitlesButton->setToolTip(tr("打开字幕面板，可搜索文本并双击定位播放头。"));
    connect(transcribeButton, &QPushButton::clicked, this, &AgentDock::transcribeRequested);
    connect(subtitlesButton, &QPushButton::clicked, this, &AgentDock::showSubtitlesRequested);
    transcriptionActions->addWidget(transcribeButton);
    transcriptionActions->addWidget(subtitlesButton);
    localToolsLayout->addLayout(transcriptionActions);

    m_recoveryStatusLabel->setObjectName(QStringLiteral("AgentSecondaryStatus"));
    m_recoveryStatusLabel->setWordWrap(true);
    m_recoveryStatusLabel->setText(tr("自动保存：每 60 秒；异常退出后打开项目时可恢复。"));
    localToolsLayout->addWidget(m_recoveryStatusLabel);
    auto *recoveryButton = new QPushButton(tr("立即创建恢复点"), localToolsGroup);
    recoveryButton->setMinimumHeight(34);
    recoveryButton->setToolTip(tr("立即写入独立的恢复文件，不覆盖当前项目文件。"));
    connect(recoveryButton, &QPushButton::clicked, this, &AgentDock::recoveryPointRequested);
    localToolsLayout->addWidget(recoveryButton);
    rootLayout->addWidget(localToolsGroup);

    auto *taskGroup = new QGroupBox(tr("剪辑任务"), root);
    auto *taskLayout = new QVBoxLayout(taskGroup);
    taskLayout->setSpacing(8);

    auto *optionsLayout = new QFormLayout;
    optionsLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_targetDurationEdit->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    m_targetDurationEdit->setTime(QTime(0, 10, 0));
    m_targetDurationEdit->setAccessibleName(tr("目标成片时长"));
    optionsLayout->addRow(tr("目标时长"), m_targetDurationEdit);
    m_editingModeCombo->addItems(
        {tr("忠实剪辑"), tr("标准压缩"), tr("精简表达")});
    m_editingModeCombo->setAccessibleName(tr("剪辑模式"));
    optionsLayout->addRow(tr("剪辑模式"), m_editingModeCombo);
    taskLayout->addLayout(optionsLayout);

    auto *promptLabel = new QLabel(tr("剪辑要求"), taskGroup);
    taskLayout->addWidget(promptLabel);
    m_promptEdit->setPlaceholderText(
        tr("例如：删除开头 5 秒；或把演讲剪成 10 分钟，保留事件后果和警示。"));
    m_promptEdit->setAccessibleName(tr("自然语言剪辑要求"));
    m_promptEdit->setMinimumHeight(86);
    m_promptEdit->setMaximumHeight(130);
    taskLayout->addWidget(m_promptEdit);

    auto *quickLayout = new QGridLayout;
    quickLayout->setHorizontalSpacing(6);
    quickLayout->setVerticalSpacing(6);
    const QList<QPair<QString, QString>> quickActions{
        {tr("剪成 10 分钟"), tr("把这段演讲剪成 10 分钟，保持原始顺序，不要改变原意")},
        {tr("删除开头 5 秒"), tr("删除开头 5 秒")},
        {tr("删除结尾 3 秒"), tr("删除结尾 3 秒")},
        {tr("保留 10–30 秒"), tr("保留 10 秒到 30 秒")},
    };
    for (int i = 0; i < quickActions.size(); ++i) {
        auto *button = makeQuickButton(quickActions.at(i).first, taskGroup);
        connect(button, &QPushButton::clicked, this, [this, quickActions, i]() {
            setPromptAndAnalyze(quickActions.at(i).second);
        });
        quickLayout->addWidget(button, i / 2, i % 2);
    }
    taskLayout->addLayout(quickLayout);

    m_analyzeButton->setObjectName(QStringLiteral("AgentPrimaryButton"));
    m_analyzeButton->setMinimumHeight(40);
    m_analyzeButton->setCursor(Qt::PointingHandCursor);
    m_analyzeButton->setAccessibleName(tr("分析并生成剪辑方案"));
    connect(m_analyzeButton, &QPushButton::clicked, this, &AgentDock::analyzePrompt);
    taskLayout->addWidget(m_analyzeButton);
    rootLayout->addWidget(taskGroup);

    auto *planGroup = new QGroupBox(tr("AI 草稿建议"), root);
    auto *planLayout = new QVBoxLayout(planGroup);
    planLayout->setSpacing(8);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setObjectName(QStringLiteral("AgentSummary"));
    m_summaryLabel->setText(tr("输入要求后生成建议。正式时间线不会自动改变。"));
    planLayout->addWidget(m_summaryLabel);

    m_planList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_planList->setAlternatingRowColors(true);
    m_planList->setWordWrap(true);
    m_planList->setMinimumHeight(170);
    m_planList->setAccessibleName(tr("剪辑建议列表"));
    connect(m_planList, &QListWidget::itemChanged, this, [this]() { updateApplyState(); });
    planLayout->addWidget(m_planList, 1);

    auto *itemActions = new QHBoxLayout;
    auto *previewButton = new QPushButton(tr("定位试听"), planGroup);
    auto *acceptButton = new QPushButton(tr("接受"), planGroup);
    auto *rejectButton = new QPushButton(tr("拒绝"), planGroup);
    previewButton->setMinimumHeight(32);
    acceptButton->setMinimumHeight(32);
    rejectButton->setMinimumHeight(32);
    connect(previewButton, &QPushButton::clicked, this, &AgentDock::previewSelected);
    connect(acceptButton, &QPushButton::clicked, this, &AgentDock::acceptSelected);
    connect(rejectButton, &QPushButton::clicked, this, &AgentDock::rejectSelected);
    itemActions->addWidget(previewButton);
    itemActions->addStretch();
    itemActions->addWidget(rejectButton);
    itemActions->addWidget(acceptButton);
    planLayout->addLayout(itemActions);

    m_applyButton->setObjectName(QStringLiteral("AgentApplyButton"));
    m_applyButton->setMinimumHeight(40);
    m_applyButton->setEnabled(false);
    m_applyButton->setCursor(Qt::PointingHandCursor);
    m_applyButton->setToolTip(tr("仅应用已接受且通过校验的规则建议，可一次撤销。"));
    connect(m_applyButton, &QPushButton::clicked, this, &AgentDock::applyAccepted);
    planLayout->addWidget(m_applyButton);
    rootLayout->addWidget(planGroup, 1);

    m_logToggle->setText(tr("运行日志"));
    m_logToggle->setCheckable(true);
    m_logToggle->setChecked(false);
    m_logToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_logToggle->setArrowType(Qt::RightArrow);
    m_logToggle->setCursor(Qt::PointingHandCursor);
    rootLayout->addWidget(m_logToggle);

    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(130);
    m_logEdit->setVisible(false);
    m_logEdit->setAccessibleName(tr("ShotClip Agent 运行日志"));
    rootLayout->addWidget(m_logEdit);
    connect(m_logToggle, &QToolButton::toggled, this, [this](bool checked) {
        m_logToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        m_logEdit->setVisible(checked);
    });

    root->setStyleSheet(QStringLiteral(
        "#ShotClipAgentRoot { background: palette(window); }"
        "#AgentTitle { font-size: 18px; font-weight: 600; }"
        "#AgentSubtitle, #AgentContext { color: #aeb8c4; }"
        "#AgentStatus { color: #b9e6ff; background: #17354a; border: 1px solid #2e607e; "
        "border-radius: 5px; padding: 6px 8px; }"
        "#AgentSecondaryStatus { color: #dbeafe; background: #111827; border: 1px solid #374151; "
        "border-radius: 5px; padding: 6px 8px; }"
        "#AgentSummary { background: palette(alternate-base); border-radius: 5px; padding: 8px; }"
        "QPushButton[quickAction=\"true\"] { text-align: left; padding: 5px 8px; }"
        "#AgentPrimaryButton { background: #2563eb; color: white; border: 1px solid #4f8cff; "
        "border-radius: 5px; font-weight: 600; padding: 7px; }"
        "#AgentPrimaryButton:hover { background: #3474ed; }"
        "#AgentPrimaryButton:pressed { background: #1d4ed8; }"
        "#AgentApplyButton { background: #0f766e; color: white; border-radius: 5px; "
        "font-weight: 600; padding: 7px; }"
        "#AgentApplyButton:hover { background: #12867d; }"
        "#AgentApplyButton:disabled { background: palette(midlight); color: palette(mid); }"
        "QPushButton:focus, QToolButton:focus, QComboBox:focus, QTimeEdit:focus, "
        "QPlainTextEdit:focus, QListWidget:focus { border: 2px solid #60a5fa; }"));

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(root);
    setWidget(scrollArea);
    connect(m_llamaProvider, &LocalLlamaProvider::diagnostic, this, [this](const QString &message) {
        appendLog(message);
    });
    connect(m_llamaProvider, &LocalLlamaProvider::failed, this, [this](const QString &message) {
        m_statusLabel->setText(tr("本地模型方案未通过 · 时间线未修改"));
        m_summaryLabel->setText(message);
        appendLog(message);
        m_analyzeButton->setEnabled(true);
        updateApplyState();
    });
    connect(m_llamaProvider,
            &LocalLlamaProvider::planReady,
            this,
            [this](const AgentEditPlan &plan) { showPlan(plan); });
    connect(this, &QDockWidget::topLevelChanged, this, &AgentDock::updateWindowModeControls);
    connect(m_llamaProvider, &LocalLlamaProvider::runningChanged, this, [this](bool running) {
        m_analyzeButton->setEnabled(true);
        m_analyzeButton->setText(running ? tr("停止本地分析") : tr("分析并生成方案"));
        if (running)
            m_applyButton->setEnabled(false);
        else
            updateApplyState();
        if (running) {
            m_statusLabel->setText(tr("llama.cpp 正在本机分析 · 可随时停止"));
            m_summaryLabel->setText(tr("正在生成结构化工具方案。模型不会直接修改时间线。"));
        }
        refreshLocalModelStatus();
    });
    setTimelineContext(0.0, 0.0, false);
    refreshLocalModelStatus();
    appendLog(tr("ShotClip 0.3.0 Agent 已启动：llama.cpp、白名单工具、无网络请求。"));
}

void AgentDock::toggleFloatingMode()
{
    const bool shouldFloat = !isFloating();
    setFloating(shouldFloat);
    if (shouldFloat) {
        resize(520, 780);
        show();
        raise();
        activateWindow();
    }
}

void AgentDock::updateWindowModeControls(bool floating)
{
    m_floatButton->setIcon(style()->standardIcon(floating ? QStyle::SP_TitleBarNormalButton
                                                         : QStyle::SP_TitleBarMaxButton));
    m_floatButton->setToolTip(floating ? tr("停靠到剪辑主窗口右侧")
                                      : tr("在独立窗口中打开 Agent"));
}

void AgentDock::setTimelineContext(double durationSeconds,
                                   double positionSeconds,
                                   bool hasTimeline,
                                   const QJsonObject &structuredContext)
{
    m_durationSeconds = qMax(0.0, durationSeconds);
    m_positionSeconds = qMax(0.0, positionSeconds);
    m_hasTimeline = hasTimeline;
    const bool hasNewStructuredContext = !structuredContext.isEmpty();
    const QString newContextId = hasNewStructuredContext
                                     ? structuredContext.value(QStringLiteral("context_id")).toString()
                                     : m_contextId;
    const bool invalidatesModelPlan = hasNewStructuredContext && !m_plan.operations.isEmpty()
                                      && !m_plan.contextId.isEmpty() && !m_contextId.isEmpty()
                                      && newContextId != m_contextId;
    if (hasNewStructuredContext) {
        m_timelineContext = structuredContext;
        m_contextId = newContextId;
    } else if (!hasTimeline) {
        m_timelineContext = QJsonObject();
        m_contextId.clear();
    } else if (!m_timelineContext.isEmpty()) {
        m_timelineContext.insert(QStringLiteral("playhead_seconds"), m_positionSeconds);
    }
    if (hasTimeline) {
        m_contextLabel->setText(
            tr("当前上下文：时间线 · 播放头 %1 · 总时长 %2 · 安全操作全部轨道")
                .arg(formatTime(m_positionSeconds), formatTime(m_durationSeconds)));
    } else {
        m_contextLabel->setText(tr("当前上下文：时间线为空。请先将素材拖入时间线。"));
    }
    if (invalidatesModelPlan) {
        m_statusLabel->setText(tr("时间线已变化 · 旧模型方案已失效"));
        m_summaryLabel->setText(tr("请重新分析，以免把旧时间范围应用到新的时间线。"));
        appendLog(tr("检测到时间线上下文变化，已禁用旧模型方案。"));
    }
    updateApplyState();
}

void AgentDock::applyFinished(bool success, const QString &message)
{
    m_statusLabel->setText(success ? tr("方案已应用 · 可使用 Ctrl+Z 整体撤销")
                                   : tr("方案未应用 · 请检查时间线"));
    m_summaryLabel->setText(message);
    appendLog(message);
    m_analyzeButton->setEnabled(true);
    updateApplyState();
}

void AgentDock::setRecoveryStatus(bool success, const QString &path)
{
    m_recoveryStatusLabel->setText(
        success ? tr("恢复点已保存：%1").arg(QFileInfo(path).fileName())
                : tr("恢复点保存失败。请查看运行日志。"));
    appendLog(m_recoveryStatusLabel->text());
}

void AgentDock::refreshLocalModelStatus()
{
    const bool exeExists = QFileInfo(Settings.whisperExe()).isExecutable();
    const bool modelExists = QFileInfo(Settings.whisperModel()).isFile();
    if (exeExists && modelExists) {
        m_transcriptionStatusLabel->setText(
            tr("whisper.cpp：已就绪 · %1").arg(QFileInfo(Settings.whisperModel()).fileName()));
    } else {
        m_transcriptionStatusLabel->setText(
            exeExists ? tr("语音转录：运行引擎已内置 · 可选导入 Whisper GGML 模型")
                      : tr("语音转录：开发环境未找到 whisper-cli · 可在高级设置中指定"));
    }
    QString plannerError;
    if (m_llamaProvider->isConfigured(&plannerError)) {
        m_plannerStatusLabel->setToolTip(QString());
        m_plannerStatusLabel->setText(
            tr("llama.cpp：已就绪 · %1 · 结构化工具模式")
                .arg(m_llamaProvider->modelDisplayName()));
    } else {
        const bool runtimeExists = QFileInfo(LocalLlamaProvider::executablePath()).isExecutable();
        m_plannerStatusLabel->setText(
            runtimeExists ? tr("剪辑 Agent：运行引擎已内置 · 请选择一个 GGUF 模型")
                          : tr("剪辑 Agent：开发环境未找到 llama-cli · 可在高级设置中指定"));
        m_plannerStatusLabel->setToolTip(plannerError);
    }
}

void AgentDock::analyzePrompt()
{
    if (m_llamaProvider->isRunning()) {
        m_llamaProvider->cancel();
        m_statusLabel->setText(tr("本地模型分析已停止 · 时间线未修改"));
        m_summaryLabel->setText(tr("可以修改要求或模型设置后重新分析。"));
        return;
    }
    const int targetSeconds = QTime(0, 0).secsTo(m_targetDurationEdit->time());
    appendLog(tr("开始分析：%1").arg(m_promptEdit->toPlainText().simplified()));
    auto plan = AgentRuleEngine::analyze(m_promptEdit->toPlainText(),
                                         m_durationSeconds,
                                         targetSeconds);
    if (plan.requestUndo) {
        appendLog(tr("请求撤销上一次修改。"));
        emit undoRequested();
        return;
    }
    if (std::any_of(plan.operations.cbegin(),
                    plan.operations.cend(),
                    [](const AgentEditOperation &operation) { return operation.executable; })) {
        plan.contextId = m_contextId;
        showPlan(plan);
        return;
    }
    QString configurationError;
    if (!m_llamaProvider->isConfigured(&configurationError)) {
        showPlan(plan);
        m_summaryLabel->setText(
            tr("%1\n配置 llama-cli 与 GGUF 模型后，可生成严格校验的结构化剪辑方案。")
                .arg(configurationError));
        return;
    }
    if (!m_hasTimeline || m_timelineContext.isEmpty()) {
        m_summaryLabel->setText(tr("当前时间线为空，无法启动本地模型分析。"));
        return;
    }
    m_plan = AgentEditPlan();
    m_planList->clear();
    m_llamaProvider->analyze(m_promptEdit->toPlainText(),
                             m_editingModeCombo->currentText(),
                             targetSeconds,
                             m_timelineContext);
}

void AgentDock::applyAccepted()
{
    QList<AgentEditOperation> accepted;
    for (int row = 0; row < m_planList->count(); ++row) {
        auto *item = m_planList->item(row);
        const int operationIndex = item->data(OperationIndexRole).toInt();
        if (operationIndex < 0 || operationIndex >= m_plan.operations.size())
            continue;
        if (item->checkState() == Qt::Checked && m_plan.operations.at(operationIndex).executable)
            accepted << m_plan.operations.at(operationIndex);
    }
    if (accepted.isEmpty()) {
        m_summaryLabel->setText(tr("没有已接受的可执行建议。语义分析任务需要先导入本地模型。"));
        appendLog(tr("应用已取消：没有可执行建议。"));
        return;
    }
    m_analyzeButton->setEnabled(false);
    m_applyButton->setEnabled(false);
    appendLog(tr("提交 %1 项规则操作到 Shotcut 时间线执行器。").arg(accepted.size()));
    emit applyRequested(accepted, m_plan.contextId);
}

void AgentDock::acceptSelected()
{
    auto *item = m_planList->currentItem();
    if (item && (item->flags() & Qt::ItemIsUserCheckable))
        item->setCheckState(Qt::Checked);
}

void AgentDock::rejectSelected()
{
    auto *item = m_planList->currentItem();
    if (item && (item->flags() & Qt::ItemIsUserCheckable))
        item->setCheckState(Qt::Unchecked);
}

void AgentDock::previewSelected()
{
    auto *item = m_planList->currentItem();
    if (!item)
        return;
    const int operationIndex = item->data(OperationIndexRole).toInt();
    if (operationIndex < 0 || operationIndex >= m_plan.operations.size())
        return;
    const auto &operation = m_plan.operations.at(operationIndex);
    if (!operation.executable) {
        m_summaryLabel->setText(tr("此项是后续本地模型任务，没有可定位的时间范围。"));
        return;
    }
    emit seekRequested(operation.startSeconds);
    appendLog(tr("定位到 %1：%2").arg(formatTime(operation.startSeconds), operation.title));
}

void AgentDock::showModelSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("导入 ShotClip 本地模型"));
    dialog.setMinimumWidth(560);
    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(
        tr("安装版已经内置离线运行引擎。选择一个 GGUF 模型后即可使用剪辑 Agent；"
           "如果需要语音转字幕，再选择一个 Whisper GGML 模型。ShotClip 只保存模型路径，"
           "不会上传素材或访问云端。"),
        &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    QSettings settings;
    auto *modelForm = new QFormLayout;
    auto *whisperExe = new QLineEdit(Settings.whisperExe(), &dialog);
    auto *whisperPath = new QLineEdit(Settings.whisperModel(), &dialog);
    auto *llamaExe = new QLineEdit(LocalLlamaProvider::executablePath(), &dialog);
    auto *plannerPath = new QLineEdit(LocalLlamaProvider::modelPath(), &dialog);
    auto *useGpu = new QCheckBox(tr("使用 GPU（失败时会自动回退 CPU）"), &dialog);
    useGpu->setChecked(Settings.whisperUseGpu());
    auto *contextSize = new QSpinBox(&dialog);
    contextSize->setRange(2048, 131072);
    contextSize->setSingleStep(2048);
    contextSize->setValue(LocalLlamaProvider::contextSize());
    contextSize->setSuffix(tr(" tokens"));
    auto *threads = new QSpinBox(&dialog);
    threads->setRange(1, 128);
    threads->setValue(LocalLlamaProvider::threadCount());
    auto *gpuLayers = new QSpinBox(&dialog);
    gpuLayers->setRange(0, 999);
    gpuLayers->setValue(LocalLlamaProvider::gpuLayers());
    gpuLayers->setSpecialValueText(tr("仅 CPU"));

    const auto makePathRow = [&dialog](QLineEdit *edit,
                                       const QString &filter,
                                       const QString &title) {
        auto *row = new QWidget(&dialog);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *browse = new QPushButton(QObject::tr("浏览…"), row);
        browse->setMinimumHeight(32);
        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(browse);
        QObject::connect(browse, &QPushButton::clicked, row, [edit, filter, title, row]() {
            const QString path = QFileDialog::getOpenFileName(
                row, title, edit->text(), filter);
            if (!path.isEmpty())
                edit->setText(path);
        });
        return row;
    };
    modelForm->addRow(tr("剪辑 Agent 模型"),
                      makePathRow(plannerPath,
                                  tr("GGUF 模型 (*.gguf);;所有文件 (*)"),
                                  tr("选择剪辑 Agent GGUF 模型")));
    modelForm->addRow(tr("语音转录模型（可选）"),
                      makePathRow(whisperPath,
                                  tr("Whisper 模型 (*.bin);;所有文件 (*)"),
                                  tr("选择 Whisper GGML 模型")));
    layout->addLayout(modelForm);

    auto *advancedToggle = new QToolButton(&dialog);
    advancedToggle->setText(tr("高级运行时设置"));
    advancedToggle->setCheckable(true);
    advancedToggle->setChecked(false);
    advancedToggle->setArrowType(Qt::RightArrow);
    advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    advancedToggle->setCursor(Qt::PointingHandCursor);
    advancedToggle->setAccessibleName(tr("展开高级运行时设置"));
    layout->addWidget(advancedToggle);

    auto *advancedPanel = new QWidget(&dialog);
    auto *advancedForm = new QFormLayout(advancedPanel);
    advancedForm->setContentsMargins(0, 0, 0, 0);
    advancedForm->addRow(tr("llama-cli"),
                         makePathRow(llamaExe,
                                     tr("可执行文件 (*)"),
                                     tr("选择 llama-cli 可执行文件")));
    advancedForm->addRow(tr("whisper-cli"),
                         makePathRow(whisperExe,
                                     tr("可执行文件 (*)"),
                                     tr("选择 whisper-cli 可执行文件")));
    advancedForm->addRow(tr("转录加速"), useGpu);
    advancedForm->addRow(tr("模型上下文"), contextSize);
    advancedForm->addRow(tr("CPU 线程"), threads);
    advancedForm->addRow(tr("GPU 卸载层"), gpuLayers);
    advancedPanel->setVisible(false);
    connect(advancedToggle, &QToolButton::toggled, advancedPanel, [advancedToggle, advancedPanel](bool checked) {
        advancedToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        advancedToggle->setAccessibleName(checked ? QObject::tr("收起高级运行时设置")
                                                  : QObject::tr("展开高级运行时设置"));
        advancedPanel->setVisible(checked);
    });
    layout->addWidget(advancedPanel);

    auto *status = new QLabel(tr("推荐：Qwen3 8B Instruct GGUF 用于剪辑规划；"
                                 "Whisper large-v3-turbo GGML 用于中文转录。"
                                 "模型保留在你选择的位置，不会复制进项目或安装目录。"),
                              &dialog);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         Qt::Horizontal,
                                         &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        Settings.setWhisperExe(whisperExe->text().trimmed());
        Settings.setWhisperModel(whisperPath->text().trimmed());
        Settings.setWhisperUseGpu(useGpu->isChecked());
        LocalLlamaProvider::setExecutablePath(llamaExe->text().trimmed());
        LocalLlamaProvider::setModelPath(plannerPath->text().trimmed());
        LocalLlamaProvider::setContextSize(contextSize->value());
        LocalLlamaProvider::setThreadCount(threads->value());
        LocalLlamaProvider::setGpuLayers(gpuLayers->value());
        settings.sync();
        const bool whisperExists = QFileInfo(Settings.whisperExe()).isExecutable()
                                   && QFileInfo(whisperPath->text().trimmed()).isFile();
        const bool plannerExists = QFileInfo(LocalLlamaProvider::executablePath()).isExecutable()
                                   && QFileInfo(plannerPath->text().trimmed()).isFile();
        m_statusLabel->setText(
            (whisperExists || plannerExists)
                ? tr("本地模型配置已保存 · 完全离线")
                : tr("本地规则模式 · 完全离线 · 未加载模型"));
        refreshLocalModelStatus();
        appendLog(tr("已保存本地模型路径设置。"));
    }
}

void AgentDock::setPromptAndAnalyze(const QString &prompt)
{
    m_promptEdit->setPlainText(prompt);
    analyzePrompt();
}

void AgentDock::showPlan(const AgentEditPlan &plan)
{
    m_plan = plan;
    m_planList->blockSignals(true);
    m_planList->clear();

    if (!plan.error.isEmpty()) {
        m_summaryLabel->setText(plan.error);
        m_statusLabel->setText(tr("方案校验未通过 · 时间线未修改"));
        appendLog(plan.error);
        m_planList->blockSignals(false);
        updateApplyState();
        return;
    }

    m_summaryLabel->setText(
        tr("%1\n原始 %2 → 预计 %3")
            .arg(plan.summary,
                 formatTime(plan.sourceDurationSeconds),
                 formatTime(plan.estimatedDurationSeconds)));
    m_statusLabel->setText(
        plan.modelGenerated ? tr("llama.cpp 结构化方案 · 等待逐项确认")
                            : (plan.requiresModel ? tr("规则模式 · 语义任务等待本地模型")
                                                  : tr("规则方案已生成 · 等待用户确认")));
    if (!plan.contextId.isEmpty() && plan.contextId != m_contextId) {
        m_statusLabel->setText(tr("模型方案已过期 · 不能应用"));
        m_summaryLabel->setText(tr("模型分析期间时间线发生变化。建议仅供查看，请重新分析。"));
    }

    for (int i = 0; i < plan.operations.size(); ++i) {
        const auto &operation = plan.operations.at(i);
        QString range;
        if (operation.executable) {
            range = QStringLiteral("%1 – %2")
                        .arg(formatTime(operation.startSeconds), formatTime(operation.endSeconds));
        } else {
            range = tr("分析任务");
        }
        const QString tool = operation.toolName.isEmpty()
                                 ? tr("本地规则")
                                 : operation.toolName;
        const QString text = QStringLiteral("%1\n%2 · 风险：%3 · %4\n%5")
                                 .arg(operation.title,
                                      range,
                                      riskText(operation.risk),
                                      tool,
                                      operation.reason);
        auto *item = new QListWidgetItem(text, m_planList);
        item->setData(OperationIndexRole, i);
        item->setToolTip(operation.reason);
        item->setSizeHint(QSize(0, operation.executable ? 78 : 72));
        if (operation.executable) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(operation.accepted ? Qt::Checked : Qt::Unchecked);
        } else {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
        }
    }
    if (m_planList->count() > 0)
        m_planList->setCurrentRow(0);
    m_planList->blockSignals(false);
    appendLog(plan.summary);
    updateApplyState();
}

void AgentDock::appendLog(const QString &message)
{
    const QString line = QStringLiteral("[%1] %2").arg(QTime::currentTime().toString("HH:mm:ss"),
                                                        message);
    m_logEdit->appendPlainText(line);
    LOG_INFO() << "ShotClip Agent:" << message;
}

void AgentDock::updateApplyState()
{
    bool hasAcceptedExecutable = false;
    for (int row = 0; row < m_planList->count(); ++row) {
        auto *item = m_planList->item(row);
        const int index = item->data(OperationIndexRole).toInt();
        if (index >= 0 && index < m_plan.operations.size() && m_plan.operations.at(index).executable
            && item->checkState() == Qt::Checked) {
            hasAcceptedExecutable = true;
            break;
        }
    }
    const bool contextIsCurrent = !m_plan.contextId.isEmpty() && !m_contextId.isEmpty()
                                  && m_plan.contextId == m_contextId;
    m_applyButton->setEnabled(hasAcceptedExecutable && m_analyzeButton->isEnabled()
                              && !m_llamaProvider->isRunning() && contextIsCurrent);
}

QString AgentDock::formatTime(double seconds)
{
    const qint64 totalSeconds = qMax<qint64>(0, qRound64(seconds));
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 remainingSeconds = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}

QString AgentDock::riskText(AgentOperationRisk risk)
{
    switch (risk) {
    case AgentOperationRisk::High:
        return tr("高");
    case AgentOperationRisk::Medium:
        return tr("中");
    default:
        return tr("低");
    }
}
