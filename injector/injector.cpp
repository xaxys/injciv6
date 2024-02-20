#include "inject.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#ifdef _CPU_X64
#define DLL_NAME L"hookdll64.dll"
#endif
#ifdef _CPU_X86
#define DLL_NAME L"hookdll32.dll"
#endif

void write_help()
{
    printf("Usage: injector32/injector64 <options>\n");
    printf("The options can be:\n");
    printf("  -h:          Show this help\n");
    printf("  -s:          Grant SE_DEBUG privilege\n");
    printf("  -i=<PID>:    Inject to the PID\n");
    printf("  -x=<EXE>:    Inject to the EXE\n");
    printf("\n");
    printf("e.g. injector32 -i=123456\n");
    printf("     injector64 -x=Abc.exe\n");
    exit(1);
}

void grant_privilege()
{
    if (!grant_se_debug_privilege()) {
        printf("Grant SE_DEBUG privilege failed\n");
    }
}

void format_error()
{
    printf("Parameter format error\n");
    write_help();
}

bool doinject(const wchar_t *dllpath, wchar_t mode, const wchar_t *param)
{
    if (!param || *param == L'\0')
        return false;
    if (mode == L'i') {
        DWORD pid = _wtoi(param);
        if (pid == 0) {
            printf("\"%s\" is not a number\n", param);
            return false;
        }
        else if (pid == (DWORD)-1) {
            printf("\"%s\" is overflow\n", param);
            return false;
        }
        return inject_dll(pid, dllpath);
    }
    else if (mode == L'x') {
        DWORD pid = find_pid_by_name(param);
        if (pid == 0) {
            printf("Can not find process by \"%s\"\n", param);
            return false;
        }
        return inject_dll(pid, dllpath);
    }
    return false;
}

int main(int argc, char *argv[])
{
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    wchar_t dll_path[MAX_PATH];
    GetModuleFileNameW(NULL, dll_path, MAX_PATH);
    wchar_t *pos = wcsrchr(dll_path, L'\\');
    *(pos + 1) = L'\0';
    wcscat(pos, DLL_NAME);
    FILE *fp = _wfopen(dll_path, L"rb");
    if (fp == NULL) {
        printf("Can not find DLL \"%s\"\n", dll_path);
        exit(1);
    }
    fclose(fp);
    argc--;
    wargv++;
    if (argc == 0)
        write_help();
    bool result = false;
    while (argc--) {
        if (**wargv != L'-') {
            format_error();
            return 0;
        }
        (*wargv)++;
        switch (**wargv) {
            case L'h': write_help(); break;
            case L's': grant_privilege(); break;
            case L'i':
            case L'x':
            {
                wchar_t mode = **wargv;
                (*wargv)++;
                if (**wargv == L'\0' || **wargv != L'=')
                    format_error();
                result = doinject(dll_path, mode, *wargv + 1);
                break;
            }
            default: format_error(); break;
        }
        wargv++;
    }
    if (result) {
        printf("Inject OK\n");
        exit(0);
    }
    else {
        printf("Unknown error\n");
        exit(-1);
    }
}
