#include "test.h"

#include "core/fit.h"

static bool same(const Rect& a, float x, float y, float w, float h) {
    return a.x == x && a.y == y && a.w == w && a.h == h;
}

// --- fitRect -------------------------------------------------------------------------------

TEST(fit_portrait_into_portrait_scales_down) {
    // Wii-Cover 160x224 in das alte Vorschaumaß 140x196: gleiches Seitenverhältnis.
    CHECK(same(fitRect(160, 224, Rect{0, 0, 140, 196}), 0, 0, 140, 196));
}

TEST(fit_portrait_into_wide_bounds_is_centered_horizontally) {
    // Linke Spalte bei 640 px: 286 breit, 224 hoch.
    CHECK(same(fitRect(160, 224, Rect{32, 112, 286, 224}), 95, 112, 160, 224));
}

TEST(fit_landscape_into_portrait_is_centered_vertically) {
    // N64-Box 312x228 in 140x196: volle Breite, Höhe 140 * 228 / 312 = 102.3 -> 102.
    CHECK(same(fitRect(312, 228, Rect{0, 0, 140, 196}), 0, 47, 140, 102));
}

TEST(fit_landscape_limited_by_width) {
    CHECK(same(fitRect(312, 228, Rect{32, 112, 286, 224}), 32, 120, 286, 209));
}

TEST(fit_exact_size_is_unchanged) {
    CHECK(same(fitRect(160, 224, Rect{5, 7, 160, 224}), 5, 7, 160, 224));
}

TEST(fit_scales_up_small_images) {
    CHECK(same(fitRect(20, 28, Rect{0, 0, 140, 196}), 0, 0, 140, 196));
}

TEST(fit_zero_size_gives_empty_rect) {
    CHECK(fitRect(0, 224, Rect{0, 0, 140, 196}).empty());
    CHECK(fitRect(160, 0, Rect{0, 0, 140, 196}).empty());
    CHECK(fitRect(160, 224, Rect{0, 0, 0, 196}).empty());
    CHECK(fitRect(-1, 224, Rect{0, 0, 140, 196}).empty());
}

static bool inside(const Rect& r, const Rect& col) {
    return r.x >= col.x && r.x + r.w <= col.x + col.w && r.y >= col.y && r.y + r.h <= col.y + col.h;
}

// --- fanGeometry ---------------------------------------------------------------------------

// Fächer über die volle Breite bei 640 (x 32, Breite 576) und als linke Spalte (Breite 285),
// jeweils 240 hoch ab y 100. Seitenrahmen: Stufe 1 = 110x154, Stufe 2 = 76x106.
static const Rect kFanFull{32, 100, 576, 240};
static const Rect kFanColumn{32, 100, 285, 240};
static const Rect kFrames[2] = {Rect{0, 0, 110, 154}, Rect{0, 0, 76, 106}};

TEST(fanFullWidthFiveCovers) {
    // 854: genug Platz, Abstände wie vorgegeben.
    const Rect area{32, 100, 790, 240};
    Rect center = fitRect(160, 224, area);  // 171 x 240
    CHECK_EQ(center.w, 171);
    FanGeometry g = fanGeometry(area, center, kFrames);
    for (int i = 0; i < 2; ++i) {
        CHECK(!g.left[i].empty());
        CHECK(!g.right[i].empty());
        CHECK(inside(g.left[i], area));
        CHECK(inside(g.right[i], area));
        CHECK_EQ(g.left[i].w, kFrames[i].w);
        CHECK_EQ(g.left[i].h, kFrames[i].h);
    }
    // Abstände Mitte-Stufe 1 = 14, Stufe 1-Stufe 2 = 10.
    CHECK_EQ(center.x - (g.left[0].x + g.left[0].w), 14);
    CHECK_EQ(g.left[0].x - (g.left[1].x + g.left[1].w), 10);
    CHECK_EQ(g.right[0].x - (center.x + center.w), 14);
    CHECK_EQ(g.right[1].x - (g.right[0].x + g.right[0].w), 10);
    // senkrecht mittig
    CHECK_EQ(g.left[0].y, 143);  // 100 + (240 - 154) / 2
    CHECK_EQ(g.right[1].y, 167);  // 100 + (240 - 106) / 2

    // 640: 171 + 2 * (14 + 110 + 10 + 76) = 591 > 576, der äußere Abstand wird enger,
    // alle fünf bleiben in der Fläche.
    Rect c640 = fitRect(160, 224, kFanFull);
    FanGeometry h = fanGeometry(kFanFull, c640, kFrames);
    for (int i = 0; i < 2; ++i) {
        CHECK(!h.left[i].empty());
        CHECK(!h.right[i].empty());
        CHECK(inside(h.left[i], kFanFull));
        CHECK(inside(h.right[i], kFanFull));
    }
    CHECK_EQ(c640.x - (h.left[0].x + h.left[0].w), 14);
    CHECK(h.left[0].x - (h.left[1].x + h.left[1].w) >= -FanGeometry::kMaxOverlap);
    CHECK(h.left[0].x - (h.left[1].x + h.left[1].w) < 10);
}

