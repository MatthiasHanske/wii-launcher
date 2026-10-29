#include "wii/fs_wii.h"

#include <dirent.h>
#include <fat.h>
#include <malloc.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "core/gpt.h"

namespace {

constexpr sec_t kGptEntriesLba = 2;
constexpr sec_t kGptEntriesSectors = 32;  // 16 KB

// Liest LBA 0, 1 und 2..33 und sucht die erste Partition. Nur für 512-Byte-Sektoren.
// readOk = false, wenn die Platte (noch) nicht lesbar war.
std::optional<uint64_t> probeUsbPartition(bool& readOk) {
    readOk = false;
    if (__io_usbstorage_sector_size != 512) {
        readOk = true;  // lesbar, aber für den Parser ungeeignet: libfat selbst suchen lassen
        return std::nullopt;
    }

    uint8_t* s0 = static_cast<uint8_t*>(memalign(32, 512));
    uint8_t* s1 = static_cast<uint8_t*>(memalign(32, 512));
    uint8_t* entries = static_cast<uint8_t*>(memalign(32, kGptEntriesSectors * 512));
    std::optional<uint64_t> lba;
    if (s0 && s1 && entries && __io_usbstorage.readSectors(0, 1, s0) &&
        __io_usbstorage.readSectors(1, 1, s1)) {
        readOk = true;
        // Die Eintragstabelle wird nur für GPT gebraucht; Lesefehler dort lassen den MBR-Weg offen.
        if (!__io_usbstorage.readSectors(kGptEntriesLba, kGptEntriesSectors, entries))
            std::memset(entries, 0, kGptEntriesSectors * 512);
        lba = findFirstPartitionLba(s0, s1, entries);
    }
    free(s0);
    free(s1);
    free(entries);
    return lba;
}

bool isDirPath(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string joinPath(const std::string& dir, const std::string& name) {
    if (!dir.empty() && dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

}  // namespace

bool WiiFs::mountSd() {
    if (sdMounted_) return true;
    if (!__io_wiisd.startup()) return false;
    sdMounted_ = fatMountSimple("sd", &__io_wiisd);
    return sdMounted_;
}

bool WiiFs::mountUsb(int retries, int delayMs) {
    if (usbMounted_) return true;
    for (int attempt = 0; attempt < retries; ++attempt) {
        if (attempt > 0) usleep(static_cast<useconds_t>(delayMs) * 1000);
        if (!__io_usbstorage.startup() || !__io_usbstorage.isInserted()) continue;

        bool readOk = false;
        std::optional<uint64_t> lba = probeUsbPartition(readOk);
        if (!readOk) continue;  // Platte noch nicht bereit
        if (lba && *lba <= UINT32_MAX) {
            if (fatMount("usb", &__io_usbstorage, static_cast<sec_t>(*lba), 8, 64)) {
                usbMounted_ = true;
                return true;
            }
            continue;
        }
        // Lesbar, aber keine Partition erkannt (z. B. Superfloppy): libfat einmal selbst
        // suchen lassen. Erneute Versuche ändern am Plattenlayout nichts.
        usbMounted_ = fatMountSimple("usb", &__io_usbstorage);
        return usbMounted_;
    }
    return false;
}

void WiiFs::unmountAll() {
    fatUnmount("sd:");
    fatUnmount("usb:");
    __io_usbstorage.shutdown();
    __io_wiisd.shutdown();
    sdMounted_ = false;
    usbMounted_ = false;
}

bool WiiFs::listDir(const std::string& path, std::vector<DirEntry>& out) {
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    while (struct dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        bool dir;
        if (e->d_type == DT_DIR)
            dir = true;
        else if (e->d_type == DT_REG)
            dir = false;
        else
            dir = isDirPath(joinPath(path, name));
        out.push_back({name, dir});
    }
    closedir(d);
    return true;
}

bool WiiFs::readBytes(const std::string& path, size_t offset, size_t len, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    bool ok = std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0;
    if (ok) {
        out.resize(len);
        size_t got = len ? std::fread(out.data(), 1, len, f) : 0;
        out.resize(got);
        ok = got == len;
    }
    std::fclose(f);
    return ok;
}

bool WiiFs::readText(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
}

bool WiiFs::writeText(const std::string& path, const std::string& text) {
    size_t slash = path.rfind('/');
    if (slash != std::string::npos) {
        std::string parent = path.substr(0, slash);
        // "sd:" (Wurzel ohne Schrägstrich) nicht anlegen.
        if (!parent.empty() && parent.back() != ':' && !isDirPath(parent)) mkdir(parent.c_str(), 0777);
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

bool WiiFs::exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}
