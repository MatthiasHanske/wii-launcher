#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/fs.h"

class FakeFs : public IFileSystem {
public:
    void addFile(const std::string& path, const std::vector<uint8_t>& bytes = {}) {
        std::string p = norm(path);
        files_[p] = bytes;
        addParents(p);
    }
    void addFile(const std::string& path, const std::string& text) {
        addFile(path, std::vector<uint8_t>(text.begin(), text.end()));
    }
    void addDir(const std::string& path) {
        std::string p = norm(path);
        dirs_.insert(p);
        addParents(p);
    }

    bool listDir(const std::string& path, std::vector<DirEntry>& out) override {
        std::string p = norm(path);
        if (!dirs_.count(p)) return false;
        std::string prefix = p + "/";
        for (const auto& d : dirs_) collect(d, prefix, true, out);
        for (const auto& f : files_) collect(f.first, prefix, false, out);
        return true;
    }
    bool readBytes(const std::string& path, size_t offset, size_t len, std::vector<uint8_t>& out) override {
        auto it = files_.find(norm(path));
        if (it == files_.end()) return false;
        out.clear();
        const auto& b = it->second;
        for (size_t i = offset; i < b.size() && out.size() < len; ++i) out.push_back(b[i]);
        return true;
    }
    bool readText(const std::string& path, std::string& out) override {
        auto it = files_.find(norm(path));
        if (it == files_.end()) return false;
        out.assign(it->second.begin(), it->second.end());
        return true;
    }
    bool writeText(const std::string& path, const std::string& text) override {
        addFile(path, text);
        return true;
    }
    bool exists(const std::string& path) override {
        std::string p = norm(path);
        return files_.count(p) || dirs_.count(p);
    }

private:
    static std::string norm(std::string p) {
        while (p.size() > 1 && p.back() == '/') p.pop_back();
        return p;
    }
    void addParents(const std::string& p) {
        std::string cur = p;
        for (;;) {
            size_t pos = cur.rfind('/');
            if (pos == std::string::npos || pos == 0) break;
            cur = cur.substr(0, pos);
            dirs_.insert(cur);
        }
    }
    static void collect(const std::string& full, const std::string& prefix, bool isDir, std::vector<DirEntry>& out) {
        if (full.size() <= prefix.size() || full.compare(0, prefix.size(), prefix) != 0) return;
        std::string rest = full.substr(prefix.size());
        if (rest.find('/') != std::string::npos) return;
        out.push_back({rest, isDir});
    }

    std::map<std::string, std::vector<uint8_t>> files_;
    std::set<std::string> dirs_;
};
