#include "Config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace config {
namespace {

constexpr size_t kMaxLine = 512;

char* SkipSpace(char* text) {
    while (*text == ' ' || *text == '\t') ++text;
    return text;
}

void TrimTrailing(char* text) {
    size_t length = strlen(text);
    while (length > 0) {
        const char c = text[length - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        text[--length] = '\0';
    }
}

void StripComment(char* text) {
    for (char* cursor = text; *cursor; ++cursor) {
        if (*cursor == ';' || *cursor == '#') {
            *cursor = '\0';
            return;
        }
        if (cursor[0] == '/' && cursor[1] == '/') {
            *cursor = '\0';
            return;
        }
    }
}

bool EqualsIgnoreCase(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return *a == '\0' && *b == '\0';
}

bool ParseBool(const char* value, bool& out) {
    if (EqualsIgnoreCase(value, "1") || EqualsIgnoreCase(value, "true") ||
        EqualsIgnoreCase(value, "yes") || EqualsIgnoreCase(value, "on")) {
        out = true;
        return true;
    }
    if (EqualsIgnoreCase(value, "0") || EqualsIgnoreCase(value, "false") ||
        EqualsIgnoreCase(value, "no") || EqualsIgnoreCase(value, "off")) {
        out = false;
        return true;
    }
    return false;
}

bool ParseFloat(const char* value, float& out) {
    char* end = nullptr;
    const double parsed = strtod(value, &end);
    if (end == value) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != '\0') return false;

    if (parsed < 0.0)  out = 0.0f;
    else if (parsed > 64.0) out = 64.0f;
    else out = static_cast<float>(parsed);
    return true;
}

void Assign(Settings& out, const char* section, const char* key, const char* value) {
    if (!EqualsIgnoreCase(section, "General")) return;

    if (EqualsIgnoreCase(key, "Enabled")) ParseBool(value, out.enabled);
    else if (EqualsIgnoreCase(key, "ResetBustMultiplier")) ParseFloat(value, out.resetBustMultiplier);
    else if (EqualsIgnoreCase(key, "ResetAutoBust")) ParseBool(value, out.resetAutoBust);
}

}

bool Load(const char* path, Settings& out) {
    FILE* file = fopen(path, "r");
    if (file == nullptr) return false;

    char section[64] = "";
    char line[kMaxLine];

    while (fgets(line, sizeof(line), file) != nullptr) {
        StripComment(line);
        TrimTrailing(line);

        char* cursor = SkipSpace(line);
        if (*cursor == '\0') continue;

        if (*cursor == '[') {
            char* close = strchr(cursor, ']');
            if (close == nullptr) continue;
            *close = '\0';
            _snprintf(section, sizeof(section), "%s", SkipSpace(cursor + 1));
            section[sizeof(section) - 1] = '\0';
            TrimTrailing(section);
            continue;
        }

        char* separator = strchr(cursor, '=');
        if (separator == nullptr) continue;
        *separator = '\0';

        char* key = cursor;
        TrimTrailing(key);

        char* value = SkipSpace(separator + 1);
        TrimTrailing(value);
        if (*key == '\0' || *value == '\0') continue;

        Assign(out, section, key, value);
    }

    fclose(file);
    return true;
}

}