TEST(fanWideCenterDropsOuter) {
    Rect center{32 + 131, 100, 314, 240};  // je Seite 131 frei: Stufe 1 passt, Stufe 2 nicht
    FanGeometry g = fanGeometry(kFanFull, center, kFrames);
    CHECK(!g.left[0].empty());
    CHECK(!g.right[0].empty());
    CHECK(g.left[1].empty());
    CHECK(g.right[1].empty());
    CHECK(g.hitLeft[1].empty());
    CHECK(g.hitRight[1].empty());
    CHECK(g.left[0].x + g.left[0].w - center.x <= FanGeometry::kMaxOverlap);
    CHECK(center.x + center.w - g.right[0].x <= FanGeometry::kMaxOverlap);
    CHECK(inside(g.left[0], kFanFull));
    CHECK(inside(g.right[0], kFanFull));
}

TEST(fanNarrowColumnOneStage) {
    Rect center = fitRect(160, 224, kFanColumn);  // 171 x 240, je Seite 57 frei
    FanGeometry g = fanGeometry(kFanColumn, center, kFrames);
    CHECK(!g.left[0].empty());
    CHECK(!g.right[0].empty());
    CHECK(g.left[1].empty());
    CHECK(g.right[1].empty());
    // an den Rand geschoben, liegt hinter der Mitte, ragt nicht über die Fläche hinaus
    CHECK(inside(g.left[0], kFanColumn));
    CHECK(inside(g.right[0], kFanColumn));
    CHECK_EQ(g.left[0].x, 32);
    CHECK_EQ(g.right[0].x + g.right[0].w, 32 + 285);
}

TEST(fanCenterFillingAreaHidesSides) {
    Rect center{32 + 10, 100, 285 - 20, 240};  // je 10 px frei: weniger als kMinVisible
    FanGeometry g = fanGeometry(kFanColumn, center, kFrames);
    for (int i = 0; i < 2; ++i) {
        CHECK(g.left[i].empty());
        CHECK(g.right[i].empty());
        CHECK(g.hitLeft[i].empty());
        CHECK(g.hitRight[i].empty());
    }
}

TEST(fanHitRectsSpanHeight) {
    Rect center = fitRect(160, 224, kFanFull);
    FanGeometry g = fanGeometry(kFanFull, center, kFrames);
    for (int i = 0; i < 2; ++i) {
        CHECK_EQ(g.hitLeft[i].y, 100);
        CHECK_EQ(g.hitLeft[i].h, 240);
        CHECK_EQ(g.hitRight[i].y, 100);
        CHECK_EQ(g.hitRight[i].h, 240);
    }
    // äußerste Stufe bis zum Flächenrand, lückenlos bis zur Mitte
    CHECK_EQ(g.hitLeft[1].x, 32);
    CHECK_EQ(g.hitLeft[1].x + g.hitLeft[1].w, g.hitLeft[0].x);
    CHECK_EQ(g.hitLeft[0].x + g.hitLeft[0].w, center.x);
    CHECK_EQ(g.hitRight[0].x, center.x + center.w);
    CHECK_EQ(g.hitRight[0].x + g.hitRight[0].w, g.hitRight[1].x);
    CHECK_EQ(g.hitRight[1].x + g.hitRight[1].w, 32 + 576);
    // die Stufe selbst liegt in ihrer Klickfläche
    CHECK(g.hitLeft[1].contains(g.left[1].x + 1, g.left[1].y + 1));
    CHECK(g.hitRight[0].contains(g.right[0].x + 1, g.right[0].y + 1));

    // nur Stufe 1: deren Klickfläche reicht bis zum Rand
    Rect col = fitRect(160, 224, kFanColumn);
    FanGeometry n = fanGeometry(kFanColumn, col, kFrames);
    CHECK_EQ(n.hitLeft[0].x, 32);
    CHECK_EQ(n.hitLeft[0].x + n.hitLeft[0].w, col.x);
    CHECK_EQ(n.hitRight[0].x + n.hitRight[0].w, 32 + 285);
}

// --- slideWithin ---------------------------------------------------------------------------

TEST(slide_moves_rect_inside_range) {
    CHECK(same(slideWithin(Rect{100, 5, 50, 60}, 20, 32, 318), 120, 5, 50, 60));
    CHECK(same(slideWithin(Rect{100, 5, 50, 60}, -20, 32, 318), 80, 5, 50, 60));
}

TEST(slide_stops_at_range_edges) {
    CHECK(same(slideWithin(Rect{40, 5, 50, 60}, -36, 32, 318), 32, 5, 50, 60));
    CHECK(same(slideWithin(Rect{260, 5, 50, 60}, 36, 32, 318), 268, 5, 50, 60));
}

TEST(slide_keeps_rect_wider_than_range_at_left_edge) {
    CHECK(same(slideWithin(Rect{32, 5, 300, 60}, 36, 32, 318), 32, 5, 300, 60));
}
