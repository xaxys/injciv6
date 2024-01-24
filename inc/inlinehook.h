#pragma once

#include <windows.h>

#define HOOK_JUMP_LEN 5

// [[deprecated]] 自版本 v0.3.0 起，不再使用 InlineHook，改用 MinHook 库
class [[deprecated]] InlineHook
{
private:
    void *old_entry; // 存放原来的代码和跳转回去的代码
    char hook_entry[HOOK_JUMP_LEN]; // hook跳转的代码
    void *func_ptr;  // 被hook函数的地址
public:
    InlineHook(HMODULE hmodule, const char *name, void *fake_func, int entry_len);
    ~InlineHook();
    void hook();
    void unhook();
    void *get_old_entry();
};
