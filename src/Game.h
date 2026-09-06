#pragma once

#include <stddef.h>
#include <stdint.h>

namespace game {

constexpr uintptr_t kReferenceImageBase   = 0x00400000;
constexpr uint32_t  kReferenceSizeOfImage = 0x00678E4E;

constexpr uintptr_t kResetPenaltyCheck = 0x0044447C;
constexpr uintptr_t kNormalRangeLoad   = 0x00444485;
constexpr uintptr_t kResetRangeLoad    = 0x0044448F;
constexpr uintptr_t kSpeedGateSkip     = 0x00444495;
constexpr uintptr_t kResetRateScale    = 0x004444D0;

constexpr size_t kMd5TextSize = 33;

uintptr_t Resolve(uintptr_t referenceAddress);

bool VerifyImage(char* reasonOut, size_t reasonSize);

bool GetHostPath(char* out, size_t outSize);
bool ComputeMd5(const char* path, char* out, size_t outSize);

bool IsReadable(const void* address, size_t size);
bool IsWritable(const void* address, size_t size);

}
