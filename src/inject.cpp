#include <windows.h>
#include <tlhelp32.h>
#include "inject.h"

#ifdef __cplusplus
extern "C" {
#endif

bool grant_se_debug_privilege()
{
    HANDLE htoken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &htoken)) {
        return false;
    }
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    if (!LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &tp.Privileges[0].Luid)) {
        CloseHandle(htoken);
        return false;
    }
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(htoken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(htoken);
        return false;
    }
    DWORD result = GetLastError();
    if (result == ERROR_NOT_ALL_ASSIGNED) {
        CloseHandle(htoken);
        return false;
    }
    CloseHandle(htoken);
    return true;
}

bool inject_dll(DWORD pid, const char *dll_path)
{
    int path_len = strlen(dll_path) + 1;
    HANDLE hproc = 0;
    LPVOID pmem = NULL;
    HANDLE hthread = 0;
    bool result = false;
    hproc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid); // 打开进程
    if (hproc == 0) goto finally;
    pmem = VirtualAllocEx(hproc, NULL, path_len, MEM_COMMIT, PAGE_READWRITE); // 申请内存
    if (pmem == NULL) goto finally;
    WriteProcessMemory(hproc, pmem, dll_path, path_len, NULL); // 把dll路径写进去
    hthread = CreateRemoteThread(hproc, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, pmem, 0, NULL); // 创建远程线程注入
    if (hthread == 0) goto finally;
    WaitForSingleObject(hthread, INFINITE); // 等待线程执行
    DWORD threadres;
    GetExitCodeThread(hthread, &threadres); // 获取返回值
    result = threadres != 0; // LoadLibraryA错误返回0
    // 安全释放相应资源
    finally:
    if (pmem)
        VirtualFreeEx(hproc, pmem, 0, MEM_RELEASE);
    if (hthread != 0)
        CloseHandle(hthread);
    if (hproc != 0)
        CloseHandle(hproc);
    return result;
}

DWORD find_pid_by_name(const char *name)
{
    HANDLE procsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 procentry;
    procentry.dwSize = sizeof(PROCESSENTRY32);
    Process32First(procsnapshot, &procentry);
    if (strcmp(procentry.szExeFile, name) == 0) {
        CloseHandle(procsnapshot);
        return procentry.th32ProcessID;
    }
    while (Process32Next(procsnapshot, &procentry)) {
        if (strcmp(procentry.szExeFile, name) == 0) {
            CloseHandle(procsnapshot);
            return procentry.th32ProcessID;
        }
    }
    CloseHandle(procsnapshot);
    return 0;
}

HMODULE find_module_handle_from_pid(DWORD pid, const char *module_name)
{
    HMODULE h_result = 0;
    HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    MODULEENTRY32 module_entry;
    module_entry.dwSize = sizeof(MODULEENTRY32);
    Module32First(hsnap, &module_entry);
    do {
        if (strcmp(module_entry.szModule, module_name) == 0) {
            h_result = module_entry.hModule;
            break;
        }
    } while (Module32Next(hsnap, &module_entry));
    CloseHandle(hsnap);
    return h_result;
}

bool remove_module(DWORD pid, HMODULE module_handle)
{
    HANDLE hproc = 0;
    HANDLE hthread = 0;
    bool result = false;
    hproc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hproc == 0) goto finally;
    hthread = CreateRemoteThread(hproc, NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, module_handle, 0, NULL);
    if (hthread == 0) goto finally;
    WaitForSingleObject(hthread, INFINITE);
    DWORD threadres;
    GetExitCodeThread(hthread, &threadres);
    result = threadres != 0;
finally:
    if (hthread != 0)
        CloseHandle(hthread);
    if (hproc != 0)
        CloseHandle(hproc);
    return result;
}

#ifdef __cplusplus
}
#endif
