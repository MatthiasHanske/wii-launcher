#pragma once
// Geometrie für Cover: Einpassen mit erhaltenem Seitenverhältnis und Karussell-Anordnung.
// Plattformfrei (Host-Tests), Koordinaten in logischen Pixeln wie in wii/gfx.h.

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    bool empty() const { return w <= 0 || h <= 0; }
};

// Größtmögliches Rechteck mit dem Seitenverhältnis imgW:imgH innerhalb bounds, zentriert,
// auf ganze Pixel gerundet. Leeres Rect, wenn imgW, imgH oder bounds nicht positiv sind.
Rect fitRect(float imgW, float imgH, const Rect& bounds);

// Karussell in der linken Spalte: column = Vorschaufläche (Spaltenbreite x Vorschauhöhe),
// center = tatsächliches Rechteck des Mittelcovers darin. Die Seitenrechtecke (sideW x sideH,
// senkrecht mittig) liegen mit höchstens kMaxGap Abstand neben der Mitte, dürfen bis
// kMaxOverlap hinter sie rücken und werden an den Spaltenrand geschoben, wenn außen kein
// Platz ist (dann liegen sie weiter hinter der Mitte, die zuletzt gezeichnet wird). Bleiben
// weniger als kMinVisible Pixel sichtbar, ist die Seite leer (nicht zeichnen, keine Klickfläche).
// hitLeft/hitRight: von der Spaltenkante bis zur Mitte, volle Vorschauhöhe.
struct CarouselGeometry {
    static constexpr float kMaxGap = 14;
    static constexpr float kMaxOverlap = 8;
    static constexpr float kMinVisible = 16;
    Rect left, right;
    Rect hitLeft, hitRight;
};
CarouselGeometry carouselGeometry(const Rect& column, const Rect& center, float sideW, float sideH);

// r waagerecht um dx verschieben, aber innerhalb [lo, hi] halten (Gleiten des Karussells).
// Ist r breiter als der Bereich, bleibt es an lo.
Rect slideWithin(Rect r, float dx, float lo, float hi);
