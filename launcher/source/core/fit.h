#pragma once
// Geometrie für Cover: Einpassen mit erhaltenem Seitenverhältnis und Fächer-Anordnung.
// Plattformfrei (Host-Tests), Koordinaten in logischen Pixeln wie in wii/gfx.h.

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    bool empty() const { return w <= 0 || h <= 0; }
};

// Größtmögliches Rechteck mit dem Seitenverhältnis imgW:imgH innerhalb bounds, zentriert,
// auf ganze Pixel gerundet. Leeres Rect, wenn imgW, imgH oder bounds nicht positiv sind.
Rect fitRect(float imgW, float imgH, const Rect& bounds);

// Fächer: area = Fächerfläche, center = tatsächliches Rechteck des Mittelcovers darin,
// sideFrames = Rahmen der Stufen 1 und 2 (nur w/h). Je Seite liegt Stufe 1 kGapCenter neben
// der Mitte, Stufe 2 kGapSide neben Stufe 1, alle senkrecht mittig. Fehlt Platz, wird zuerst
// der äußere Abstand enger (Stufe 2 bis kMaxOverlap hinter Stufe 1), dann der innere; reicht
// das nicht, entfällt Stufe 2. Stufe 1 wird dann an den Flächenrand geschoben (liegt hinter
// der Mitte, die zuletzt gezeichnet wird) und entfällt, wenn weniger als kMinVisible sichtbar
// bleiben. Klickflächen: volle Flächenhöhe, lückenlos von der Mitte nach außen, die äußerste
// vorhandene Stufe bis zum Flächenrand.
struct FanGeometry {
    static constexpr float kGapCenter = 14, kGapSide = 10, kMaxOverlap = 8, kMinVisible = 16;
    Rect left[2], right[2];        // [0] = Stufe 1 (direkt neben der Mitte), [1] = Stufe 2; leer = entfällt
    Rect hitLeft[2], hitRight[2];  // Rahmenbreite x volle area-Höhe; äußerste vorhandene Stufe bis zum Rand erweitert
};
FanGeometry fanGeometry(const Rect& area, const Rect& center, const Rect sideFrames[2]);

// r waagerecht um dx verschieben, aber innerhalb [lo, hi] halten (Gleiten des Fächers).
// Ist r breiter als der Bereich, bleibt es an lo.
Rect slideWithin(Rect r, float dx, float lo, float hi);
