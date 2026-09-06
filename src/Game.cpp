#include "Game.h"

#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

namespace game {
namespace {

uintptr_t ModuleBase() {
    static uintptr_t cached = 0;
    if (cached == 0) cached = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    return cached;
}

const IMAGE_NT_HEADERS32* NtHeaders() {
    const uintptr_t base = ModuleBase();
    if (base == 0) return nullptr;

    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (!IsReadable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

    const IMAGE_NT_HEADERS32* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (!IsReadable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    return nt;
}

}

uintptr_t Resolve(uintptr_t referenceAddress) {
    return ModuleBase() + (referenceAddress - kReferenceImageBase);
}

bool VerifyImage(char* reasonOut, size_t reasonSize) {
    const IMAGE_NT_HEADERS32* nt = NtHeaders();
    if (nt == nullptr) {
        _snprintf(reasonOut, reasonSize, "the host module has no readable PE header");
        reasonOut[reasonSize - 1] = '\0';
        return false;
    }

    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        _snprintf(reasonOut, reasonSize, "the host module is not a 32-bit x86 image");
        reasonOut[reasonSize - 1] = '\0';
        return false;
    }

    if (nt->OptionalHeader.SizeOfImage != kReferenceSizeOfImage) {
        _snprintf(reasonOut, reasonSize,
                  "SizeOfImage is 0x%08X, expected 0x%08X",
                  static_cast<unsigned>(nt->OptionalHeader.SizeOfImage),
                  static_cast<unsigned>(kReferenceSizeOfImage));
        reasonOut[reasonSize - 1] = '\0';
        return false;
    }

    return true;
}

bool GetHostPath(char* out, size_t outSize) {
    if (out == nullptr || outSize == 0) return false;

    out[0] = '\0';
    const DWORD written = GetModuleFileNameA(nullptr, out, static_cast<DWORD>(outSize));
    if (written == 0 || written >= outSize) return false;

    return true;
}

bool ComputeMd5(const char* path, char* out, size_t outSize) {
    if (out == nullptr || outSize < kMd5TextSize) return false;
    out[0] = '\0';

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash     = 0;
    bool ok = false;

    if (CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_MD5, 0, 0, &hash)) {
        static BYTE buffer[32768];
        ok = true;

        for (;;) {
            DWORD read = 0;
            if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) { ok = false; break; }
            if (read == 0) break;
            if (!CryptHashData(hash, buffer, read, 0)) { ok = false; break; }
        }

        if (ok) {
            BYTE  digest[16];
            DWORD digestSize = sizeof(digest);
            ok = CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0) != 0 &&
                 digestSize == sizeof(digest);
            if (ok) {
                for (size_t i = 0; i < sizeof(digest); ++i) {
                    _snprintf(out + i * 2, 3, "%02X", digest[i]);
                }
                out[kMd5TextSize - 1] = '\0';
            }
        }
    }

    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    return ok;
}

bool IsReadable(const void* address, size_t size) {
    if (address == nullptr || size == 0) return false;

    MEMORY_BASIC_INFORMATION info;
    if (VirtualQuery(address, &info, sizeof(info)) == 0) return false;
    if (info.State != MEM_COMMIT) return false;

    const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY;
    if ((info.Protect & readable) == 0) return false;
    if (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;

    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end   = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return start + size <= end;
}

bool IsWritable(const void* address, size_t size) {
    if (address == nullptr || size == 0) return false;

    MEMORY_BASIC_INFORMATION info;
    if (VirtualQuery(address, &info, sizeof(info)) == 0) return false;
    if (info.State != MEM_COMMIT) return false;

    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end   = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return start + size <= end;
}

}
