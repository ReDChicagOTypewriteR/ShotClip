/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "agent/subtitleutils.h"

#include <QtTest>

class TestSubtitleRipple : public QObject
{
    Q_OBJECT

private slots:
    void removesTrimsAndMovesItems()
    {
        const QList<Subtitles::SubtitleItem> items{
            {0, 2000, "before"},
            {3000, 7000, "left overlap"},
            {5000, 6000, "removed"},
            {7000, 12000, "right overlap"},
            {13000, 15000, "after"},
        };

        const auto result = Subtitles::rippleDeletedItems(items, 5000, 10000);
        QCOMPARE(result.size(), 4);
        QCOMPARE(result.at(0).start, int64_t(0));
        QCOMPARE(result.at(0).end, int64_t(2000));
        QCOMPARE(result.at(1).start, int64_t(3000));
        QCOMPARE(result.at(1).end, int64_t(5000));
        QCOMPARE(result.at(2).start, int64_t(5000));
        QCOMPARE(result.at(2).end, int64_t(7000));
        QCOMPARE(result.at(3).start, int64_t(8000));
        QCOMPARE(result.at(3).end, int64_t(10000));
    }

    void preservesItemsForInvalidRange()
    {
        const QList<Subtitles::SubtitleItem> items{{1000, 2000, "text"}};
        const auto result = Subtitles::rippleDeletedItems(items, 5000, 5000);
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.first().start, int64_t(1000));
        QCOMPARE(result.first().end, int64_t(2000));
    }
};

QTEST_APPLESS_MAIN(TestSubtitleRipple)

#include "test_subtitle_ripple.moc"
