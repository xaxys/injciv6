#include <winsock2.h>

#include "Minhook.h"
#include "fkhook.h"
#include "platform.h"
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

typedef PCSTR WINAPI (*inet_ntop_func) (int, const void *, char *, socklen_t);
typedef unsigned long WINAPI (*inet_addr_func) (const char *);
typedef u_short WINAPI (*ntohs_func) (u_short);
typedef int WINAPI (*getsockname_func) (SOCKET, sockaddr *, int *);
typedef int WINAPI (*getaddrinfo_func) (const char *, const char *, const addrinfo *, addrinfo **);
typedef void WINAPI (*freeaddrinfo_func) (addrinfo *);
typedef int WINAPI (*wsagetlasterror_func) ();
typedef SOCKET WINAPI (*socket_func) (int, int, int);
typedef int WINAPI (*ioctlsocket_func) (SOCKET, long, u_long *);
typedef int WINAPI (*setsockopt_func) (SOCKET, int, int, const char *, int);

static inet_ntop_func _inet_ntop = NULL;
static inet_addr_func _inet_addr = NULL;
static ntohs_func _ntohs = NULL;
static getsockname_func _getsockname = NULL;
static getaddrinfo_func _getaddrinfo = NULL;
static freeaddrinfo_func _freeaddrinfo = NULL;
static wsagetlasterror_func _wsagetlasterror = NULL;
static socket_func _socket = NULL;
static ioctlsocket_func _ioctlsocket = NULL;
static setsockopt_func _setsockopt = NULL;

const struct in6_addr in6addr_any = {0};

namespace std {
    std::string to_string(const struct in_addr& addr) {
        char str[INET_ADDRSTRLEN];
        memset(str, 0, sizeof(str));
        _inet_ntop(AF_INET, &addr, str, sizeof(str));
        return str;
    }

    std::string to_string(const struct in6_addr& addr) {
        char str[INET6_ADDRSTRLEN];
        memset(str, 0, sizeof(str));
        _inet_ntop(AF_INET6, &addr, str, sizeof(str));
        return str;
    }

    std::string to_string(const struct sockaddr_in& addr) {
        return to_string(addr.sin_addr) + ":" + to_string(_ntohs(addr.sin_port));
    }

    std::string to_string(const struct sockaddr_in6& addr) {
        return to_string(addr.sin6_addr) + ":" + to_string(_ntohs(addr.sin6_port));
    }
}

struct SockAddrIn {
    union {
        sockaddr_in addr_v4;
        sockaddr_in6 addr_v6;
    } addr;

    SockAddrIn() {
        memset(&addr, 0, sizeof(addr));
    }

    static SockAddrIn from_socket(SOCKET sock) {
        SockAddrIn addr_in;
        if (sock == INVALID_SOCKET) return addr_in;
        int namelen = sizeof(addr_in.addr);
        _getsockname(sock, (sockaddr *)&addr_in.addr, &namelen);
        return addr_in;
    }

    static SockAddrIn from_sockaddr(const sockaddr *addr) {
        SockAddrIn addr_in;
        if (!addr) return addr_in;
        memcpy(&addr_in.addr, addr, get_len(addr->sa_family));
        return addr_in;
    }

    static SockAddrIn from_addr(const char *addr) {
        SockAddrIn addr_in;
        from_addr(addr, &addr_in);
        return addr_in;
    }

    static bool from_addr(const char *addr, SockAddrIn *addr_in) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC; // 不指定是IPv4还是IPv6

        int err = _getaddrinfo(addr, NULL, &hints, &res);
        if (err != 0) {
            return false;
        }

