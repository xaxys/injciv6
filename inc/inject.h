#pragma once
#include <stdbool.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

bool grant_se_debug_privilege();
bool inject_dll(DWORD pid, const wchar_t *dll_path);
DWORD find_pid_by_name(const wchar_t *name);
HMODULE find_module_handle_from_pid(DWORD pid, const wchar_t *module_name);
bool remove_module(DWORD pid, HMODULE module_handle);

#ifdef __cplusplus
}
#endif
