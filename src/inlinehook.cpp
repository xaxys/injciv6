#include <windows.h>
#include "inlinehook.h"
#include "platform.h"

// [[deprecated]] 自版本 v0.3.0 起，不再使用 InlineHook，改用 MinHook 库

#ifdef _CPU_X64
#define HOOK_PATCH_MAX 18
#endif
#ifdef _CPU_X86
#define HOOK_PATCH_MAX 27
#endif

#ifdef _CPU_X64
static void *FindModuleTextBlankAlign(HMODULE hmodule)
{
    BYTE *p = (BYTE *)hmodule;
    p += ((IMAGE_DOS_HEADER *)p)->e_lfanew + 4; // 根据DOS头获取PE信息偏移量
    p += sizeof(IMAGE_FILE_HEADER) + ((IMAGE_FILE_HEADER *)p)->SizeOfOptionalHeader; // 跳过可选头
    WORD sections = ((IMAGE_FILE_HEADER *)p)->NumberOfSections; // 获取区段长度
    for (int i = 0; i < sections; i++) {
        IMAGE_SECTION_HEADER *psec = (IMAGE_SECTION_HEADER *)p;
        p += sizeof(IMAGE_SECTION_HEADER);
        if (memcmp(psec->Name, ".text", 5) == 0) { // 是否.text段
            BYTE *offset = (BYTE *)hmodule + psec->VirtualAddress + psec->Misc.VirtualSize; // 计算空白区域偏移量
            offset += 16 - (INT_PTR)offset % 16; // 对齐16字节
            UINT64 *buf = (UINT64 *)offset;
            while (buf[0] != 0 || buf[1] != 0) // 找到一块全是0的区域
                buf += 16;
            return (void *)buf;
        }
    }
    return 0;
}
#endif

InlineHook::InlineHook(HMODULE hmodule, const char *name, void *fake_func, int entry_len)
{
    func_ptr = (void *)GetProcAddress(hmodule, name);
    // 范围检查
    if (entry_len < HOOK_JUMP_LEN)
        entry_len = HOOK_JUMP_LEN;
    if (entry_len > HOOK_PATCH_MAX)
        entry_len = HOOK_PATCH_MAX;
    // 允许func_ptr处最前面的5字节内存可读可写可执行
    VirtualProtect(func_ptr, HOOK_JUMP_LEN, PAGE_EXECUTE_READWRITE, NULL);
    // 使用VirtualAlloc申请内存，使其可读可写可执行
    old_entry = VirtualAlloc(NULL, 32, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#ifdef _CPU_X64
    union
    {
        void *ptr;
        struct
        {
            DWORD32 lo;
            DWORD32 hi;
        };
    } ptr64;
    void *blank = FindModuleTextBlankAlign(hmodule); // 找到第一处空白区域
    VirtualProtect(blank, 14, PAGE_EXECUTE_READWRITE, NULL); // 可读写
    hook_entry[0] = 0xE9; // 跳转代码
    *(DWORD32 *)&hook_entry[1] = (BYTE *)blank - (BYTE *)func_ptr - 5; // 跳转到空白区域
    ptr64.ptr = fake_func;
    BYTE blank_jump[14];
    blank_jump[0] = 0x68; // push xxx
    *(DWORD32 *)&blank_jump[1] = ptr64.lo; // xxx，即地址的低4位
    blank_jump[5] = 0xC7;
    blank_jump[6] = 0x44;
    blank_jump[7] = 0x24;
    blank_jump[8] = 0x04; // mov dword [rsp+4], yyy
    *(DWORD32 *)&blank_jump[9] = ptr64.hi; // yyy，即地址的高4位
    blank_jump[13] = 0xC3; // ret
    // 写入真正的跳转代码到空白区域
    WriteProcessMemory(GetCurrentProcess(), blank, &blank_jump, 14, NULL);
    // 保存原来的入口代码
    memcpy(old_entry, func_ptr, entry_len);
    ptr64.ptr = (BYTE *)func_ptr + entry_len;
    // 设置新的跳转代码
    BYTE *new_jump = (BYTE *)old_entry + entry_len;
    new_jump[0] = 0x68;
    *(DWORD32 *)(new_jump + 1) = ptr64.lo;
    new_jump[5] = 0xC7;
    new_jump[6] = 0x44;
    new_jump[7] = 0x24;
    new_jump[8] = 0x04;
    *(DWORD32 *)(new_jump + 9) = ptr64.hi;
    new_jump[13] = 0xC3;
#endif
#ifdef _CPU_X86
    hook_entry[0] = 0xE9; // 跳转代码
    *(DWORD32 *)&hook_entry[1] = (BYTE *)fake_func - (BYTE *)func_ptr - 5; // 直接到hook的代码
    memcpy(old_entry, func_ptr, entry_len); // 保存入口
    BYTE *new_jump = (BYTE *)old_entry + entry_len;
    *new_jump = 0xE9; // 跳回去的代码
    *(DWORD32 *)(new_jump + 1) = (BYTE *)func_ptr + entry_len - new_jump - 5;
#endif
}

InlineHook::~InlineHook()
{
    if (old_entry)
        VirtualFree(old_entry, 0, MEM_RELEASE);
}

void InlineHook::hook()
{
    WriteProcessMemory(GetCurrentProcess(), func_ptr, &hook_entry, HOOK_JUMP_LEN, NULL);
}

void InlineHook::unhook()
{
    WriteProcessMemory(GetCurrentProcess(), func_ptr, old_entry, HOOK_JUMP_LEN, NULL);
}

void *InlineHook::get_old_entry()
{
    return old_entry;
}

