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

namespace {

// Abstände einer Fächerseite, gemessen von der Mittelkante nach außen. stages = 0, 1 oder 2.
struct FanSide {
    int stages = 0;
    float gap1 = 0, gap2 = 0;  // gap1 < -kMaxOverlap: Stufe 1 an den Rand geschoben
};

FanSide fanSide(float space, float w0, float w1) {
    using G = FanGeometry;
    FanSide f;
    const float total = std::floor(space) - w0 - w1;  // Platz für beide Abstände
    if (w1 > 0 && total >= -2 * G::kMaxOverlap) {
        f.stages = 2;
        f.gap2 = std::clamp(total - G::kGapCenter, -G::kMaxOverlap, G::kGapSide);
        f.gap1 = std::min(G::kGapCenter, total - f.gap2);
        return f;
    }
    if (w0 > 0 && std::floor(space) >= G::kMinVisible) {
        f.stages = 1;
        f.gap1 = std::min(G::kGapCenter, std::floor(space) - w0);
    }
    return f;
}

}  // namespace

FanGeometry fanGeometry(const Rect& area, const Rect& center, const Rect sideFrames[2]) {
    FanGeometry g;
    const float w0 = sideFrames[0].w, h0 = sideFrames[0].h;
    const float w1 = sideFrames[1].w, h1 = sideFrames[1].h;
    const float y0 = area.y + std::round((area.h - h0) / 2);
    const float y1 = area.y + std::round((area.h - h1) / 2);
    const float areaRight = area.x + area.w;
    const float centerRight = center.x + center.w;

    FanSide l = fanSide(center.x - area.x, w0, w1);
    if (l.stages >= 1) {
        g.left[0] = Rect{center.x - l.gap1 - w0, y0, w0, h0};
        g.hitLeft[0] = Rect{g.left[0].x, area.y, center.x - g.left[0].x, area.h};
    }
    if (l.stages == 2) {
        g.left[1] = Rect{g.left[0].x - l.gap2 - w1, y1, w1, h1};
        g.hitLeft[1] = Rect{area.x, area.y, g.left[0].x - area.x, area.h};
    } else if (l.stages == 1) {
        g.hitLeft[0].w += g.hitLeft[0].x - area.x;
        g.hitLeft[0].x = area.x;
    }

    FanSide r = fanSide(areaRight - centerRight, w0, w1);
    if (r.stages >= 1) {
        g.right[0] = Rect{centerRight + r.gap1, y0, w0, h0};
        g.hitRight[0] = Rect{centerRight, area.y, g.right[0].x + w0 - centerRight, area.h};
    }
    if (r.stages == 2) {
        const float r0 = g.right[0].x + w0;
        g.right[1] = Rect{r0 + r.gap2, y1, w1, h1};
        g.hitRight[1] = Rect{r0, area.y, areaRight - r0, area.h};
    } else if (r.stages == 1) {
        g.hitRight[0].w = areaRight - centerRight;
    }
    return g;
}

Rect slideWithin(Rect r, float dx, float lo, float hi) {
    r.x = std::max(lo, std::min(r.x + dx, hi - r.w));
    return r;
}
