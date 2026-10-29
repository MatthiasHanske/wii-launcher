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

// --- carouselGeometry ----------------------------------------------------------------------

// Linke Spalte bei 640 (x 32, Breite 286) und 854 (Breite 397), Vorschau 224 hoch ab y 112.
static const Rect kCol640{32, 112, 286, 224};
static const Rect kCol854{32, 112, 397, 224};

static bool inside(const Rect& r, const Rect& col) {
    return r.x >= col.x && r.x + r.w <= col.x + col.w && r.y >= col.y && r.y + r.h <= col.y + col.h;
}

TEST(carousel_portrait_center_shows_both_sides_in_column) {
    Rect center = fitRect(160, 224, kCol640);
    CarouselGeometry g = carouselGeometry(kCol640, center, 88, 123);
    CHECK(!g.left.empty());
    CHECK(!g.right.empty());
    CHECK(inside(g.left, kCol640));
    CHECK(inside(g.right, kCol640));
    CHECK(g.left.x < center.x);
    CHECK(g.right.x + g.right.w > center.x + center.w);
    CHECK_EQ(g.left.y, 163);  // 112 + 50.5, gerundet
    // Klickflächen: von der Spaltenkante bis zur Mitte, volle Höhe.
    CHECK(same(g.hitLeft, 32, 112, center.x - 32, 224));
    CHECK(same(g.hitRight, center.x + center.w, 112, 32 + 286 - (center.x + center.w), 224));
}

TEST(carousel_sides_follow_center_width) {
    Rect narrow = fitRect(160, 224, kCol854);
    Rect wide = fitRect(240, 224, kCol854);
    CarouselGeometry a = carouselGeometry(kCol854, narrow, 88, 123);
    CarouselGeometry b = carouselGeometry(kCol854, wide, 88, 123);
    CHECK(b.left.x < a.left.x);
    CHECK(b.right.x > a.right.x);
    CHECK(inside(b.left, kCol854));
    CHECK(inside(b.right, kCol854));
    // Abstand zur Mitte höchstens 14 px.
    CHECK(narrow.x - (a.left.x + a.left.w) <= 14);
}

TEST(carousel_wide_center_pushes_sides_behind_it) {
    Rect center = fitRect(312, 228, kCol854);  // 306 x 224
    CarouselGeometry g = carouselGeometry(kCol854, center, 88, 123);
    CHECK(!g.left.empty());
    CHECK_EQ(g.left.x, 32);
    CHECK(g.left.x + g.left.w > center.x);  // teilweise hinter der Mitte
    CHECK(inside(g.left, kCol854));
    CHECK(inside(g.right, kCol854));
    CHECK_EQ(g.right.x + g.right.w, 32 + 397);
}

TEST(carousel_center_filling_column_hides_sides) {
    Rect center = fitRect(312, 228, kCol640);  // 286 x 209, ganze Spalte
    CarouselGeometry g = carouselGeometry(kCol640, center, 88, 123);
    CHECK(g.left.empty());
    CHECK(g.right.empty());
    CHECK(g.hitLeft.empty());
    CHECK(g.hitRight.empty());
}

TEST(carousel_hides_side_with_only_a_sliver_visible) {
    Rect center{32 + 10, 112, 286 - 20, 224};  // je 10 px frei
    CarouselGeometry g = carouselGeometry(kCol640, center, 88, 123);
    CHECK(g.left.empty());
    CHECK(g.right.empty());
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
