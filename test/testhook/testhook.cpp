#include <windows.h>
#include <stdio.h>
#include "inlinehook.h"
#include "platform.h"

#ifdef _CPU_X64
#define MESSAGEBOXA_ENTRYLEN 7 // 64位的一般需要反汇编得出
#endif
#ifdef _CPU_X86
#define MESSAGEBOXA_ENTRYLEN 5
#endif

typedef int WINAPI(*MessageBoxA_func)(HWND, LPCSTR, LPCSTR, UINT);

static InlineHook *hook;
static MessageBoxA_func _MessageBoxA;

int  WINAPI fk_MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    printf("original caption: %s\noriginal text: %s\n", lpCaption, lpText);
    return _MessageBoxA(hWnd, "Hook MessageBoxA", "test", MB_ICONERROR);
}

int main()
{
    hook = new InlineHook(GetModuleHandleA("user32.dll"), "MessageBoxA", (void *)fk_MessageBoxA, MESSAGEBOXA_ENTRYLEN);
    _MessageBoxA = (MessageBoxA_func)hook->get_old_entry();
    hook->hook();
    MessageBoxA(0, "testhook", "test", 0); // 被hook
    _MessageBoxA(0, "never hook", "test", 0); // 不被hook
    hook->unhook();
    delete hook;
    MessageBoxA(0, "nohook", "test", 0); // 不被hook
}
