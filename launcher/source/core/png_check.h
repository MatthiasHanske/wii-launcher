#pragma once
// Strukturprüfung einer PNG-Datei im Speicher, bevor sie an libpng/PNGU geht.
// PNGU liest aus einem Puffer ohne Längenangabe; eine abgeschnittene oder kaputte Datei
// könnte libpng über das Pufferende hinaus lesen lassen. checkPng läuft deshalb alle
// Chunks ab und verlangt, dass jeder vollständig im Puffer liegt und IEND vorkommt.
// CRCs prüft weiterhin libpng.
#include <cstddef>
#include <cstdint>

struct PngInfo {
    uint32_t width = 0;
    uint32_t height = 0;
};

// Maximale Kantenlänge einer GX-Textur.
constexpr uint32_t kPngMaxSide = 1024;

// true: Signatur stimmt, erster Chunk ist ein IHDR (13 Byte) mit 1..kPngMaxSide als
// Breite/Höhe, alle Chunks liegen im Puffer, ein IEND schließt ab. info nur dann gesetzt.
bool checkPng(const uint8_t* data, size_t size, PngInfo& info);