        int len = get_len(res->ai_family);
        memcpy(&addr_in->addr, res->ai_addr, len);
        _freeaddrinfo(res);
        return len > 0;
    }

    sockaddr *get_sockaddr() const {
        return (sockaddr *)&addr;
    }

    int get_family() const {
        return get_sockaddr()->sa_family;
    }

    int get_len() const {
        return get_len(get_family());
    }

    static int get_len(int family) {
        switch (family) {
        case AF_INET:
            return sizeof(sockaddr_in);
        case AF_INET6:
            return sizeof(sockaddr_in6);
        default:
            return 0;
        }
    }

    std::string get_ip() const {
        int family = get_family();
        switch (family) {
        case AF_INET:
            return std::to_string(addr.addr_v4.sin_addr);
        case AF_INET6:
            return std::to_string(addr.addr_v6.sin6_addr);
        default:
            return "";
        }
    }
    
    int get_port() const {
        int family = get_family();
        switch (family) {
        case AF_INET:
            return _ntohs(addr.addr_v4.sin_port);
        case AF_INET6:
            return _ntohs(addr.addr_v6.sin6_port);
        default:
            return 0;
        }
    }

    void set_ip_any() {
        int family = get_family();
        if (family == AF_INET) {
            addr.addr_v4.sin_addr.s_addr = INADDR_ANY;
        } else if (family == AF_INET6) {
            addr.addr_v6.sin6_addr = in6addr_any;
        }
    }

    void set_port_raw(USHORT port) {
        int family = get_family();
        if (family == AF_INET) {
            addr.addr_v4.sin_port = port;
        } else if (family == AF_INET6) {
            addr.addr_v6.sin6_port = port;
        }
    }
};

namespace std {
    std::string to_string(const SockAddrIn &addr) {
        return addr.get_ip() + ":" + std::to_string(addr.get_port());
    }
}

struct FakeAddrPool {

    struct IPv4Addr {
        in_addr addr;

        IPv4Addr() {
            memset(&addr, 0, sizeof(in_addr));
        }

        IPv4Addr(const in_addr& addr) {
            this->addr = addr;
        }

        IPv4Addr(ULONG addr) {
            memset(&this->addr, 0, sizeof(in_addr));
            this->addr.s_addr = addr;
        }

        bool operator<(const IPv4Addr& other) const {
            return memcmp(&addr, &other.addr, sizeof(in_addr)) < 0;
        }

        bool operator==(const IPv4Addr& other) const {
            return memcmp(&addr, &other.addr, sizeof(in_addr)) == 0;
        }
    };

    struct IPv6Addr {
        in6_addr addr;

        IPv6Addr() {
            memset(&addr, 0, sizeof(in6_addr));
        }

        IPv6Addr(const in6_addr& addr) {
            this->addr = addr;
        }

        bool operator<(const IPv6Addr& other) const {
            return memcmp(&addr, &other.addr, sizeof(in6_addr)) < 0;
        }

        bool operator==(const IPv6Addr& other) const {
            return memcmp(&addr, &other.addr, sizeof(in6_addr)) == 0;
        }
    };

    std::map<IPv4Addr, IPv6Addr> v4_to_v6;
    std::map<IPv6Addr, IPv4Addr> v6_to_v4;
    ULONG start;
    ULONG end;
    ULONG current;

    FakeAddrPool(ULONG start, ULONG len) {
        this->start = start;
        this->end = this->start + len;
        this->current = this->start;
    }

    sockaddr_in get_fake_addr(const sockaddr_in6 *addr) {
        ULONG fake_ip;
        IPv6Addr addr6(addr->sin6_addr);
        if (v6_to_v4.count(addr6)) {
            fake_ip = v6_to_v4[addr6].addr.s_addr;
        } else {
            fake_ip = search_available_addr(current);
            IPv4Addr addr4(fake_ip);
            v6_to_v4[addr6] = addr4;
            v4_to_v6[addr4] = addr6;
        }
        sockaddr_in fake_addr;
        memset(&fake_addr, 0, sizeof(fake_addr));
        fake_addr.sin_family = AF_INET;
        fake_addr.sin_addr.S_un.S_addr = fake_ip;
        return fake_addr;
    }

    sockaddr_in6 get_origin_addr(const sockaddr_in *addr) {
        sockaddr_in6 origin_addr;
        memset(&origin_addr, 0, sizeof(origin_addr));
        IPv4Addr addr4(addr->sin_addr);
        if (v4_to_v6.count(addr4)) {
            IPv6Addr addr6 = v4_to_v6[addr4];
            origin_addr.sin6_family = AF_INET6;
            origin_addr.sin6_addr = addr6.addr;
        }
        return origin_addr;
    }

