#include "Log.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

namespace logger {

void Write(const char* asiDirectory, const char* fmt, ...) {
    char path[MAX_PATH];
    _snprintf(path, sizeof(path), "%s\\NFSMWFairBusts.log", asiDirectory);
    path[sizeof(path) - 1] = '\0';

    FILE* file = fopen(path, "w");
    if (file == nullptr) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);

    fputc('\n', file);
    fclose(file);
}

}
