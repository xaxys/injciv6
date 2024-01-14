#include <stdio.h>
#include <shlobj.h>
#include "inject.h"
#include "injciv6.h"

int main(int argc, char *argv[])
{
    char dll_path[MAX_PATH];
    strcpy(dll_path, argv[0]);
    int pos = strlen(dll_path) - 1;
    while (pos > 0) {
        if (dll_path[pos] == '\\')
            break;
        pos--;
    }
    dll_path[pos + 1] = '\0';
    strcat(dll_path, "hookdll64.dll");
    FILE *fp = fopen(dll_path, "rb");
    if (fp == NULL) {
        MessageBoxW(0, L"找不到hookdll64.dll", L"错误", MB_ICONERROR);
        return 0;
    }
    fclose(fp);
    int msgres = 0;
    bool isadmin = IsUserAnAdmin(); // 检测当前是否有管理员权限，需要shlobj.h
    DWORD civ6pid = 0;
    while (1) {
        civ6pid = get_civ6_proc();
        if (civ6pid != 0)
            break;
        msgres = MessageBoxW(0, L"请先运行游戏，然后点击重试", L"错误", MB_RETRYCANCEL | MB_ICONERROR);
        if (msgres != IDRETRY)
            return 0;
    }
retry_inject:
    // 尝试注入
    if (inject_dll(civ6pid, dll_path)) {
        MessageBoxW(0, L"注入成功！", L"成功", MB_OK);
        return 0;
    }
    // 如果管理员权限也失败了，那就不知道什么情况了
    if (isadmin) {
        // 一般情况下是走不到这里来的
        msgres = MessageBoxW(0, L"注入失败", L"错误", MB_RETRYCANCEL | MB_ICONERROR);
        if (msgres == IDRETRY)
            goto retry_inject;
        return 0;
    }
    // 有的时候比如steam运行游戏可能是带管理员权限的，这个时候要注入程序也要有管理员权限
    msgres = MessageBoxW(0, L"注入失败，是否以管理员权限重试？", L"错误", MB_ICONERROR | MB_RETRYCANCEL);
    if (msgres == IDRETRY) {
    retry_runas:
        if (runas_admin(argv[0])) // 成功运行就退出自己
            return 0;
        msgres = MessageBoxW(0, L"请在弹出的窗口中点击“是”", L"错误", MB_ICONERROR | MB_RETRYCANCEL);
        if (msgres == IDRETRY)
            goto retry_runas;
    }
}
