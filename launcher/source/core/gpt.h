#pragma once
#include <cstdint>
#include <optional>

// Erste nutzbare Partition einer Festplatte finden (reines Byte-Parsing, little-endian).
// sector0/sector1: je 512 Bytes (LBA 0 und 1); entries: 16 KB ab LBA 2 (GPT-Eintragstabelle).
// MBR mit FAT-Typ (0x0B/0x0C/0x06/0x0E/0x07) wird bevorzugt; bei Schutz-MBR (0xEE)
// oder ohne FAT-Eintrag wird die GPT ausgewertet. nullopt, wenn nichts Gültiges gefunden wird.
std::optional<uint64_t> findFirstPartitionLba(const uint8_t* sector0, const uint8_t* sector1,
                                              const uint8_t* entries);
