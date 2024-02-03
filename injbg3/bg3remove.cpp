#include <shlobj.h>
#include "inject.h"
#include "injbg3.h"

int main(int argc, char *argv[])
{
    bool isadmin = IsUserAnAdmin();
    int msgres = 0;
    if (!isadmin) {
    retry_runas:
        if (runas_admin(argv[0])) // 成功运行就退出自己
            return 0;
        msgres = MessageBoxW(0, L"请允许管理员权限", L"错误", MB_ICONERROR | MB_RETRYCANCEL);
        if (msgres == IDRETRY)
            goto retry_runas;
        return 0;
    }
    bool se_debug = grant_se_debug_privilege(); // 尝试获取SE_DEBUG权限
    if (!se_debug) {
        MessageBoxW(0, L"获取SE_DEBUG权限失败，后续可能会出现禁止访问的错误", L"警告", MB_ICONWARNING);
    }
    DWORD pid = 0;
    HMODULE module_handle = 0;
    pid = get_bg3_proc();
    if (pid == 0) {
        MessageBoxW(0, L"找不到游戏进程", L"错误", MB_ICONERROR);
        return 0;
    }
    module_handle = find_module_handle_from_pid(pid, "hookdll64.dll");
    if (module_handle == 0) {
        MessageBoxW(0, L"当前没有注入DLL", L"错误", MB_ICONERROR);
        return 0;
    }
    if (remove_module(pid, module_handle))
        MessageBoxW(0, L"成功移除DLL！", L"成功", MB_OK);
    else
        msgres = MessageBoxW(0, L"移除失败", L"错误", MB_ICONERROR);
}
