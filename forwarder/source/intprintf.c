// libogc ruft intern printf/sprintf/snprintf auf (system.c, wiilaunch.c, usb.c). Die
// newlib-Varianten mit Gleitkomma ziehen _vfprintf_r, _svfprintf_r und dtoa nach (~40 KB).
// Das Makefile leitet die drei Funktionen per -Wl,--wrap auf die Integer-Varianten um,
// damit forwarder.dol unter 300 KB bleibt. libogc formatiert dort keine Gleitkommazahlen.
#include <stdarg.h>
#include <stdio.h>

int __wrap_printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = viprintf(fmt, ap);
    va_end(ap);
    return r;
}

int __wrap_sprintf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsiprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

int __wrap_snprintf(char* buf, size_t n, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsniprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}
