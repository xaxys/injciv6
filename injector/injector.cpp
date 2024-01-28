#include "inject.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#ifdef _CPU_X64
#define DLL_NAME "hookdll64.dll"
#endif
#ifdef _CPU_X86
#define DLL_NAME "hookdll32.dll"
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

bool doinject(const char *dllpath, char mode, const char *param)
{
    if (!param || *param == '\0')
        return false;
    if (mode == 'i') {
        DWORD pid = atoi(param);
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
    else if (mode == 'x') {
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
    char dll_path[MAX_PATH];
    GetModuleFileNameA(NULL, dll_path, MAX_PATH);
    char *pos = strrchr(dll_path, '\\');
    *(pos + 1) = '\0';
    strcat(pos, DLL_NAME);
    FILE *fp = fopen(dll_path, "rb");
    if (fp == NULL) {
        printf("Can not find DLL \"%s\"\n", dll_path);
        exit(1);
    }
    fclose(fp);
    argc--;
    argv++;
    if (argc == 0)
        write_help();
    bool result = false;
    while (argc--) {
        if (**argv != '-') {
            format_error();
            return 0;
        }
        (*argv)++;
        switch (**argv) {
            case 'h': write_help(); break;
            case 's': grant_privilege(); break;
            case 'i':
            case 'x':
            {
                char mode = **argv;
                (*argv)++;
                if (**argv == '\0' || **argv != '=')
                    format_error();
                result = doinject(dll_path, mode, *argv + 1);
                break;
            }
            default: format_error(); break;
        }
        argv++;
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
