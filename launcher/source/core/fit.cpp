#include "core/fit.h"

#include <algorithm>
#include <cmath>

Rect fitRect(float imgW, float imgH, const Rect& bounds) {
    if (!(imgW > 0) || !(imgH > 0) || bounds.empty()) return Rect{};
    const float scale = std::min(bounds.w / imgW, bounds.h / imgH);
    const float w = std::min(bounds.w, std::round(imgW * scale));
    const float h = std::min(bounds.h, std::round(imgH * scale));
    if (w <= 0 || h <= 0) return Rect{};
    return Rect{bounds.x + std::round((bounds.w - w) / 2), bounds.y + std::round((bounds.h - h) / 2), w, h};
}

CarouselGeometry carouselGeometry(const Rect& column, const Rect& center, float sideW, float sideH) {
    using G = CarouselGeometry;
    G g;
    const float colRight = column.x + column.w;
    const float centerRight = center.x + center.w;
    const float gap = std::clamp((column.w - center.w - 2 * sideW) / 2, -G::kMaxOverlap, G::kMaxGap);
    const float y = column.y + std::round((column.h - sideH) / 2);

    const float leftX = std::max(center.x - gap - sideW, column.x);
    if (center.x - leftX >= G::kMinVisible) {
        g.left = Rect{leftX, y, sideW, sideH};
        g.hitLeft = Rect{column.x, column.y, center.x - column.x, column.h};
    }
    const float rightX = std::min(centerRight + gap, colRight - sideW);
    if (rightX + sideW - centerRight >= G::kMinVisible) {
        g.right = Rect{rightX, y, sideW, sideH};
        g.hitRight = Rect{centerRight, column.y, colRight - centerRight, column.h};
    }
    return g;
}

Rect slideWithin(Rect r, float dx, float lo, float hi) {
    r.x = std::max(lo, std::min(r.x + dx, hi - r.w));
    return r;
}
