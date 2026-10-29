#pragma once
#include <optional>
#include <string>

// Bereinigt einen Dateibasisnamen zu einem Anzeigetitel.
std::string cleanTitle(const std::string& raw);

// Sortierschluessel: klein, ohne fuehrenden Artikel, Umlaute und Akzente vereinfacht
// (wie searchKey), nur a-z0-9.
std::string sortKey(const std::string& title);

// Suchschluessel fuer Titel und Suchanfrage: klein, Umlaute/Akzente auf den Grundbuchstaben
// (ae->a, oe->o, ue->u, ss-Ligatur->ss, e/a/n/c mit Akzent -> e/a/n/c), Apostrophe entfernt,
// andere Satzzeichen als Leerzeichen, Leerzeichen zusammengefasst, vorn/hinten getrimmt.
// Ergebnis enthaelt nur a-z, 0-9 und einzelne Leerzeichen.
std::string searchKey(const std::string& text);

// Index-Buchstabe fuer die Alphabet-Leiste: 'A'..'Z' oder '#'.
char indexLetter(const std::string& title);

// 6-stellige Game-ID aus einem abschliessenden "[XXXXXX]" (A-Z, 0-9).
std::optional<std::string> extractGameId(const std::string& raw);
