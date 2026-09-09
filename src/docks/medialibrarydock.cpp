// Copyright (c) 2026 AleXJokeR. SPDX-License-Identifier: GPL-3.0-or-later
#include "medialibrarydock.h"
#include "agent/medialibrary.h"
#include "settings.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

MediaLibraryDock::MediaLibraryDock(QWidget *parent) : QDockWidget(tr("素材理解与文稿"), parent)
{
    setObjectName("ShotClipMediaLibraryDock");
    auto *library = new MediaLibrary(Settings.appDataLocation() + "/media-library-v1", this);
    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    auto *intro = new QLabel(tr("批量导入 → 开始分析 → 搜索原话 → 双击定位。分析结果保存在本机，跨项目可复用。"), root);
    intro->setWordWrap(true); layout->addWidget(intro);
    auto *actions = new QHBoxLayout;
    auto *files = new QPushButton(tr("导入素材"), root);
    auto *folder = new QPushButton(tr("导入文件夹"), root);
    auto *start = new QPushButton(tr("开始 / 继续"), root);
    auto *pause = new QPushButton(tr("暂停"), root);
    auto *retry = new QPushButton(tr("重试失败项"), root);
    for (auto *button : {files, folder}) actions->addWidget(button);
    layout->addLayout(actions);
    auto *queueActions = new QHBoxLayout;
    for (auto *button : {start, pause, retry}) queueActions->addWidget(button);
    layout->addLayout(queueActions);
    auto *status = new QLabel(library->error().isEmpty() ? tr("准备就绪；转录需要 Whisper 模型") : library->error(), root);
    status->setWordWrap(true); layout->addWidget(status);
    auto *items = new QListWidget(root); items->setAccessibleName(tr("素材分析任务")); layout->addWidget(items);
    auto *sourceActions = new QHBoxLayout;
    auto *transcript = new QPushButton(tr("查看完整文稿"), root);
    auto *append = new QPushButton(tr("加入播放列表"), root);
    sourceActions->addWidget(transcript); sourceActions->addWidget(append);
    layout->addLayout(sourceActions);
    auto *query = new QLineEdit(root); query->setPlaceholderText(tr("搜索文稿，例如：家庭、后果、反思"));
    query->setAccessibleName(tr("搜索全部素材文稿")); layout->addWidget(query);
    auto *results = new QListWidget(root); results->setWordWrap(true);
    results->setAccessibleName(tr("文稿搜索结果，双击播放原始素材")); layout->addWidget(results);
    auto *hint = new QLabel(tr("搜索最多显示 500 条；当前为关键词搜索。双击文稿定位原片，加入播放列表后可拖入时间线。"), root);
    hint->setWordWrap(true); layout->addWidget(hint);
    setWidget(root);
    const QStringList filters{"*.mp4", "*.mov", "*.mkv", "*.avi", "*.webm", "*.mp3", "*.wav", "*.aac", "*.flac", "*.m4a"};
    connect(files, &QPushButton::clicked, this, [this, library, filters] {
        library->addFiles(QFileDialog::getOpenFileNames(this, tr("导入分析素材"), {}, tr("媒体 (%1)").arg(filters.join(' '))));
    });
    struct ScanState {
        std::unique_ptr<QDirIterator> iterator;
        int count{0};
    };
    auto scan = std::make_shared<ScanState>();
    auto *scanTimer = new QTimer(this);
    scanTimer->setInterval(10);
    connect(folder, &QPushButton::clicked, this, [=] {
        auto directory = QFileDialog::getExistingDirectory(this, tr("递归导入素材文件夹"));
        if (directory.isEmpty()) return;
        scan->iterator = std::make_unique<QDirIterator>(directory, filters, QDir::Files, QDirIterator::Subdirectories);
        scan->count = 0;
        files->setEnabled(false); folder->setEnabled(false); start->setEnabled(false);
        retry->setEnabled(false); pause->setEnabled(true);
        scanTimer->start();
    });
    connect(scanTimer, &QTimer::timeout, this, [=] {
        QStringList batch;
        while (batch.size() < 100 && scan->iterator->hasNext()) batch << scan->iterator->next();
        scan->count += batch.size();
        if (!scan->iterator->hasNext()) { scanTimer->stop(); scan->iterator.reset(); }
        library->addFiles(batch);
        status->setText(tr("已扫描 %1 个媒体文件%2").arg(scan->count)
                            .arg(scan->iterator ? tr("，可暂停") : tr("，可以开始分析")));
    });
    connect(start, &QPushButton::clicked, this, [library] {
        auto executable = [](QString name) {
#ifdef Q_OS_WIN
            name += ".exe";
#endif
            auto bundled = QDir(QCoreApplication::applicationDirPath()).filePath(name);
            return QFileInfo(bundled).isExecutable() ? bundled : QStandardPaths::findExecutable(name);
        };
        library->start(executable("ffprobe"), executable("ffmpeg"), Settings.whisperExe(),
                       Settings.whisperModel(), Settings.whisperUseGpu());
    });
    connect(pause, &QPushButton::clicked, this, [=] {
        scanTimer->stop(); scan->iterator.reset();
        library->pause(); status->setText(tr("已暂停；已完成分段保留，点击继续恢复"));
    });
    connect(retry, &QPushButton::clicked, library, &MediaLibrary::retry);
    connect(library, &MediaLibrary::progress, status, &QLabel::setText);
    auto refresh = [=] {
        const auto selectedId = items->currentItem()
            ? items->currentItem()->data(Qt::UserRole).toJsonObject()["id"].toString() : QString();
        items->clear();
        for (auto value : library->items()) {
            auto item = value.toObject();
            const auto state = item["state"].toString();
            QString label = state == "ready" ? tr("完成") : state == "failed" ? tr("失败")
                             : state == "no_audio" ? tr("无音轨") : tr("待分析 / 可继续");
            auto *row = new QListWidgetItem(QString("%1 · %2 · %3/%4 秒 · %5×%6\n%7")
                .arg(item["name"].toString(), label).arg(item["completed"].toDouble(), 0, 'f', 0)
                .arg(item["duration"].toDouble(), 0, 'f', 0).arg(item["width"].toInt()).arg(item["height"].toInt())
                .arg(item["error"].toString()), items);
            row->setData(Qt::UserRole, item); row->setToolTip(item["path"].toString());
            if (item["id"].toString() == selectedId) items->setCurrentItem(row);
        }
        const bool busy = library->running() || bool(scan->iterator);
        const bool healthy = library->error().isEmpty();
        files->setEnabled(!busy && healthy); folder->setEnabled(!busy && healthy);
        start->setEnabled(!busy && healthy); pause->setEnabled(busy); retry->setEnabled(!busy && healthy);
    };
    connect(library, &MediaLibrary::changed, this, refresh); refresh();
    auto *debounce = new QTimer(this); debounce->setSingleShot(true); debounce->setInterval(250);
    connect(query, &QLineEdit::textChanged, debounce, [debounce] { debounce->start(); });
    connect(library, &MediaLibrary::changed, debounce, [debounce] { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, [=] {
        results->clear();
        for (auto value : library->search(query->text())) {
            auto hit = value.toObject();
            auto *row = new QListWidgetItem(QString("%1 · %2–%3 秒\n%4").arg(hit["name"].toString())
                .arg(hit["start"].toDouble(), 0, 'f', 2).arg(hit["end"].toDouble(), 0, 'f', 2)
                .arg(hit["text"].toString()), results);
            row->setData(Qt::UserRole, hit);
        }
    });
    auto preview = [this, status](QListWidgetItem *row) {
        auto hit = row->data(Qt::UserRole).toJsonObject();
        if (MediaLibrary::fingerprint(hit["path"].toString()) != hit["fingerprint"].toString()) {
            status->setText(tr("原始素材丢失或已改变，请重新导入")); return;
        }
        emit sourceRequested(hit["path"].toString(), hit["start"].toDouble());
    };
    connect(results, &QListWidget::itemDoubleClicked, this, preview);
    connect(items, &QListWidget::itemDoubleClicked, this, preview);
    auto selectionChanged = [=] {
        append->setEnabled(items->currentItem() != nullptr);
        transcript->setEnabled(items->currentItem() != nullptr);
    };
    connect(items, &QListWidget::itemSelectionChanged, this, selectionChanged);
    selectionChanged();
    connect(append, &QPushButton::clicked, this, [=] {
        if (!items->currentItem()) return;
        const auto item = items->currentItem()->data(Qt::UserRole).toJsonObject();
        if (MediaLibrary::fingerprint(item["path"].toString()) != item["fingerprint"].toString()) {
            status->setText(tr("原始素材丢失或已改变，请重新导入")); return;
        }
        emit addSourceRequested(item["path"].toString());
    });
    connect(transcript, &QPushButton::clicked, this, [=] {
        if (!items->currentItem()) return;
        const auto item = items->currentItem()->data(Qt::UserRole).toJsonObject();
        auto *dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(tr("文稿 · %1").arg(item["name"].toString()));
        dialog->resize(700, 500);
        auto *body = new QVBoxLayout(dialog);
        body->addWidget(new QLabel(tr("双击句子定位原片；这里显示已完成分析的文稿快照。"), dialog));
        auto *sentences = new QListWidget(dialog);
        sentences->setWordWrap(true); body->addWidget(sentences);
        for (auto value : item["segments"].toArray()) {
            auto segment = value.toObject();
            segment["path"] = item["path"]; segment["fingerprint"] = item["fingerprint"];
            auto *row = new QListWidgetItem(QString("%1–%2 秒\n%3")
                .arg(segment["start"].toDouble(), 0, 'f', 2).arg(segment["end"].toDouble(), 0, 'f', 2)
                .arg(segment["text"].toString()), sentences);
            row->setData(Qt::UserRole, segment);
        }
        if (!sentences->count()) body->addWidget(new QLabel(tr("尚无文稿，请先开始分析。"), dialog));
        connect(sentences, &QListWidget::itemDoubleClicked, this, preview);
        dialog->show();
    });
}
