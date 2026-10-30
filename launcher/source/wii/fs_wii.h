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
    // Wie der letzte mountUsb-Aufruf ausging (Anzeige- und Logtext, UTF-8), z. B.
    // "eingehängt: MBR/GPT, fatMount auf LBA 2048 (…)".
    const std::string& lastMountInfo() const { return mountInfo_; }
    bool usbMounted() const { return usbMounted_; }
    // "usb:" aushängen und das USB-Gerät schließen; die SD bleibt. Harmlos, wenn nichts
    // eingehängt oder gestartet ist. Ein späteres mountUsb hängt die Platte neu ein.
    void unmountUsb();
    // SD und USB aushängen (vor Sprung, Wii-Menü, HBC, Ausschalten). Mehrfach aufrufbar.
    void unmountAll();

    bool listDir(const std::string& path, std::vector<DirEntry>& out) override;
    bool readBytes(const std::string& path, size_t offset, size_t len, std::vector<uint8_t>& out) override;
    bool readText(const std::string& path, std::string& out) override;
    bool writeText(const std::string& path, const std::string& text) override;
    bool exists(const std::string& path) override;

private:
    bool sdMounted_ = false;
    bool usbMounted_ = false;
    bool usbStarted_ = false;  // __io_usbstorage.startup() lief, Gerät evtl. offen
    std::string mountInfo_;
};
