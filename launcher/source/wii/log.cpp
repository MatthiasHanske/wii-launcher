#include "wii/log.h"

#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <sys/stat.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

#ifndef BUILD_ID
#define BUILD_ID "dev"
#endif

namespace diag {

namespace {

constexpr char kDir[] = "sd:/launcher";
constexpr char kPath[] = "sd:/launcher/launcher.log";
constexpr long kMaxSize = 200 * 1024;

enum class State { Pending, Open, Closed };
State gState = State::Pending;
std::string gPending;  // Zeilen vor logInit
uint64_t gStartMs = 0;

uint64_t nowMs() { return ticks_to_millisecs(gettime()); }

bool append(const std::string& text) {
    FILE* f = std::fopen(kPath, "a");
    if (!f) return false;
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = std::fflush(f) == 0 && ok;
    ok = std::fclose(f) == 0 && ok;
    return ok;
}

}  // namespace

void logInit() {
    if (gState != State::Pending) return;
    struct stat st;
    if (stat(kDir, &st) != 0) mkdir(kDir, 0777);
    if (stat(kPath, &st) == 0 && st.st_size > kMaxSize) {
        if (FILE* f = std::fopen(kPath, "w")) std::fclose(f);
    }
    char when[32] = "?";
    std::time_t t = std::time(nullptr);
    if (std::tm* tm = std::localtime(&t)) std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", tm);
    char head[128];
    std::snprintf(head, sizeof head, "\n=== Launcher-Start %s, Build %s ===\n", when, BUILD_ID);
    gState = State::Open;
    append(head + gPending);
    gPending.clear();
    gPending.shrink_to_fit();
}

void logf(const char* fmt, ...) {
    if (gState == State::Closed) return;
    if (gStartMs == 0) gStartMs = nowMs();
    char buf[1024];
    int n = std::snprintf(buf, sizeof buf, "[%7llu] ", static_cast<unsigned long long>(nowMs() - gStartMs));
    if (n < 0) n = 0;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf + n, sizeof buf - static_cast<size_t>(n), fmt, ap);
    va_end(ap);
    std::string line = buf;
    line += '\n';
    if (gState == State::Pending) gPending += line;
    else append(line);
}

void logClose() { gState = State::Closed; }

}  // namespace diag