    bool has_origin_addr(const sockaddr_in *addr) {
        return v4_to_v6.count(IPv4Addr(addr->sin_addr));
    }

    ULONG search_available_addr(ULONG search_start) {
        if (!v4_to_v6.count(IPv4Addr(search_start))) {
            return search_start;
        }
        ULONG addr = search_start + 1;
        while (addr != search_start) {
            if (!v4_to_v6.count(IPv4Addr(addr))) {
                return addr;
            }
            addr++;
            if (addr == end) {
                addr = start;
            }
        }
        return search_start;
    }
};

// #define DEBUG_ENABLE
#define LOG_ENABLE

static const ULONG FAKE_IP_RANGE = 100;
static const ULONG FAKE_IP_START = 0x017F007F; // 127.0.127.1

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
static std::unordered_map<SOCKET, SOCKET> socks;
static FakeAddrPool fake_addr_pool(FAKE_IP_START, FAKE_IP_RANGE);
static char address[256] = "255.255.255.255";

#ifdef DEBUG_ENABLE
static void write_debug_impl(const char *fmt, ...)
{
    // FILE *fp = fopen("injbg3-debug.txt", "r");
    // if (!fp) return;
    FILE *fp = fopen("injbg3-debug.txt", "a+");
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
#define write_socks() write_socks_impl()
#else
#define write_debug(fmt, ...)
#define write_socks()
#endif

#ifdef LOG_ENABLE
static void write_log_impl(const char *fmt, ...)
{
    FILE *fp = fopen("injbg3-log.txt", "a+");
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
#else
#define write_log(fmt, ...)
#endif

static void read_config()
{
    FILE *fp = fopen("injbg3-config.txt", "r");
    if (fp) {
        fscanf(fp, "%s", address);
    } else {
        fp = fopen("injbg3-config.txt", "w+");
        if (fp) {
            fprintf(fp, "%s", address);
        }
    }
    fclose(fp);
}

// 如果需要使用这个函数，需要在编译时链接 iphlpapi.lib，即 linkflags 加上 -l iphlpapi
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
        write_log("GetAdaptersInfo failed: %d\n", dwRetVal);
        return broadcast_ip;
    }

    for (PIP_ADAPTER_INFO pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
        // 检查ip是否在当前适配器的子网内
        ULONG adpt_ip = _inet_addr(pAdapter->IpAddressList.IpAddress.String);
        ULONG mask = _inet_addr(pAdapter->IpAddressList.IpMask.String);
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

// hook后替换的函数
static int WINAPI fake_wsasendto(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const sockaddr *lpTo, int iTolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    sockaddr_in *origin_to = (sockaddr_in *)lpTo;
    if (fake_addr_pool.has_origin_addr(origin_to)) {
        sockaddr_in6 new_to = fake_addr_pool.get_origin_addr(origin_to);
        new_to.sin6_port = origin_to->sin_port;
        int result = _wsasendto(socks[s], lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, (sockaddr *)&new_to, sizeof(sockaddr_in6), lpOverlapped, lpCompletionRoutine);

#ifdef LOG_ENABLE
        if (result == SOCKET_ERROR) {
            int errorcode = _wsagetlasterror();
            write_log("fake-ip  wsasendto failed: %s -> %s, through: %s, %d -> %d, result: %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), s, socks[s], result, errorcode);
        }
#endif

#ifdef DEBUG_ENABLE
        SockAddrIn local_addr = SockAddrIn::from_socket(socks[s]);
        write_debug("fake-ip  wsasendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(local_addr).c_str(), s, socks[s], result);
#endif

        return result;
    }

    if (!is_broadcast_ip(origin_to->sin_addr)) {
        int result = _wsasendto(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine);
#ifdef DEBUG_ENABLE
        if (result == SOCKET_ERROR) {
            int errorcode = _wsagetlasterror();
            write_debug("original wsasendto failed: %d\n", errorcode);
        }
        write_debug("original wsasendto: %s, %d, result: %d\n", std::to_string(*origin_to).c_str(), s, result);
#endif
        return result;
    }

    // 重定向广播
    int result = SOCKET_ERROR;

    SockAddrIn new_to;
    if (!SockAddrIn::from_addr(address, &new_to)) {
        write_log("parse address failed: %s\n", address);
        return true;
    }
    new_to.set_port_raw(origin_to->sin_port);

    SOCKET new_sock = s;

    if (socks.count(s)) {
        new_sock = socks[s];
    } else if (new_to.get_family() == AF_INET6) {
        // 获取原sockaddr
        sockaddr_in origin_local_addr;
        origin_local_addr.sin_family = AF_INET;
        int namelen = sizeof(sockaddr_in);
        _getsockname(s, (sockaddr *)&origin_local_addr, &namelen); // 获取原sockaddr
        if (origin_local_addr.sin_port == 0) {
            // 如果没有端口号，先原样发送，这样系统才会分配一个端口号
            result = _wsasendto(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iTolen, lpOverlapped, lpCompletionRoutine);
            _getsockname(s, (sockaddr *)&origin_local_addr, &namelen); // 重新获取
        }

        SockAddrIn new_local_addr;
        new_local_addr.addr.addr_v4.sin_family = new_to.get_family();
        new_local_addr.set_ip_any();
        new_local_addr.set_port_raw(origin_local_addr.sin_port);

        SOCKET sock = _socket(new_local_addr.get_family(), SOCK_DGRAM, 0);
#ifdef LOG_ENABLE
        if (sock == INVALID_SOCKET) {
            int errorcode = _wsagetlasterror();
            write_log("create socket failed while wsasendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
            return SOCKET_ERROR;
        }
#endif

        ULONG on = 1;
        _ioctlsocket(sock, FIONBIO, &on); // 设置为非阻塞, 非常重要，尤其是对于 steam 版的 steam 好友发现 47584 端口不可以去掉
        BOOL opt = TRUE;
        _setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(BOOL)); // 允许发送广播
        _setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口

        result = _bind(sock, new_local_addr.get_sockaddr(), new_local_addr.get_len()); // 绑定地址端口
#ifdef LOG_ENABLE
        if (result == SOCKET_ERROR) {
            int errorcode = _wsagetlasterror();
            write_log("bind failed while wsasendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
            return SOCKET_ERROR;
        }
#endif

        socks[s] = sock;

        write_socks();
        write_debug("replace origin_sock: %s -> %s, %d -> %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock);

        new_sock = sock;
    }

    result = _wsasendto(new_sock, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, new_to.get_sockaddr(), new_to.get_len(), lpOverlapped, lpCompletionRoutine);

#ifdef DEBUG_ENABLE
    if (result == SOCKET_ERROR) {
        int errorcode = _wsagetlasterror();
        write_debug("redirect wsasendto failed: %d\n", errorcode);
    }
    SockAddrIn local_addr = SockAddrIn::from_socket(new_sock);
    if (result == 0) write_debug("redirect wsasendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(local_addr).c_str(), s, new_sock, result);
#endif

    return result;
}

static int WINAPI fake_wsarecvfrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    if (socks.count(s)) {
        SockAddrIn new_from;
        int new_from_len = sizeof(new_from.addr);
        int result = _wsarecvfrom(socks[s], lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, new_from.get_sockaddr(), &new_from_len, lpOverlapped, lpCompletionRoutine);

#ifdef LOG_ENABLE
        if (result == SOCKET_ERROR) {
            int errorcode = _wsagetlasterror();
            if (errorcode != WSAEWOULDBLOCK) write_log("redirect wsarecvfrom failed: %s -> %s, %d -> %d, result: %d, errorcode: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), s, socks[s], result, errorcode);
        }
#endif
        if (result == SOCKET_ERROR) return result;

        switch (new_from.get_family()) {
        case AF_INET: // 如果是v4地址，直接返回
            {
                memcpy(lpFrom, &new_from.addr.addr_v4, sizeof(sockaddr_in));
                break;
            }
        case AF_INET6: // 如果是v6地址，替换成v4的fake-ip地址
            {
                sockaddr_in fake_from = fake_addr_pool.get_fake_addr((sockaddr_in6 *)&new_from.addr.addr_v6);
                fake_from.sin_port = new_from.addr.addr_v6.sin6_port;
                memcpy(lpFrom, &fake_from, sizeof(sockaddr_in));
                *lpFromlen = sizeof(sockaddr_in);
                break;
            }
        default:
            write_log("unknown address family while wsarecvfrom: %d, %d -> %d\n", new_from.get_family(), s, socks[s]);
        }

#ifdef DEBUG_ENABLE
        if (result != SOCKET_ERROR) {
            SockAddrIn origin_local_addr = SockAddrIn::from_socket(s);
            SockAddrIn new_local_addr = SockAddrIn::from_socket(socks[s]);
            write_debug("redirect wsarecvfrom: %s -> %s, from: %s, %d -> %d, result: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), std::to_string(new_from).c_str(), s, socks[s], result);
            if (new_from.get_family() == AF_INET6) {
                write_debug("    fake-ip: %s -> %s\n", std::to_string(new_from).c_str(), std::to_string(*((sockaddr_in *)lpFrom)).c_str());
            } 
        }
#endif

        return result;
    }
    // 非替换的socket，原样接收
    int result = _wsarecvfrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

#ifdef DEBUG_ENABLE
    if (result == SOCKET_ERROR) {
        int errorcode = _wsagetlasterror();
        write_debug("original wsarecvfrom failed: %d\n", errorcode);
    }
    if (result != SOCKET_ERROR) write_debug("original wsarecvfrom: %s, %d, result: %d\n", std::to_string(*((sockaddr_in *)lpFrom)).c_str(), s, result);
#endif

    return result;
}

static int WINAPI fake_bind(SOCKET s, const sockaddr *addr, int addrlen)
{
    // 将监听 0.0.0.0 的 socket 替换成监听 [::] 的 socket
    SockAddrIn origin_addr_v4 = SockAddrIn::from_sockaddr(addr);
    if (origin_addr_v4.get_family() == AF_INET && origin_addr_v4.addr.addr_v4.sin_addr.S_un.S_addr == INADDR_ANY) {
        if (SockAddrIn::from_addr(address).get_family() == AF_INET6) {

            sockaddr_in6 new_addr_v6;
            new_addr_v6.sin6_family = AF_INET6;
            new_addr_v6.sin6_flowinfo = 0;
            new_addr_v6.sin6_scope_id = 0;
            new_addr_v6.sin6_addr = in6addr_any;
            new_addr_v6.sin6_port = origin_addr_v4.addr.addr_v4.sin_port;

            SOCKET sock = _socket(AF_INET6, SOCK_DGRAM, 0);

#ifdef LOG_ENABLE
            if (sock == INVALID_SOCKET) {
                int errorcode = _wsagetlasterror();
                write_log("create socket failed while bind: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_addr_v4).c_str(), std::to_string(new_addr_v6).c_str(), s, sock, errorcode);
                return SOCKET_ERROR;
            }
#endif

            ULONG on = 1;
            _ioctlsocket(sock, FIONBIO, &on); // 设置为非阻塞, 非常重要，尤其是对于 62056 端口不可以去掉
            BOOL opt = TRUE;
            _setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(BOOL)); // 允许发送广播
            _setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口

            int result = _bind(sock, (sockaddr *)&new_addr_v6, sizeof(new_addr_v6));

#ifdef LOG_ENABLE
            if (result == SOCKET_ERROR) {
                int errorcode = _wsagetlasterror();
                write_log("bind socket failed while bind: %s -> %s, %d -> %d, result: %s, errorcode: %d\n", std::to_string(origin_addr_v4).c_str(), std::to_string(new_addr_v6).c_str(), s, sock, result, errorcode);
                return SOCKET_ERROR;
            }
#endif
            socks[s] = sock;
            write_socks();
            write_debug("replace origin_bind: %s -> %s, %d -> %d\n", std::to_string(origin_addr_v4).c_str(), std::to_string(new_addr_v6).c_str(), s, sock);
        }
    }
    return _bind(s, addr, addrlen);
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

static int WINAPI fake_sendto(SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen)
{
    sockaddr_in *origin_to = (sockaddr_in *)to;

    if (fake_addr_pool.has_origin_addr(origin_to)) {
        sockaddr_in6 new_to = fake_addr_pool.get_origin_addr(origin_to);
        new_to.sin6_port = origin_to->sin_port;
        int result = _sendto(socks[s], buf, len, flags, (sockaddr *)&new_to, sizeof(sockaddr_in6));
#ifdef LOG_ENABLE
        if (result == SOCKET_ERROR) {
            int errorcode = _wsagetlasterror();
            write_log("fake-ip  sendto failed: %s -> %s, through: %s, %d -> %d, result: %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), s, socks[s], result, errorcode);
        }
#endif

#ifdef DEBUG_ENABLE
        SockAddrIn local_addr = SockAddrIn::from_socket(socks[s]);
        write_debug("fake-ip  sendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(local_addr).c_str(), s, socks[s], result);
#endif

        return result;
    }

    if (!is_broadcast_ip(origin_to->sin_addr)) {
        int result = _sendto(s, buf, len, flags, to, tolen); // 非广播直接原样发送
        return result;
    }

    // 重定向广播
    int result = SOCKET_ERROR;

    SockAddrIn new_to;
    if (!SockAddrIn::from_addr(address, &new_to)) {
        write_log("parse address failed: %s\n", address);
        return true;
    }
    new_to.set_port_raw(origin_to->sin_port);

    SOCKET new_sock = s;

    if (socks.count(s)) {
        new_sock = socks[s];
    } else if (new_to.get_family() == AF_INET6) {
        // 获取原sockaddr
        sockaddr_in origin_local_addr;
        origin_local_addr.sin_family = AF_INET;
        int namelen = sizeof(sockaddr_in);
        _getsockname(s, (sockaddr *)&origin_local_addr, &namelen); // 获取原sockaddr
        if (origin_local_addr.sin_port == 0) {
            // 如果没有端口号，先原样发送，这样系统才会分配一个端口号
            result = _sendto(s, buf, len, flags, to, tolen);
            _getsockname(s, (sockaddr *)&origin_local_addr, &namelen); // 重新获取
        }

        SockAddrIn new_local_addr;
        new_local_addr.addr.addr_v4.sin_family = new_to.get_family();
        new_local_addr.set_ip_any();
        new_local_addr.set_port_raw(origin_local_addr.sin_port);

        SOCKET sock = _socket(new_local_addr.get_family(), SOCK_DGRAM, 0);
#ifdef LOG_ENABLE
        if (sock == INVALID_SOCKET) {
            int errorcode = _wsagetlasterror();
            write_log("create socket failed while sendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
            return SOCKET_ERROR;
        }
#endif

        ULONG on = 1;
        _ioctlsocket(sock, FIONBIO, &on); // 设置为非阻塞, 非常重要，尤其是对于 steam 版的 steam 好友发现 47584 端口不可以去掉
        BOOL opt = TRUE;
        _setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(BOOL)); // 允许发送广播
        _setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口

        result = _bind(sock, new_local_addr.get_sockaddr(), new_local_addr.get_len()); // 绑定地址端口
#ifdef LOG_ENABLE
        if (result == SOCKET_ERROR) {
            int errorcode = _wsagetlasterror();
            write_log("bind failed while sendto: %s -> %s, %d -> %d, errorcode: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock, errorcode);
            return SOCKET_ERROR;
        }
#endif

        socks[s] = sock;

        write_socks();
        write_debug("replace origin_sock: %s -> %s, %d -> %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), s, sock);

        new_sock = sock;
    }

    result = _sendto(new_sock, buf, len, flags, new_to.get_sockaddr(), new_to.get_len());

#ifdef DEBUG_ENABLE
    if (result == SOCKET_ERROR) {
        int errorcode = _wsagetlasterror();
        write_debug("redirect sendto failed: %s -> %s, through: %s, %d -> %d, errorcode: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(SockAddrIn::from_socket(new_sock)).c_str(), s, new_sock, errorcode);
    }
    SockAddrIn local_addr = SockAddrIn::from_socket(new_sock);
    if (result > 0) write_debug("redirect sendto: %s -> %s, through: %s, %d -> %d, result: %d\n", std::to_string(*origin_to).c_str(), std::to_string(new_to).c_str(), std::to_string(local_addr).c_str(), s, new_sock, result);
#endif

    return result;
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
                // write_debug("redirect select: %d -> %d\n", s, socks[s]);
                return fds.fd_count;
            }
            rd->fd_count = 0;
            return 0;
        }
    }
    return _select(n, rd, wr, ex, timeout);
}

static int WINAPI fake_recvfrom(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
    if (socks.count(s)) {
        SockAddrIn new_from;
        int new_from_len = sizeof(new_from.addr);
        int result = _recvfrom(socks[s], buf, len, flags, new_from.get_sockaddr(), &new_from_len);
#ifdef LOG_ENABLE
        if (result == SOCKET_ERROR) {
            int errorcode = _wsagetlasterror();
            if (errorcode != WSAEWOULDBLOCK) write_log("redirect recvfrom failed: %s -> %s, %d -> %d, result: %d, errorcode: %d\n", std::to_string(SockAddrIn::from_socket(s)).c_str(), std::to_string(SockAddrIn::from_socket(socks[s])).c_str(), s, socks[s], result, errorcode);
            return SOCKET_ERROR;
        }
#endif

        switch (new_from.get_family()) {
        case AF_INET: // 如果是v4地址，直接返回
            {
                memcpy(from, &new_from.addr.addr_v4, sizeof(sockaddr_in));
                break;
            }
        case AF_INET6: // 如果是v6地址，替换成v4的fake-ip地址
            {
                sockaddr_in fake_from = fake_addr_pool.get_fake_addr((sockaddr_in6 *)&new_from.addr.addr_v6);
                fake_from.sin_port = new_from.addr.addr_v6.sin6_port;
                memcpy(from, &fake_from, sizeof(sockaddr_in));
                *fromlen = sizeof(sockaddr_in);
                break;
            }
        default:
            write_log("unknown address family: %d, %d -> %d\n", new_from.get_family(), s, socks[s]);
        }
#ifdef DEBUG_ENABLE
        if (result > 0) {
            SockAddrIn origin_local_addr = SockAddrIn::from_socket(s);
            SockAddrIn new_local_addr = SockAddrIn::from_socket(socks[s]);
            write_debug("redirect recvfrom: %s -> %s, from: %s, %d -> %d, result: %d\n", std::to_string(origin_local_addr).c_str(), std::to_string(new_local_addr).c_str(), std::to_string(new_from).c_str(), s, socks[s], result);
            if (new_from.get_family() == AF_INET6) {
                write_debug("    fake-ip: %s -> %s\n", std::to_string(new_from).c_str(), std::to_string(*((sockaddr_in *)from)).c_str());
            } 
        }
#endif
        return result;
    }
    // 非替换的socket，原样接收
    int result = _recvfrom(s, buf, len, flags, from, fromlen);

#ifdef DEBUG_ENABLE
    if (result == SOCKET_ERROR) {
        int errorcode = _wsagetlasterror();
        write_debug("original recvfrom failed: %d\n", errorcode);
    }
    if (result > 0) write_debug("original recvfrom: %s, %d, result: %d\n", std::to_string(*((sockaddr_in *)from)).c_str(), s, result);
#endif

    return result;
}

void init_tool_func() {
    HMODULE hModule = GetModuleHandleA("ws2_32.dll");
    _inet_ntop = (inet_ntop_func)GetProcAddress(hModule, "inet_ntop");
    _inet_addr = (inet_addr_func)GetProcAddress(hModule, "inet_addr");
    _ntohs = (ntohs_func)GetProcAddress(hModule, "ntohs");
    _getsockname = (getsockname_func)GetProcAddress(hModule, "getsockname");
    _getaddrinfo = (getaddrinfo_func)GetProcAddress(hModule, "getaddrinfo");
    _freeaddrinfo = (freeaddrinfo_func)GetProcAddress(hModule, "freeaddrinfo");
    _wsagetlasterror = (wsagetlasterror_func)GetProcAddress(hModule, "WSAGetLastError");
    _socket = (socket_func)GetProcAddress(hModule, "socket");
    _ioctlsocket = (ioctlsocket_func)GetProcAddress(hModule, "ioctlsocket");
    _setsockopt = (setsockopt_func)GetProcAddress(hModule, "setsockopt");
}

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
    init_tool_func();
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
