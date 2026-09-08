/*
 * Copyright (c) 2026 AleXJokeR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHOTCLIP_SUBTITLEUTILS_H
#define SHOTCLIP_SUBTITLEUTILS_H

#include "models/subtitles.h"

#include <QList>

namespace Subtitles {

QList<SubtitleItem> rippleDeletedItems(const QList<SubtitleItem> &items,
                                       int64_t startMs,
                                       int64_t endMs);

} // namespace Subtitles

#endif
