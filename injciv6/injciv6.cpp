#include <stdio.h>
#include <shlobj.h>
#include "inject.h"
#include "injciv6.h"

int main(int argc, char *argv[])
{
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool silence = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) silence = true;
    }
    wchar_t dll_path[MAX_PATH];
    wcscpy_s(dll_path, MAX_PATH, wargv[0]);
    int pos = wcslen(dll_path);
    while (pos > 0) {
        if (dll_path[pos] == L'\\')
            break;
        pos--;
    }
    dll_path[pos + 1] = L'\0';
    wcscat_s(dll_path, MAX_PATH, L"hookdll64.dll");
    FILE *fp = _wfopen(dll_path, L"rb");
    if (fp == NULL) {
        MessageBoxW(0, L"找不到hookdll64.dll", L"错误", MB_ICONERROR);
        return 0;
    }
    fclose(fp);
    int msgres = 0;
    bool isadmin = IsUserAnAdmin(); // 检测当前是否有管理员权限，需要shlobj.h
    bool se_debug = false;
    if (isadmin) {
        se_debug = grant_se_debug_privilege(); // 尝试获取SE_DEBUG权限
        if (!se_debug && !silence) {
            MessageBoxW(0, L"获取SE_DEBUG权限失败，后续可能会出现禁止访问的错误", L"警告", MB_ICONWARNING);
        }
    }
    DWORD civ6pid = 0;
    while (1) {
        civ6pid = get_civ6_proc();
        if (civ6pid != 0)
            break;
        msgres = MessageBoxW(0, L"请先运行游戏，然后点击重试", L"错误", MB_RETRYCANCEL | MB_ICONERROR);
        if (msgres != IDRETRY)
            return 0;
    }
    bool reinj = false;
    HMODULE dll = find_module_handle_from_pid(civ6pid, L"hookdll64.dll");
    if (dll != 0) {
        msgres = IDYES;
        if (!silence)
            msgres = MessageBoxW(0, L"当前已经注入DLL，是否重新注入？", L"警告", MB_ICONWARNING | MB_YESNO);
        if (msgres == IDNO)
            return 0;
        if (!isadmin) {
        retry_runas_silence:
            if (runas_admin(wargv[0], L"-s")) // 成功运行就退出自己
                return 0;
            msgres = MessageBoxW(0, L"请允许管理员权限", L"错误", MB_ICONERROR | MB_RETRYCANCEL);
            if (msgres == IDRETRY)
                goto retry_runas_silence;
            return 0;
        }
        if (!remove_module(civ6pid, dll)) {
            MessageBoxW(0, L"移除失败", L"错误", MB_ICONERROR);
            return 0;
        }
        reinj = true;
    }
retry_inject:
    // 尝试注入
    if (inject_dll(civ6pid, dll_path)) {
        if (reinj)
            MessageBoxW(0, L"重新注入成功！", L"成功", MB_OK);
        else
            MessageBoxW(0, L"注入成功！", L"成功", MB_OK);
        return 0;
    }
    // 如果管理员权限也失败了，那就不知道什么情况了
    if (isadmin) {
        // 一般情况下是走不到这里来的
        if (reinj) {
            msgres = MessageBoxW(0, L"移除成功，但重新注入失败", L"错误", MB_RETRYCANCEL | MB_ICONERROR);
            reinj = false;
        } else
            msgres = MessageBoxW(0, L"注入失败", L"错误", MB_RETRYCANCEL | MB_ICONERROR);
        if (msgres == IDRETRY)
            goto retry_inject;
        return 0;
    }
    // 重新注入的一定是管理员权限，所以不会走到这里
    // 有的时候比如steam运行游戏可能是带管理员权限的，这个时候要注入程序也要有管理员权限
    msgres = MessageBoxW(0, L"注入失败，是否以管理员权限重试？", L"错误", MB_ICONERROR | MB_RETRYCANCEL);
    if (msgres == IDRETRY) {
    retry_runas:
        if (runas_admin(wargv[0])) // 成功运行就退出自己
            return 0;
        msgres = MessageBoxW(0, L"请在弹出的窗口中点击“是”", L"错误", MB_ICONERROR | MB_RETRYCANCEL);
        if (msgres == IDRETRY)
            goto retry_runas;
    }
}
