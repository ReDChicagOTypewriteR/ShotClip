/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "subtitleutils.h"

namespace Subtitles {

QList<SubtitleItem> rippleDeletedItems(const QList<SubtitleItem> &items,
                                       int64_t startMs,
                                       int64_t endMs)
{
    if (startMs < 0 || endMs <= startMs)
        return items;

    const int64_t duration = endMs - startMs;
    QList<SubtitleItem> result;
    result.reserve(items.size());
    for (auto item : items) {
        if (item.end <= startMs) {
            result << item;
        } else if (item.start >= endMs) {
            item.start -= duration;
            item.end -= duration;
            result << item;
        } else if (item.start < startMs && item.end > endMs) {
            item.end -= duration;
            result << item;
        } else if (item.start < startMs) {
            item.end = startMs;
            if (item.end > item.start)
                result << item;
        } else if (item.end > endMs) {
            item.start = startMs;
            item.end -= duration;
            if (item.end > item.start)
                result << item;
        }
    }
    return result;
}

} // namespace Subtitles
