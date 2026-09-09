// Copyright (c) 2026 AleXJokeR. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MEDIALIBRARYDOCK_H
#define MEDIALIBRARYDOCK_H
#include <QDockWidget>
class MediaLibraryDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit MediaLibraryDock(QWidget *parent = nullptr);
signals:
    void sourceRequested(const QString &path, double seconds);
    void addSourceRequested(const QString &path);
};
#endif
