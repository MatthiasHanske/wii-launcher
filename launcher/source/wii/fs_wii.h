#pragma once
#include "core/fs.h"

// IFileSystem über libfat (stdio/dirent). Pfade wie "sd:/launcher/recent.txt" oder "usb:/wbfs".
class WiiFs : public IFileSystem {
public:
    // SD-Karte im Frontslot als "sd:" einhängen.
    bool mountSd();
    // USB-Festplatte als "usb:" einhängen. Erkennt MBR und GPT; versucht es retries-mal
    // mit delayMs Pause (Platten brauchen nach dem Start oft einige Sekunden).
    bool mountUsb(int retries, int delayMs);
    void unmountAll();

    bool listDir(const std::string& path, std::vector<DirEntry>& out) override;
    bool readBytes(const std::string& path, size_t offset, size_t len, std::vector<uint8_t>& out) override;
    bool readText(const std::string& path, std::string& out) override;
    bool writeText(const std::string& path, const std::string& text) override;
    bool exists(const std::string& path) override;

private:
    bool sdMounted_ = false;
    bool usbMounted_ = false;
};
