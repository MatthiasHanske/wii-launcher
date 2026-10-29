#pragma once
#include <string>

#include "core/library.h"

struct Settings {
    SystemSet systems = SystemSet::defaults();
    std::string extraRoot;  // leer oder z. B. sd:/launcher/mock
};

// Format: key=value je Zeile (systems=wii,gc,n64 / extra_root=...).
Settings parseSettings(const std::string& text);
std::string serializeSettings(const Settings& s);
