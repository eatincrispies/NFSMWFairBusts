#pragma once

namespace config {

struct Settings {
    bool  enabled             = true;
    float resetBustMultiplier = 1.0f;
    bool  resetAutoBust       = false;
};

bool Load(const char* path, Settings& out);

}
