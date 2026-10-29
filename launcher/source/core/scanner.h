#pragma once
#include <string>
#include <vector>

#include "core/fs.h"
#include "core/model.h"

// Scannt die USB-Bibliothek unter root (z. B. "usb:/"). Ergebnis nach sortKey sortiert.
std::vector<Game> scanLibrary(IFileSystem& fs, const std::string& root);
