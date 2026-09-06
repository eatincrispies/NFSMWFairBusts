#pragma once

#include "Config.h"

namespace patch {

bool Verify(char* reasonOut, unsigned reasonSize);

bool Apply(const config::Settings& settings);

}
