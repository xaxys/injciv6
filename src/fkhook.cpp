#include <winsock2.h>

#include "Minhook.h"
#include "fkhook.h"
#include "platform.h"
#include "sockaddrin.h"
#include "fakeaddrpool.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <iphlpapi.h>
#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <ws2tcpip.h>

// #define DEBUG_ENABLE
#define LOG_ENABLE

/*
==================== Constants ====================
*/

static const ULONG FAKE_IP_RANGE = 100;
static const IN_ADDR FAKE_IP_START = {127, 0, 127, 1};

/*
==================== Global Variables ====================
*/

static std::unordered_map<SOCKET, SOCKET> socks;
static FakeAddrPool fake_addr_pool(FAKE_IP_START, FAKE_IP_RANGE);
static char address[256] = "255.255.255.255";

/*
==================== Hooked Functions ====================
*/

typedef int WINAPI(*sendto_func) (SOCKET, const char *, int, int, const sockaddr *, int);
typedef int WINAPI(*select_func) (int, fd_set *, fd_set *, fd_set *, const TIMEVAL *);
typedef int WINAPI(*recvfrom_func) (SOCKET, char *, int, int, sockaddr *, int *);
typedef int WINAPI(*closesocket_func) (SOCKET);
typedef int WINAPI(*bind_func) (SOCKET, const sockaddr *, int);
typedef int WINAPI(*wsasendto_func) (SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, const sockaddr *, int, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
typedef int WINAPI(*wsarecvfrom_func) (SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD, sockaddr *, LPINT, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

static sendto_func _sendto = NULL;
static select_func _select = NULL;
static recvfrom_func _recvfrom = NULL;
static closesocket_func _closesocket = NULL;
static bind_func _bind = NULL;
static wsasendto_func _wsasendto = NULL;
static wsarecvfrom_func _wsarecvfrom = NULL;

/*
==================== Log Functions ====================
*/

#ifdef DEBUG_ENABLE
static void write_debug_impl(const char *fmt, ...)
{
    // FILE *fp = fopen("injciv6-debug.txt", "r");
    // if (!fp) return;
    FILE *fp = fopen("injciv6-debug.txt", "a+");
    if (fp) {
        time_t now = time(0);
        struct tm *timeinfo = localtime(&now);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

        fprintf(fp, "[%s] ", timestamp);

        va_list args;
        va_start(args, fmt);
        vfprintf(fp, fmt, args);
        va_end(args);

        fclose(fp);
    }
}

static void write_socks_impl()
{
    write_debug_impl("socks: %d\n", socks.size());
    for (auto it = socks.begin(); it != socks.end(); it++) {
        write_debug_impl("    %d -> %d\n", it->first, it->second);
    }
}
#define write_debug(fmt, ...) write_debug_impl(fmt, __VA_ARGS__)
#define write_debug_ex(cond, stmt, fmt, ...) if (cond) { stmt; write_debug_impl(fmt, __VA_ARGS__); }
#define write_socks() write_socks_impl()
#else
#define write_debug(fmt, ...)
#define write_debug_ex(cond, stmt, fmt, ...)
#define write_socks()
#endif

#ifdef LOG_ENABLE
static void write_log_impl(const char *fmt, ...)
{
    FILE *fp = fopen("injciv6-log.txt", "a+");
    if (fp) {
        time_t now = time(0);
        struct tm *timeinfo = localtime(&now);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

        fprintf(fp, "[%s] ", timestamp);

        va_list args;
        va_start(args, fmt);
        vfprintf(fp, fmt, args);
        va_end(args);

        fclose(fp);
    }
}
#define write_log(fmt, ...) write_log_impl(fmt, __VA_ARGS__)
#define write_log_ex(cond, stmt, fmt, ...) if (cond) { stmt; write_log_impl(fmt, __VA_ARGS__); }
#else
#define write_log(fmt, ...)
#define write_log_ex(cond, stmt, fmt, ...)
#endif

/*
==================== Config Functions ====================
*/

static void read_config()
{
    FILE *fp = fopen("injciv6-config.txt", "r");
    if (fp) {
        fscanf(fp, "%s", address);
    } else {
        fp = fopen("injciv6-config.txt", "w+");
        if (fp) {
            fprintf(fp, "%s", address);
        }
    }
    fclose(fp);
}

/*
==================== Util Functions ====================
*/

// 如果需要使用这个函数，需要在编译时链接 iphlpapi.lib，即 linkflags 加上 -l iphlpapi
// 文明6其实不需要这个判断，但是博德之门3需要这个来判断广播IP，实测AdaptersInfo使用栈上空间时约0.8ms，使用堆上空间时2-3ms
in_addr get_broadcast_ip(const struct in_addr& ip) {
    in_addr broadcast_ip = {0};
    IP_ADAPTER_INFO adapterInfo[32]; // 静态储存空间

    // 获取网络适配器信息
    PIP_ADAPTER_INFO pAdapterInfo = adapterInfo; // 指向静态储存空间
    ULONG ulOutBufLen = sizeof(adapterInfo);

    DWORD dwRetVal;
    dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        pAdapterInfo = (IP_ADAPTER_INFO *) malloc(ulOutBufLen); // 空间不够，动态重新分配 ulOutBufLen 会被修改为实际长度
        dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    }

    if (dwRetVal != NO_ERROR) {
        write_log("get adapters info failed: %d\n", dwRetVal);
        return broadcast_ip;
    }

    for (PIP_ADAPTER_INFO pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
        // 检查ip是否在当前适配器的子网内
        ULONG adpt_ip = inet_addr(pAdapter->IpAddressList.IpAddress.String);
        ULONG mask = inet_addr(pAdapter->IpAddressList.IpMask.String);
        if (adpt_ip == INADDR_ANY || mask == INADDR_ANY) {
            continue;
        }
        if ((ip.S_un.S_addr & mask) == (adpt_ip & mask)) {
            // 计算广播地址
            broadcast_ip.S_un.S_addr = ip.S_un.S_addr | ~mask;
            return broadcast_ip;
        }
    }

    if (pAdapterInfo != adapterInfo) {
        free(pAdapterInfo);
    }

    return broadcast_ip;
}

bool is_broadcast_ip(const struct in_addr& ip) {
    if (ip.S_un.S_addr == INADDR_BROADCAST) return true;
    in_addr broadcast_ip = get_broadcast_ip(ip);
    if (ip.S_un.S_addr == broadcast_ip.S_un.S_addr) return true;
    return false;
}

/*
==================== Detour Functions For Hooking ====================
*/

// SendTo & WSASendTo

static int WINAPI fake_sendto(SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen)
{
    sockaddr_in *origin_to = (sockaddr_in *)to;

    // FakeIP直接重定向后发送
    if (fake_addr_pool.has_origin_addr(origin_to)) {
        SockAddrIn new_to = fake_addr_pool.get_origin_addr(origin_to);
        new_to.set_port_raw(origin_to->sin_port);
        SOCKET new_sock = socks.count(s) ? socks[s] : s;

        int result = _sendto(new_sock, buf, len, flags, (sockaddr *)&new_to, sizeof(sockaddr_in6));
        if (result == SOCKET_ERROR) {
            int errorcode = WSAGetLastError();
            write_log("fake-ip  sendto failed: %s -> %s, through: %s, %d -> %d, result: %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, result, errorcode);
            goto fallback;
        }
        write_debug("fake-ip  sendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, result);
        return result;
    }

    // 重定向广播至单播
    if (is_broadcast_ip(origin_to->sin_addr)) {
        SockAddrIn new_to;
        if (!SockAddrIn::from_addr(address, &new_to)) {
            write_log("parse address failed while sendto: %s\n", address);
            goto fallback;
        }
        new_to.set_port_raw(origin_to->sin_port);
        SOCKET new_sock = socks.count(s) ? socks[s] : s;

        // 如果是IPv6，需要创建新的socket
        if (new_to.get_family() == AF_INET6) {
            // 获取原socket绑定的地址端口
            sockaddr_in origin_local_addr;
            int origin_local_addr_len = sizeof(sockaddr_in);
            getsockname(s, (sockaddr *)&origin_local_addr, &origin_local_addr_len); // 获取原sockaddr
            
            // 如果没有端口号，先原样发送，这样系统才会分配一个端口号
            if (origin_local_addr.sin_port == 0) {
                int result = _sendto(s, buf, len, flags, to, tolen);
                if (result == SOCKET_ERROR) {
                    int errorcode = WSAGetLastError();
                    write_log("original sendto failed while get origin socket port: %s, %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), s, errorcode);
                    goto fallback;
                }
                getsockname(s, (sockaddr *)&origin_local_addr, &origin_local_addr_len); // 重新获取
            }

            // 新socket绑定的地址端口
            SockAddrIn new_local_addr;
            new_local_addr.set_family(new_to.get_family());
            new_local_addr.set_ip_any();
            new_local_addr.set_port_raw(origin_local_addr.sin_port);

            // 创建新socket
            SOCKET sock = socket(new_local_addr.get_family(), SOCK_DGRAM, 0);
            if (sock == INVALID_SOCKET) {
                int errorcode = WSAGetLastError();
                write_log("create socket failed while sendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
                goto fallback;
            }
            ULONG on = 1;
            ioctlsocket(sock, FIONBIO, &on); // 设置为非阻塞, 非常重要，尤其是对于 steam 版的 steam 好友发现 47584 端口不可以去掉
            BOOL opt = TRUE;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(BOOL)); // 允许发送广播
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口

            // 绑定地址端口
            int result = _bind(sock, new_local_addr.get_sockaddr(), new_local_addr.get_len());
            if (result == SOCKET_ERROR) {
                int errorcode = WSAGetLastError();
                write_log("bind failed while sendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
                return SOCKET_ERROR; // 绑定失败，则不触发fallback，直接返回SOCKET_ERROR，由上层处理
            }

            // 替换原socket
            socks[s] = sock;
            new_sock = sock;

            write_socks();
            write_debug("replace origin_sock while sendto: %s -> %s, %d -> %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock);
        }

        int result = _sendto(new_sock, buf, len, flags, new_to.get_sockaddr(), new_to.get_len());
        if (result == SOCKET_ERROR) {
            int errorcode = WSAGetLastError();
            write_log("redirect sendto failed: %s -> %s, through: %s, %d -> %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, errorcode);
            goto fallback;
        }

        write_debug_ex(result > 0, , "redirect sendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, result);
        return result;
    }

    // 非广播原样发送
fallback:
    int result = _sendto(s, buf, len, flags, to, tolen);
    if (result == SOCKET_ERROR) {
        int errorcode = WSAGetLastError();
        write_log("original sendto failed: %s, through: %s, %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(SockAddrIn::from_socket(s)).c_str(), s, errorcode);
    }
    write_debug("original sendto: %s, through: %s, %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(SockAddrIn::from_socket(s)).c_str(), s, result);
    return result;
}

static int WINAPI fake_wsasendto(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const sockaddr *lpTo, int iTolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    sockaddr_in *origin_to = (sockaddr_in *)lpTo;

    // FakeIP直接重定向后发送
    if (fake_addr_pool.has_origin_addr(origin_to)) {
        SockAddrIn new_to = fake_addr_pool.get_origin_addr(origin_to);
        new_to.set_port_raw(origin_to->sin_port);
        SOCKET new_sock = socks.count(s) ? socks[s] : s;

        int result = _wsasendto(new_sock, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, (sockaddr *)&new_to, sizeof(sockaddr_in6), lpOverlapped, lpCompletionRoutine);
        if (result == SOCKET_ERROR) {
            int errorcode = WSAGetLastError();
            write_log("fake-ip  wsasendto failed: %s -> %s, through: %s, %d -> %d, result: %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, result, errorcode);
            goto fallback;
        }
        write_debug("fake-ip  wsasendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, result);
        return result;
    }

    // 重定向广播至单播
    if (is_broadcast_ip(origin_to->sin_addr)) {
        SockAddrIn new_to;
        if (!SockAddrIn::from_addr(address, &new_to)) {
            write_log("parse address failed while wsasendto: %s\n", address);
            goto fallback;
        }
        new_to.set_port_raw(origin_to->sin_port);
        SOCKET new_sock = socks.count(s) ? socks[s] : s;

        // 如果是IPv6，需要创建新的socket
        if (new_to.get_family() == AF_INET6) {
            // 获取原socket绑定的地址端口
            sockaddr_in origin_local_addr;
            int origin_local_addr_len = sizeof(sockaddr_in);
            getsockname(s, (sockaddr *)&origin_local_addr, &origin_local_addr_len); // 获取原sockaddr
            
            // 如果没有端口号，先原样发送，这样系统才会分配一个端口号
            if (origin_local_addr.sin_port == 0) {
                int result = _wsasendto(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine);
                if (result == SOCKET_ERROR) {
                    int errorcode = WSAGetLastError();
                    write_log("original wsasendto failed while get origin socket port: %s, %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), s, errorcode);
                    goto fallback;
                }
                getsockname(s, (sockaddr *)&origin_local_addr, &origin_local_addr_len); // 重新获取
            }

            // 新socket绑定的地址端口
            SockAddrIn new_local_addr;
            new_local_addr.set_family(new_to.get_family());
            new_local_addr.set_ip_any();
            new_local_addr.set_port_raw(origin_local_addr.sin_port);

            // 创建新socket
            SOCKET sock = socket(new_local_addr.get_family(), SOCK_DGRAM, 0);
            if (sock == INVALID_SOCKET) {
                int errorcode = WSAGetLastError();
                write_log("create socket failed while wsasendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
                goto fallback;
            }
            ULONG on = 1;
            ioctlsocket(sock, FIONBIO, &on); // 设置为非阻塞, 非常重要，尤其是对于 steam 版的 steam 好友发现 47584 端口不可以去掉
            BOOL opt = TRUE;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(BOOL)); // 允许发送广播
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口

            // 绑定地址端口
            int result = _bind(sock, new_local_addr.get_sockaddr(), new_local_addr.get_len());
            if (result == SOCKET_ERROR) {
                int errorcode = WSAGetLastError();
                write_log("bind failed while wsasendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
                return SOCKET_ERROR; // 绑定失败，则不触发fallback，直接返回SOCKET_ERROR，由上层处理
            }

            // 替换原socket
            socks[s] = sock;
            new_sock = sock;

            write_socks();
            write_debug("replace origin_sock while wsasendto: %s -> %s, %d -> %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock);
        }

        int result = _wsasendto(new_sock, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, new_to.get_sockaddr(), new_to.get_len(), lpOverlapped, lpCompletionRoutine);
        if (result == SOCKET_ERROR) {
            int errorcode = WSAGetLastError();
            write_log("redirect wsasendto failed: %s -> %s, through: %s, %d -> %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, errorcode);
            goto fallback;
        }

        write_debug_ex(result == 0, , "redirect wsasendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, result);
        return result;
    }

    // 非广播原样发送
fallback:
    int result = _wsasendto(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine);
    if (result == SOCKET_ERROR) {
        int errorcode = WSAGetLastError();
        write_log("original wsasendto failed: %s, through: %s, %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(SockAddrIn::from_socket(s)).c_str(), s, errorcode);
    }
    write_debug("original wsasendto: %s, through: %s, %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(SockAddrIn::from_socket(s)).c_str(), s, result);
    return result;
}

// recvfrom & WSARecvFrom

static int WINAPI fake_recvfrom(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
    // 替换的socket，重定向后接收，替换的一定是IPv6Socket
    if (socks.count(s)) {
        // 重定向后接收
        SockAddrIn new_from;
        int new_from_len = sizeof(new_from.addr);
        int result = _recvfrom(socks[s], buf, len, flags, new_from.get_sockaddr(), &new_from_len);
        if (result == SOCKET_ERROR) {
            int errorcode = WSAGetLastError();
            if (errorcode != WSAEWOULDBLOCK) write_log("redirect recvfrom failed: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), s, socks[s], errorcode);
            return result;
        }

        // 替换为fake-ip
        sockaddr_in fake_from = fake_addr_pool.get_fake_addr(new_from);
        fake_from.sin_port = new_from.get_port_raw();
        memcpy(from, &fake_from, sizeof(sockaddr_in));
        *fromlen = sizeof(sockaddr_in);

        write_debug_ex(result > 0, , "redirect recvfrom: %s -> %s, from: %s(origin-ip) -> %s(fake-ip), %d -> %d, result: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), std::to_string(new_from).c_str(), std::to_string(*((sockaddr_in *)from)).c_str(), s, socks[s], result);
        return result;
    }

    // 非替换的socket，原样接收，但也替换为fake-ip（规避文明6的局域网联机私有IPv4地址过滤）
    int result = _recvfrom(s, buf, len, flags, from, fromlen);
    if (result == SOCKET_ERROR) {
        int errorcode = WSAGetLastError();
        if (errorcode != WSAEWOULDBLOCK) write_log("original recvfrom failed: %s, %d, errorcode: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), s, errorcode);
        return result;
    }

    // 替换为fake-ip
    sockaddr_in *origin_from = (sockaddr_in *)from;
    sockaddr_in fake_from = fake_addr_pool.get_fake_addr(origin_from);
    fake_from.sin_port = origin_from->sin_port;
    memcpy(from, &fake_from, sizeof(sockaddr_in));
    *fromlen = sizeof(sockaddr_in);

    write_debug_ex(result > 0, , "original recvfrom: %s, from: %s(origin-ip) -> %s(fake-ip), %d, result: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(*origin_from).c_str(), std::to_string(*((sockaddr_in *)from)).c_str(), s, result);
    return result;
}

static int WINAPI fake_wsarecvfrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // 替换的socket，重定向后接收，替换的一定是IPv6Socket
    if (socks.count(s)) {
        SockAddrIn new_from;
        int new_from_len = sizeof(new_from.addr);
        int result = _wsarecvfrom(socks[s], lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, new_from.get_sockaddr(), &new_from_len, lpOverlapped, lpCompletionRoutine);
        if (result == SOCKET_ERROR) {
            int errorcode = WSAGetLastError();
            if (errorcode != WSAEWOULDBLOCK) write_log("redirect wsarecvfrom failed: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), s, socks[s], errorcode);
            return result;
        }

        // 替换为fake-ip
        sockaddr_in fake_from = fake_addr_pool.get_fake_addr(new_from);
        fake_from.sin_port = new_from.get_port_raw();
        memcpy(lpFrom, &fake_from, sizeof(sockaddr_in));
        *lpFromlen = sizeof(sockaddr_in);

        write_debug_ex(result == 0, , "redirect wsarecvfrom: %s -> %s, from: %s(origin-ip) -> %s(fake-ip), %d -> %d, result: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), std::to_string(new_from).c_str(), std::to_string(*((sockaddr_in *)lpFrom)).c_str(), s, socks[s], result);
        return result;
    }

    // 非替换的socket，原样接收，但也替换为fake-ip（规避文明6的局域网联机私有IPv4地址过滤）
    int result = _wsarecvfrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);
    if (result == SOCKET_ERROR) {
        int errorcode = WSAGetLastError();
        if (errorcode != WSAEWOULDBLOCK) write_log("original wsarecvfrom failed: %s, %d, errorcode: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), s, errorcode);
        return result;
    }

    // 替换为fake-ip
    sockaddr_in *origin_from = (sockaddr_in *)lpFrom;
    sockaddr_in fake_from = fake_addr_pool.get_fake_addr(origin_from);
    fake_from.sin_port = origin_from->sin_port;
    memcpy(lpFrom, &fake_from, sizeof(sockaddr_in));
    *lpFromlen = sizeof(sockaddr_in);

    write_debug_ex(result == 0, , "original wsarecvfrom: %s, from: %s(origin-ip) -> %s(fake-ip), %d, result: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(*origin_from).c_str(), std::to_string(*((sockaddr_in *)lpFrom)).c_str(), s, result);
    return result;
}

static int WINAPI fake_bind(SOCKET s, const sockaddr *addr, int addrlen)
{
    // 将监听 0.0.0.0 的 socket 替换成监听 [::] 的 socket
    SockAddrIn origin_v4 = SockAddrIn::from_sockaddr(addr);
    if (origin_v4.get_family() == AF_INET && origin_v4.addr.addr_v4.sin_addr.S_un.S_addr == INADDR_ANY) {
        if (SockAddrIn::from_addr(address).get_family() == AF_INET6) {
            // 获取原socket绑定的地址端口
            SockAddrIn new_v6;
            new_v6.set_family(AF_INET6);
            new_v6.set_ip_any();
            new_v6.set_port_raw(origin_v4.addr.addr_v4.sin_port);

            // 创建新socket
            SOCKET sock = socket(AF_INET6, SOCK_DGRAM, 0);
            if (sock == INVALID_SOCKET) {
                int errorcode = WSAGetLastError();
                write_log("create socket failed while bind: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_v4).c_str(), std::to_string(new_v6).c_str(), s, sock, errorcode);
                return SOCKET_ERROR;
            }

            ULONG on = 1;
            ioctlsocket(sock, FIONBIO, &on); // 设置为非阻塞, 非常重要，尤其是对于 62056 端口不可以去掉
            BOOL opt = TRUE;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(BOOL)); // 允许发送广播
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口

            // 绑定地址端口
            int result = _bind(sock, new_v6.get_sockaddr(), new_v6.get_len());
            if (result == SOCKET_ERROR) {
                int errorcode = WSAGetLastError();
                write_log("bind socket failed while bind: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_v4).c_str(), std::to_string(new_v6).c_str(), s, sock, errorcode);
                return result;
            }

            socks[s] = sock;
            write_socks();
            write_debug("replace origin_bind: %s -> %s, %d -> %d\n", std::to_string(origin_v4).c_str(), std::to_string(new_v6).c_str(), s, sock);
        }
    }

    // 原样绑定
    int result = _bind(s, addr, addrlen);
    if (result == SOCKET_ERROR) {
        int errorcode = WSAGetLastError();
        write_log("original bind failed: %s, %d, errorcode: %d\n", std::to_string(SockAddrIn::from_sockaddr(addr)).c_str(), s, errorcode);
        return result;
    }
    write_debug("original bind: %s, %d\n", std::to_string(SockAddrIn::from_sockaddr(addr)).c_str(), s);
    return result;
}

static int WINAPI fake_closesocket(SOCKET s)
{
    if (socks.count(s)) {
        write_debug("close socket: %d -> %d\n", s, socks[s]);
        _closesocket(socks[s]);
        socks.erase(s);
        write_socks();
    }
    return _closesocket(s);
}

static int WINAPI fake_select(int n, fd_set *rd, fd_set *wr, fd_set *ex, const TIMEVAL *timeout)
{
    if (rd && rd->fd_count == 1) {
        SOCKET s = rd->fd_array[0];
        if (socks.count(s)) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(socks[s], &fds);
            int r = _select(0, &fds, NULL, NULL, timeout);
            if (r > 0) {
                return fds.fd_count;
            }
            rd->fd_count = 0;
            return 0;
        }
    }
    return _select(n, rd, wr, ex, timeout);
}

/*
==================== Hook Functions ====================
*/

template <typename T>
void hook_func(const char *func_name, T* new_func, T *&p_old_func) {
    HMODULE hModule = GetModuleHandleA("ws2_32.dll");
    auto handler = GetProcAddress(hModule, func_name);
    MH_STATUS status = MH_CreateHook(reinterpret_cast<LPVOID*>(handler), reinterpret_cast<LPVOID*>(new_func), reinterpret_cast<LPVOID*>(&p_old_func));
    if (status != MH_OK) {
        write_log("hook %s failed: %d\n", func_name, status);
    }
    status = MH_EnableHook(reinterpret_cast<LPVOID*>(handler));
    if (status != MH_OK) {
        write_log("enable %s hook failed: %d\n", func_name, status);
    }
}

void unhook_func(const char *func_name) {
    HMODULE hModule = GetModuleHandleA("ws2_32.dll");
    auto handler = GetProcAddress(hModule, func_name);
    MH_STATUS status = MH_DisableHook(reinterpret_cast<LPVOID*>(handler));
    if (status != MH_OK) {
        write_log("disable %s hook failed: %d\n", func_name, status);
    }
    status = MH_RemoveHook(reinterpret_cast<LPVOID*>(handler));
    if (status != MH_OK) {
        write_log("remove hook %s failed: %d\n", func_name, status);
    }
}

void init_hooklib()
{
    static std::once_flag once_flag;
    std::call_once(once_flag, []() {
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK) {
            write_log("init hooklib failed: %d\n", status);
        }
    });
}

void uninit_hooklib()
{
    MH_STATUS status = MH_Uninitialize();
    if (status != MH_OK) {
        write_log("uninit hooklib failed: %d\n", status);
    }
}

void hook()
{
    init_hooklib();
    read_config();
    hook_func("bind", fake_bind, _bind);
    hook_func("sendto", fake_sendto, _sendto);
    hook_func("select", fake_select, _select);
    hook_func("recvfrom", fake_recvfrom, _recvfrom);
    hook_func("closesocket", fake_closesocket, _closesocket);
    hook_func("WSASendTo", fake_wsasendto, _wsasendto);
    hook_func("WSARecvFrom", fake_wsarecvfrom, _wsarecvfrom);
}

void unhook()
{
    for (auto it = socks.begin(); it != socks.end(); it++) {
        _closesocket(it->second);
    }
    unhook_func("bind");
    unhook_func("sendto");
    unhook_func("select");
    unhook_func("recvfrom");
    unhook_func("closesocket");
    unhook_func("WSASendTo");
    unhook_func("WSARecvFrom");
    uninit_hooklib();
}
