#include "Config.h"
#include "Game.h"
#include "Log.h"
#include "Patch.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace {

config::Settings g_settings;
char             g_directory[MAX_PATH] = "";
char             g_iniPath[MAX_PATH]   = "";

void ResolvePaths(HMODULE module) {
    char modulePath[MAX_PATH] = "";
    GetModuleFileNameA(module, modulePath, sizeof(modulePath));
    modulePath[sizeof(modulePath) - 1] = '\0';

    _snprintf(g_directory, sizeof(g_directory), "%s", modulePath);
    g_directory[sizeof(g_directory) - 1] = '\0';

    char* lastSlash = strrchr(g_directory, '\\');
    if (lastSlash != nullptr) *lastSlash = '\0';

    _snprintf(g_iniPath, sizeof(g_iniPath), "%s\\NFSMWFairBusts.ini", g_directory);
    g_iniPath[sizeof(g_iniPath) - 1] = '\0';
}

DWORD WINAPI Startup(LPVOID parameter) {
    ResolvePaths(static_cast<HMODULE>(parameter));
    config::Load(g_iniPath, g_settings);

    if (!g_settings.enabled) {
        logger::Write(g_directory, "Mod not applied: disabled in NFSMWFairBusts.ini.");
        return 0;
    }

    char hostPath[MAX_PATH] = "";
    char md5[game::kMd5TextSize] = "";
    if (!game::GetHostPath(hostPath, sizeof(hostPath)) ||
        !game::ComputeMd5(hostPath, md5, sizeof(md5))) {
        logger::Write(g_directory, "Mod not applied: could not hash the host executable.");
        return 0;
    }

    char reason[256] = "";
    if (!game::VerifyImage(reason, sizeof(reason)) || !patch::Verify(reason, sizeof(reason))) {
        logger::Write(g_directory,
                      "Mod not applied: %s does not look like Most Wanted v1.3 (%s).",
                      md5, reason);
        return 0;
    }

    if (!patch::Apply(g_settings)) {
        logger::Write(g_directory, "Mod not applied: could not write to the pursuit code.");
        return 0;
    }

    logger::Write(g_directory, "Mod injected and applied to v1.3 and %s", md5);
    return 0;
}

}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, Startup, module, 0, nullptr);
        if (thread != nullptr) CloseHandle(thread);
    }
    return TRUE;
}
