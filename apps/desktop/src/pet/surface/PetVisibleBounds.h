#pragma once

#include <QImage>
#include <QRect>

QRect visibleBoundsFromImage(const QImage &image, int alphaThreshold);
