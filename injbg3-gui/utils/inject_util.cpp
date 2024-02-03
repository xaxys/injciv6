#include <winsock2.h>

#include "inject.h"

#include <Shlwapi.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <ws2tcpip.h>

bool runas_admin(LPCWSTR exename) {
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_DDEWAIT | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = exename;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei);
}

DWORD get_bg3_dx11_proc() {
    return find_pid_by_name("bg3_dx11.exe");
}

DWORD get_bg3_dx12_proc() {
    return find_pid_by_name("bg3.exe");
}

DWORD get_bg3_proc() {
    DWORD pid = get_bg3_dx11_proc();
    if (pid == 0) pid = get_bg3_dx12_proc();
    return pid;
}

bool get_proc_path(DWORD pid, LPWSTR buf, DWORD bufsize) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc == NULL) {
        DWORD err = GetLastError();
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, bufsize, NULL);
        CloseHandle(hProc);
        return false;
    }
    DWORD result = GetModuleFileNameExW(hProc, NULL, buf, bufsize);
    if (result == 0) {
        DWORD err = GetLastError();
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, bufsize, NULL);
        CloseHandle(hProc);
        return false;
    }
    buf[result] = '\0';
    CloseHandle(hProc);
    return true;
}

bool get_bg3_path(LPWSTR buf, DWORD bufsize) {
    DWORD pid = get_bg3_proc();
    if (pid == 0) return false;
    return get_proc_path(pid, buf, bufsize);
}

bool is_injbg3_running(DWORD pid) {
    MIB_UDP6TABLE_OWNER_PID staticUdpTable[128];
    PMIB_UDP6TABLE_OWNER_PID pUdpTable = staticUdpTable;
    DWORD dwSize = sizeof(staticUdpTable);

    DWORD dwRetVal = GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pUdpTable = (MIB_UDP6TABLE_OWNER_PID*)malloc(dwSize);
        dwRetVal = GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
    }

    printf("GetExtendedUdpTable returned %d\n", dwRetVal);
    
    bool found = false;
    if (dwRetVal == NO_ERROR) {
        for (unsigned long i = 0; i < pUdpTable->dwNumEntries; i++) {
            // filter pid
            if (pUdpTable->table[i].dwOwningPid != pid) continue;

            // filter [::]
            static u_char zero[16] = {0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0};
            if (memcmp(pUdpTable->table[i].ucLocalAddr, zero, 16) != 0) continue;

            u_short port = ntohs((u_short)pUdpTable->table[i].dwLocalPort);
            if (port >= 23253 && port <= 23262) {
                found = true;
                break;
            }
            if (port == 23243) {
                found = true;
                break;
            }
        }
    }

    if (pUdpTable != staticUdpTable) {
        free(pUdpTable);
        pUdpTable = NULL;
    }

    return found;
}
