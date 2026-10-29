#pragma once
#include <optional>
#include <string>

// Bereinigt einen Dateibasisnamen zu einem Anzeigetitel.
std::string cleanTitle(const std::string& raw);

// Sortierschluessel: klein, ohne fuehrenden Artikel, Umlaute vereinfacht, nur a-z0-9.
std::string sortKey(const std::string& title);

// Index-Buchstabe fuer die Alphabet-Leiste: 'A'..'Z' oder '#'.
char indexLetter(const std::string& title);

// 6-stellige Game-ID aus einem abschliessenden "[XXXXXX]" (A-Z, 0-9).
std::optional<std::string> extractGameId(const std::string& raw);
