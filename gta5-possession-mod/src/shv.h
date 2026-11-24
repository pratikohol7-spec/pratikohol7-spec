// Minimal Script Hook V + Windows declarations for a CRT-free .asi.
// Only the functions declared here are imported; each one is implemented by the
// GTA V iOS port's ASI interpreter (app/Source/NativeASI.c import table) and by
// Script Hook V / Windows on PC.
#pragma once

typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long long UINT64;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

struct HINSTANCE__;
typedef HINSTANCE__ *HMODULE;
typedef HINSTANCE__ *HINSTANCE;

// ScriptHookV.dll (C++ exports; MSVC mangling must match ScriptHookV.lib)
__declspec(dllimport) void scriptRegister(HMODULE module, void (*LP_SCRIPT_MAIN)());
__declspec(dllimport) void scriptUnregister(HMODULE module);
__declspec(dllimport) void scriptWait(DWORD time);
__declspec(dllimport) void nativeInit(UINT64 hash);
__declspec(dllimport) void nativePush64(UINT64 val);
__declspec(dllimport) UINT64 *nativeCall();

extern "C" {
// KERNEL32.dll
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(HMODULE module, char *fileName, DWORD size);
__declspec(dllimport) UINT __stdcall GetPrivateProfileIntA(const char *app, const char *key, int def, const char *file);
// USER32.dll
__declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
}
