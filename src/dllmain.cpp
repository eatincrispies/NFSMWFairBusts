#include <Windows.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace FairBusts {

    HMODULE g_hModule = nullptr;

    namespace Offsets {

        struct Pattern {
            std::array<std::uint8_t, 128> bytes{};
            std::array<bool, 128>         wildcard{};
            std::size_t                   size = 0;
        };

        inline void RejectPattern() {}

        consteval std::uint8_t ParseNibble(char c) {
            if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
            if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
            RejectPattern();
            return 0;
        }

        consteval Pattern ParsePattern(std::string_view text) {
            Pattern pattern{};
            for (std::size_t i = 0; i < text.size();) {
                if (text[i] == ' ') {
                    ++i;
                    continue;
                }
                if (i + 1 >= text.size() || pattern.size == pattern.bytes.size()) RejectPattern();
                if (text[i] == '?') {
                    pattern.wildcard[pattern.size] = true;
                } else {
                    pattern.bytes[pattern.size] =
                        static_cast<std::uint8_t>(ParseNibble(text[i]) << 4 | ParseNibble(text[i + 1]));
                }
                ++pattern.size;
                i += 2;
            }
            return pattern;
        }

        inline constexpr Pattern kResetPenaltyBlock = ParsePattern(
            "83 F8 01 75 02 8A D8 84 DB "
            "D9 05 ?? ?? ?? ?? 74 0A DD D8 "
            "D9 05 ?? ?? ?? ?? EB 16 "
            "8B 8E B4 00 00 00 E8 ?? ?? ?? ?? "
            "D8 5C 24 2C DF E0 F6 C4 05 7A 31 "
            "D9 44 24 1C D8 D9 DF E0 DD D8 F6 C4 05 7A 24 "
            "84 DB D9 44 24 5C D8 0D ?? ?? ?? ?? D9 96 20 01 00 00 74 22 "
            "D8 0D ?? ?? ?? ??");

        inline constexpr std::ptrdiff_t kNormalRangeLoad = 0x09;
        inline constexpr std::ptrdiff_t kResetRangeLoad  = 0x13;
        inline constexpr std::ptrdiff_t kSpeedGateSkip   = 0x19;
        inline constexpr std::ptrdiff_t kResetRateScale  = 0x54;
        inline constexpr std::ptrdiff_t kOperand         = 0x02;

        inline constexpr float kNormalRange = 15.0f;
        inline constexpr float kResetRange  = 90.0f;
        inline constexpr float kResetRate   = 4.0f;

        inline constexpr std::array<std::uint8_t, 2> kSpeedGateNops{ 0x90, 0x90 };

    }

    namespace Memory {

        bool IsReadable(const void* address, std::size_t size) noexcept {
            if (address == nullptr || size == 0) return false;

            constexpr DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                       PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

            auto cursor    = reinterpret_cast<std::uintptr_t>(address);
            const auto end = cursor + size;
            if (end < cursor) return false;

            while (cursor < end) {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) return false;
                if (info.State != MEM_COMMIT) return false;
                if ((info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 || (info.Protect & readable) == 0) return false;
                cursor = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
            }
            return true;
        }

        bool SafeCopy(void* destination, const void* source, std::size_t size) noexcept {
            if (destination == nullptr || source == nullptr) return false;
#if defined(_MSC_VER)
            __try {
                std::memcpy(destination, source, size);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
            return true;
#else
            if (!IsReadable(source, size)) return false;
            std::memcpy(destination, source, size);
            return true;
#endif
        }

        template <class T>
        std::optional<T> Read(std::uintptr_t address) noexcept {
            T value{};
            if (!SafeCopy(&value, reinterpret_cast<const void*>(address), sizeof(T))) return std::nullopt;
            return value;
        }

        bool Write(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept {
            auto* const target = reinterpret_cast<void*>(address);

            DWORD previous = 0;
            if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &previous)) return false;

            std::memcpy(target, bytes.data(), bytes.size());

            DWORD ignored = 0;
            VirtualProtect(target, bytes.size(), previous, &ignored);
            FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
            return true;
        }

        std::array<std::uint8_t, 4> PointerBytes(std::uintptr_t pointer) noexcept {
            return std::bit_cast<std::array<std::uint8_t, 4>>(static_cast<std::uint32_t>(pointer));
        }

    }

    namespace Scan {

        std::uintptr_t HostModule() noexcept {
            return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        }

        bool Matches(const std::uint8_t* at, const Offsets::Pattern& pattern) noexcept {
            for (std::size_t i = 0; i < pattern.size; ++i) {
                if (!pattern.wildcard[i] && at[i] != pattern.bytes[i]) return false;
            }
            return true;
        }

        std::optional<std::uintptr_t> FindUnique(const Offsets::Pattern& pattern,
                                                 std::uintptr_t module = HostModule()) noexcept {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
            if (!Memory::IsReadable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return std::nullopt;

            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
            if (!Memory::IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) return std::nullopt;

            std::optional<std::uintptr_t> found;
            const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);

            for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
                if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;

                const auto* begin = reinterpret_cast<const std::uint8_t*>(module + section->VirtualAddress);
                const std::size_t length = section->Misc.VirtualSize;
                if (length < pattern.size || !Memory::IsReadable(begin, length)) continue;

                for (std::size_t offset = 0; offset + pattern.size <= length; ++offset) {
                    if (!Matches(begin + offset, pattern)) continue;
                    if (found) return std::nullopt;
                    found = reinterpret_cast<std::uintptr_t>(begin + offset);
                }
            }
            return found;
        }

    }

    class ScopedPatch {
    public:
        ScopedPatch(std::uintptr_t address, std::span<const std::uint8_t> bytes) noexcept {
            if (bytes.empty() || bytes.size() > original_.size()) return;
            if (!Memory::SafeCopy(original_.data(), reinterpret_cast<const void*>(address), bytes.size())) return;
            if (!Memory::Write(address, bytes)) return;

            address_ = address;
            size_    = bytes.size();
        }

        ~ScopedPatch() {
            if (size_ != 0) static_cast<void>(Memory::Write(address_, std::span{ original_.data(), size_ }));
        }

        ScopedPatch(const ScopedPatch&)            = delete;
        ScopedPatch& operator=(const ScopedPatch&) = delete;

        explicit operator bool() const noexcept { return size_ != 0; }

    private:
        std::uintptr_t              address_ = 0;
        std::array<std::uint8_t, 8> original_{};
        std::size_t                 size_ = 0;
    };

    struct Settings {
        bool  enabled             = true;
        bool  resetAutoBust       = false;
        float resetBustMultiplier = 1.0f;
    };

    struct Patches {
        std::optional<ScopedPatch> resetRange;
        std::optional<ScopedPatch> speedGate;
        std::optional<ScopedPatch> resetRate;

        [[nodiscard]] bool AllApplied() const noexcept {
            return Applied(resetRange) && Applied(speedGate) && Applied(resetRate);
        }

    private:
        static bool Applied(const std::optional<ScopedPatch>& patch) noexcept { return !patch || *patch; }
    };

    Patches g_patches;
    float   g_resetRate = Offsets::kResetRate;

    std::string ModuleDirectory() {
        std::array<char, MAX_PATH> path{};
        GetModuleFileNameA(g_hModule, path.data(), static_cast<DWORD>(path.size()));

        std::string directory{ path.data() };
        if (const auto slash = directory.find_last_of("\\/"); slash != std::string::npos) directory.resize(slash);
        return directory;
    }

    std::string ReadIni(const std::string& ini, const char* key) {
        std::array<char, 32> buffer{};
        GetPrivateProfileStringA("General", key, "", buffer.data(), static_cast<DWORD>(buffer.size()), ini.c_str());

        std::string value{ buffer.data() };
        std::ranges::transform(value, value.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool ParseBool(const std::string& value, bool fallback) {
        if (value == "1" || value == "true" || value == "yes" || value == "on") return true;
        if (value == "0" || value == "false" || value == "no" || value == "off") return false;
        return fallback;
    }

    Settings LoadSettings(const std::string& ini) {
        Settings settings{};
        settings.enabled       = ParseBool(ReadIni(ini, "Enabled"), settings.enabled);
        settings.resetAutoBust = ParseBool(ReadIni(ini, "ResetAutoBust"), settings.resetAutoBust);

        if (const std::string rate = ReadIni(ini, "ResetBustMultiplier"); !rate.empty()) {
            settings.resetBustMultiplier = std::clamp(std::strtof(rate.c_str(), nullptr), 0.0f, 64.0f);
        }
        return settings;
    }

    std::string HostMd5() {
        std::array<char, MAX_PATH> path{};
        if (GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size())) == 0) return "unknown";

        const HANDLE file = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return "unknown";

        std::string digest = "unknown";
        HCRYPTPROV provider = 0;
        HCRYPTHASH hash     = 0;

        if (CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) &&
            CryptCreateHash(provider, CALG_MD5, 0, 0, &hash)) {
            std::array<BYTE, 32768> chunk{};
            DWORD read = 0;
            bool ok = true;

            while (ok && ReadFile(file, chunk.data(), static_cast<DWORD>(chunk.size()), &read, nullptr) && read != 0) {
                ok = CryptHashData(hash, chunk.data(), read, 0) != 0;
            }

            std::array<BYTE, 16> bytes{};
            DWORD size = static_cast<DWORD>(bytes.size());
            if (ok && CryptGetHashParam(hash, HP_HASHVAL, bytes.data(), &size, 0) && size == bytes.size()) {
                digest.clear();
                for (const BYTE b : bytes) {
                    std::array<char, 3> hex{};
                    std::snprintf(hex.data(), hex.size(), "%02X", b);
                    digest += hex.data();
                }
            }
        }

        if (hash != 0) CryptDestroyHash(hash);
        if (provider != 0) CryptReleaseContext(provider, 0);
        CloseHandle(file);
        return digest;
    }

    void WriteLog(const std::string& line) {
        const std::string path = ModuleDirectory() + "\\NFSMWFairBusts.log";
        if (FILE* file = std::fopen(path.c_str(), "w")) {
            std::fputs(line.c_str(), file);
            std::fputc('\n', file);
            std::fclose(file);
        }
    }

    std::optional<std::uintptr_t> OperandTarget(std::uintptr_t instruction) {
        return Memory::Read<std::uint32_t>(instruction + Offsets::kOperand);
    }

    bool PointsAt(std::optional<std::uintptr_t> pointer, float expected) {
        return pointer && Memory::Read<float>(*pointer) == expected;
    }

    void Shutdown() {
        g_patches.resetRate.reset();
        g_patches.speedGate.reset();
        g_patches.resetRange.reset();
    }

    bool Initialize() {
        const Settings settings = LoadSettings(ModuleDirectory() + "\\NFSMWFairBusts.ini");
        if (!settings.enabled) {
            WriteLog("Mod not applied: disabled in NFSMWFairBusts.ini.");
            return false;
        }

        const std::string md5 = HostMd5();
        const auto block = Scan::FindUnique(Offsets::kResetPenaltyBlock);
        if (!block) {
            WriteLog("Mod not applied: " + md5 + " does not look like Most Wanted v1.3.");
            return false;
        }

        const std::uintptr_t normalRangeLoad = *block + Offsets::kNormalRangeLoad;
        const std::uintptr_t resetRangeLoad  = *block + Offsets::kResetRangeLoad;
        const std::uintptr_t resetRateScale  = *block + Offsets::kResetRateScale;

        const auto normalRange = OperandTarget(normalRangeLoad);
        if (!PointsAt(normalRange, Offsets::kNormalRange) ||
            !PointsAt(OperandTarget(resetRangeLoad), Offsets::kResetRange) ||
            !PointsAt(OperandTarget(resetRateScale), Offsets::kResetRate)) {
            WriteLog("Mod not applied: the pursuit code has already been changed by another plugin.");
            return false;
        }

        if (!settings.resetAutoBust) {
            g_patches.resetRange.emplace(resetRangeLoad + Offsets::kOperand, Memory::PointerBytes(*normalRange));
            g_patches.speedGate.emplace(*block + Offsets::kSpeedGateSkip, Offsets::kSpeedGateNops);
        }

        if (settings.resetBustMultiplier != Offsets::kResetRate) {
            g_resetRate = settings.resetBustMultiplier;
            g_patches.resetRate.emplace(resetRateScale + Offsets::kOperand,
                                        Memory::PointerBytes(reinterpret_cast<std::uintptr_t>(&g_resetRate)));
        }

        if (!g_patches.AllApplied()) {
            Shutdown();
            WriteLog("Mod not applied: could not write to the pursuit code.");
            return false;
        }

        WriteLog("Mod injected and applied to v1.3 and " + md5);
        return true;
    }

    void ReportCrash() {
        WriteLog("Mod not applied: startup failed unexpectedly.");
    }

}

DWORD __stdcall InitThread(LPVOID) {
#if defined(_MSC_VER)
    __try {
        FairBusts::Initialize();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        FairBusts::ReportCrash();
    }
#else
    FairBusts::Initialize();
#endif
    return 0;
}

BOOL __stdcall DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        FairBusts::g_hModule = hModule;
        if (const HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr)) CloseHandle(thread);
        break;

    case DLL_PROCESS_DETACH:
        FairBusts::Shutdown();
        break;

    default:
        break;
    }
    return TRUE;
}
