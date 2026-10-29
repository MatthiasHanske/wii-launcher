#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct DirEntry {
    std::string name;
    bool isDir;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    // Direkte Kinder von path. false, wenn der Ordner nicht existiert.
    virtual bool listDir(const std::string& path, std::vector<DirEntry>& out) = 0;
    virtual bool readBytes(const std::string& path, size_t offset, size_t len, std::vector<uint8_t>& out) = 0;
    virtual bool readText(const std::string& path, std::string& out) = 0;
    virtual bool writeText(const std::string& path, const std::string& text) = 0;
    virtual bool exists(const std::string& path) = 0;
};
