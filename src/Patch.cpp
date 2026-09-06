#include "Patch.h"

#include "Game.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace patch {
namespace {

const uint8_t kFlagSignature[] = {
    0x83, 0xF8, 0x01,
    0x75, 0x02,
    0x8A, 0xD8,
    0x84, 0xDB,
};

constexpr size_t kFlagSignatureSize = sizeof(kFlagSignature);

const uint8_t kFldOpcode[]  = { 0xD9, 0x05 };
const uint8_t kFmulOpcode[] = { 0xD8, 0x0D };
const uint8_t kSpeedGateSkipBytes[] = { 0xEB, 0x16 };
const uint8_t kNops[] = { 0x90, 0x90 };

constexpr size_t kOperandOffset = 2;

constexpr float kStockNormalRange = 15.0f;
constexpr float kStockResetRange  = 90.0f;
constexpr float kStockMultiplier  = 4.0f;

float g_resetRange = kStockResetRange;
float g_multiplier = kStockMultiplier;

bool ReadOperandTarget(uintptr_t address, const uint8_t* opcode, uintptr_t& targetOut) {
    const uint8_t* code = reinterpret_cast<const uint8_t*>(address);
    if (!game::IsReadable(code, kOperandOffset + sizeof(uint32_t))) return false;
    if (code[0] != opcode[0] || code[1] != opcode[1]) return false;

    uintptr_t target = 0;
    memcpy(&target, code + kOperandOffset, sizeof(uint32_t));
    if (!game::IsReadable(reinterpret_cast<const void*>(target), sizeof(float))) return false;

    targetOut = target;
    return true;
}

bool ReadOperandFloat(uintptr_t reference, const uint8_t* opcode, float& valueOut) {
    uintptr_t target = 0;
    if (!ReadOperandTarget(game::Resolve(reference), opcode, target)) return false;

    memcpy(&valueOut, reinterpret_cast<const void*>(target), sizeof(valueOut));
    return true;
}

bool WriteCode(uintptr_t address, const void* bytes, size_t size) {
    void* code = reinterpret_cast<void*>(address);
    if (!game::IsWritable(code, size)) return false;

    DWORD previousProtect = 0;
    if (!VirtualProtect(code, size, PAGE_EXECUTE_READWRITE, &previousProtect)) return false;

    memcpy(code, bytes, size);

    VirtualProtect(code, size, previousProtect, &previousProtect);
    FlushInstructionCache(GetCurrentProcess(), code, size);
    return true;
}

bool RedirectOperand(uintptr_t reference, const float* replacement) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(replacement);
    return WriteCode(game::Resolve(reference) + kOperandOffset, &value, sizeof(uint32_t));
}

bool VerifyOperand(uintptr_t reference, const uint8_t* opcode, float expected,
                   char* reasonOut, unsigned reasonSize) {
    float found = 0.0f;
    if (!ReadOperandFloat(reference, opcode, found)) {
        _snprintf(reasonOut, reasonSize, "unexpected instruction at 0x%08X",
                  static_cast<unsigned>(reference));
        reasonOut[reasonSize - 1] = '\0';
        return false;
    }

    if (found != expected) {
        _snprintf(reasonOut, reasonSize, "0x%08X reads %f, expected %f",
                  static_cast<unsigned>(reference),
                  static_cast<double>(found), static_cast<double>(expected));
        reasonOut[reasonSize - 1] = '\0';
        return false;
    }

    return true;
}

bool VerifyBytes(uintptr_t reference, const uint8_t* expected, size_t size,
                 char* reasonOut, unsigned reasonSize) {
    const uint8_t* code = reinterpret_cast<const uint8_t*>(game::Resolve(reference));

    if (!game::IsReadable(code, size) || memcmp(code, expected, size) != 0) {
        _snprintf(reasonOut, reasonSize, "unexpected instruction at 0x%08X",
                  static_cast<unsigned>(reference));
        reasonOut[reasonSize - 1] = '\0';
        return false;
    }

    return true;
}

}

bool Verify(char* reasonOut, unsigned reasonSize) {
    if (!VerifyBytes(game::kResetPenaltyCheck, kFlagSignature, kFlagSignatureSize,
                     reasonOut, reasonSize)) return false;

    if (!VerifyOperand(game::kNormalRangeLoad, kFldOpcode, kStockNormalRange,
                       reasonOut, reasonSize)) return false;

    if (!VerifyOperand(game::kResetRangeLoad, kFldOpcode, kStockResetRange,
                       reasonOut, reasonSize)) return false;

    if (!VerifyBytes(game::kSpeedGateSkip, kSpeedGateSkipBytes, sizeof(kSpeedGateSkipBytes),
                     reasonOut, reasonSize)) return false;

    if (!VerifyOperand(game::kResetRateScale, kFmulOpcode, kStockMultiplier,
                       reasonOut, reasonSize)) return false;

    return true;
}

bool Apply(const config::Settings& settings) {
    g_multiplier = settings.resetBustMultiplier;
    if (!RedirectOperand(game::kResetRateScale, &g_multiplier)) return false;

    if (settings.resetAutoBust) return true;

    float normalRange = kStockNormalRange;
    if (!ReadOperandFloat(game::kNormalRangeLoad, kFldOpcode, normalRange)) return false;

    g_resetRange = normalRange;
    if (!RedirectOperand(game::kResetRangeLoad, &g_resetRange)) return false;

    return WriteCode(game::Resolve(game::kSpeedGateSkip), kNops, sizeof(kNops));
}

}
