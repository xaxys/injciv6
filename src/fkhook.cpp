#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <unordered_map>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "fkhook.h"
#include "platform.h"

struct SockAddrIn {
    union {
        sockaddr_in addr_v4;
        sockaddr_in6 addr_v6;
    } addr;

    static SockAddrIn from_socket(SOCKET sock) {
        SockAddrIn addr_in;
        if (sock == INVALID_SOCKET) return addr_in;
        int namelen = sizeof(addr_in.addr);
        getsockname(sock, (sockaddr *)&addr_in.addr, &namelen);
        return addr_in;
    }

    static SockAddrIn from_sockaddr(const sockaddr *addr) {
        SockAddrIn addr_in;
        if (!addr) return addr_in;
        memcpy(&addr_in.addr, addr, addr_in.get_len());
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

        int err = getaddrinfo(addr, NULL, &hints, &res);
        if (err != 0) {
            return false;
        }

        int len = get_len(res->ai_family);
        memcpy(&addr_in->addr, res->ai_addr, len);
        freeaddrinfo(res);
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
        char str[INET6_ADDRSTRLEN];
        int family = get_family();
        if (family == AF_INET) {
            inet_ntop(AF_INET, &addr.addr_v4.sin_addr, str, sizeof(str));
        } else if (family == AF_INET6) {
            inet_ntop(AF_INET6, &addr.addr_v6.sin6_addr, str, sizeof(str));
        } else {
            str[0] = '\0';
        }
        return str;
    }
    
    int get_port() const {
        char str[6];
        int family = get_family();
        if (family == AF_INET) {
            return ntohs(addr.addr_v4.sin_port);
        } else if (family == AF_INET6) {
            return ntohs(addr.addr_v6.sin6_port);
        } else {
            return 0;
        }
    }

    void set_ip_any() {
        int family = get_family();
        if (family == AF_INET) {
            addr.addr_v4.sin_addr.s_addr = htonl(INADDR_ANY);
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

struct FakeAddrPool {

    struct IPv4Addr {
        in_addr addr;

        IPv4Addr() {
            this->addr.s_addr = 0;
        }

        IPv4Addr(const in_addr& addr) {
            this->addr = addr;
        }

        IPv4Addr(ULONG addr) {
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
        this->end = start + len;
        this->current = start;
    }

    SockAddrIn get_fake_addr(const SockAddrIn &addr) {
        SockAddrIn fake_addr;
        if (addr.get_family() != AF_INET6) return fake_addr;
        ULONG fake_ip;
        IPv6Addr addr6(addr.addr.addr_v6.sin6_addr);
        if (v6_to_v4.count(addr6)) {
            fake_ip = v6_to_v4[addr6].addr.s_addr;
        } else {
            fake_ip = search_available_addr(current);
            IPv4Addr addr4(fake_ip);
            v6_to_v4[addr6] = addr4;
            v4_to_v6[addr4] = addr6;
        }
        sockaddr_in fake_addr_v4;
        fake_addr_v4.sin_family = AF_INET;
        fake_addr_v4.sin_addr.S_un.S_addr = fake_ip;

        fake_addr = SockAddrIn::from_sockaddr((sockaddr *)&fake_addr_v4);
        return fake_addr;
    }

    SockAddrIn get_origin_addr(const SockAddrIn &addr) {
        SockAddrIn origin_addr;
        if (addr.get_family() != AF_INET) return origin_addr;

        IPv4Addr addr4(addr.addr.addr_v4.sin_addr);
        if (v4_to_v6.count(addr4)) {
            IPv6Addr addr6 = v4_to_v6[addr4];
            origin_addr.addr.addr_v6.sin6_family = AF_INET6;
            origin_addr.addr.addr_v6.sin6_addr = addr6.addr;
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

#ifdef _CPU_X86
#define SENDTO_ENTRY_LEN 5
#define SELECT_ENTRY_LEN 7
#define RECVFROM_ENTRY_LEN 5
#define CLOSESOCKET_ENTRY_LEN 5
#endif
#ifdef _CPU_X64
#define SENDTO_ENTRY_LEN 7
#define SELECT_ENTRY_LEN 7
#define RECVFROM_ENTRY_LEN 7
#define CLOSESOCKET_ENTRY_LEN 7
#endif

// #define DEBUG_ENABLE

static const ULONG FAKE_IP_RANGE = 100;
static const ULONG FAKE_IP_START = inet_addr("127.0.127.1");

typedef int WINAPI(*sendto_func) (SOCKET, const char *, int, int, const sockaddr *, int);
typedef int WINAPI(*select_func) (int, fd_set *, fd_set *, fd_set *, const TIMEVAL *);
typedef int WINAPI(*recvfrom_func) (SOCKET, char *, int, int, sockaddr *, int *);
typedef int WINAPI(*closesocket_func) (SOCKET);

static InlineHook *sendto_hook = NULL;
static InlineHook *select_hook = NULL;
static InlineHook *recvfrom_hook = NULL;
static InlineHook *closesocket_hook = NULL;
static sendto_func _sendto = NULL;
static select_func _select = NULL;
static recvfrom_func _recvfrom = NULL;
static closesocket_func _closesocket = NULL;
static std::unordered_map<SOCKET, SOCKET> socks;
static FakeAddrPool fake_addr_pool(FAKE_IP_START, FAKE_IP_RANGE);
static char address[256] = "255.255.255.255";

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
#define write_debug(fmt, ...) write_debug_impl(fmt, __VA_ARGS__)
#else
#define write_debug(fmt, ...)
#endif

static void write_log(const char *fmt, ...)
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

static void write_socks()
{
    write_debug("socks: %d\n", socks.size());
    for (auto it = socks.begin(); it != socks.end(); it++) {
        write_debug("    %d -> %d\n", it->first, it->second);
    }
}

// hook后替换的函数
static int WINAPI fake_closesocket(SOCKET s)
{
    if (socks.count(s)) {
        write_debug("close socket: %d -> %d\n", s, socks[s]);
        closesocket(socks[s]);
        socks.erase(s);
        write_socks();
    }
    return _closesocket(s);
}

static int WINAPI fake_sendto(SOCKET s, const char *buf, int len, int flags, const sockaddr *to, int tolen)
{
    sockaddr_in *toaddr = (sockaddr_in *)to;

    if (fake_addr_pool.has_origin_addr(toaddr)) {
        SockAddrIn origin_to = SockAddrIn::from_sockaddr(to);
        SockAddrIn new_to = fake_addr_pool.get_origin_addr(origin_to);
        new_to.set_port_raw(origin_to.get_port());
        int result = _sendto(s, buf, len, flags, new_to.get_sockaddr(), new_to.get_len()); // 替换v4地址到v6后发送
        write_debug("fake-ip  sendto: %s:%d -> %s:%d, result: %d\n", inet_ntoa(toaddr->sin_addr), ntohs(toaddr->sin_port), new_to.get_ip().c_str(), new_to.get_port(), result);
        return result;
    }

    if (toaddr->sin_addr.S_un.S_addr != INADDR_BROADCAST) {
        return _sendto(s, buf, len, flags, to, tolen); // 非广播直接原样发送
    }

    int result = -1;

    SockAddrIn target_addr;
    if (!SockAddrIn::from_addr(address, &target_addr)) {
        write_log("parse address failed\n");
        return true;
    }
    target_addr.set_port_raw(toaddr->sin_port);

    if (!socks.count(s)) {
        // 获取原sockaddr
        sockaddr_in addr_self;
        addr_self.sin_family = AF_INET;
        int namelen = sizeof(sockaddr_in);
        getsockname(s, (sockaddr *)&addr_self, &namelen); // 获取原sockaddr
        if (addr_self.sin_port == 0) {
            // 如果没有端口号，先原样发送，这样系统才会分配一个端口号
            result = _sendto(s, buf, len, flags, to, tolen);
            getsockname(s, (sockaddr *)&addr_self, &namelen); // 重新获取
        }

        SockAddrIn local_addr;
        local_addr = target_addr;
        local_addr.set_ip_any();
        local_addr.set_port_raw(addr_self.sin_port);

        SOCKET sock = socket(local_addr.get_family(), SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET) {
            write_log("create socket failed\n");
            return true;
        }

        unsigned long on = 1;
        ioctlsocket(sock, FIONBIO, &on); // 设置非阻塞
        BOOL opt = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口
        
        bind(sock, local_addr.get_sockaddr(), local_addr.get_len()); // 绑定地址端口

        socks[s] = sock;
        write_socks();

        write_debug("replace origin_sock: %s:%d -> %s:%d, %d -> %d\n", inet_ntoa(addr_self.sin_addr), ntohs(addr_self.sin_port), local_addr.get_ip().c_str(), local_addr.get_port(), s, sock);
    }

    SOCKET new_sock = socks[s];
    result = _sendto(new_sock, buf, len, flags, target_addr.get_sockaddr(), target_addr.get_len());

    SockAddrIn local_addr = SockAddrIn::from_socket(new_sock);
    write_debug("redirect sendto: %s:%d -> %s:%d, through: %s:%d, result: %d\n", inet_ntoa(toaddr->sin_addr), ntohs(toaddr->sin_port), target_addr.get_ip().c_str(), target_addr.get_port(), local_addr.get_ip().c_str(), local_addr.get_port(), result);
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
        SockAddrIn addr = SockAddrIn::from_socket(s);
        if (addr.get_family() == AF_INET && addr.addr.addr_v4.sin_addr.S_un.S_addr == INADDR_ANY) {
            if (SockAddrIn::from_addr(address).get_family() == AF_INET6) {
                SockAddrIn local_addr;
                local_addr.addr.addr_v6.sin6_family = AF_INET6;
                local_addr.set_ip_any();
                local_addr.set_port_raw(addr.addr.addr_v4.sin_port);
                SOCKET sock = socket(AF_INET6, SOCK_DGRAM, 0);
                if (sock == INVALID_SOCKET) {
                    write_log("create socket failed\n");
                    return -1;
                }
                BOOL opt = TRUE;
                setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(BOOL)); // 重用地址端口
                bind(sock, local_addr.get_sockaddr(), local_addr.get_len()); // 绑定地址端口
                socks[s] = sock;
                write_socks();
                write_debug("replace origin_sock: %s:%d -> %s:%d, %d -> %d\n", inet_ntoa(addr.addr.addr_v4.sin_addr), ntohs(addr.addr.addr_v4.sin_port), local_addr.get_ip().c_str(), local_addr.get_port(), s, sock);
                return fake_select(n, rd, wr, ex, timeout);
            }
        }
    }
    return _select(n, rd, wr, ex, timeout);
}

static int WINAPI fake_recvfrom(SOCKET s, char *buf, int len, int flags, sockaddr *from, int *fromlen)
{
    SockAddrIn self_addr = SockAddrIn::from_socket(s);
    if (socks.count(s)) {
        SockAddrIn new_from;
        int result = _recvfrom(socks[s], buf, len, flags, new_from.get_sockaddr(), fromlen);
        SockAddrIn local_addr = SockAddrIn::from_socket(socks[s]);
        if (result > 0) write_debug("redirect recvfrom: %s:%d -> %s:%d, %d -> %d, result: %d\n", self_addr.get_ip().c_str(), self_addr.get_port(), local_addr.get_ip().c_str(), local_addr.get_port(), s, socks[s], result);
        if (new_from.get_family() == AF_INET) { // 如果是v4地址，直接返回
            memcpy(from, &new_from.addr.addr_v4, sizeof(sockaddr_in));
        } else if (new_from.get_family() == AF_INET6) { // 如果是v6地址，替换成v4的fake-ip地址
            SockAddrIn fake_from = fake_addr_pool.get_fake_addr(new_from);
            memcpy(from, &fake_from.addr.addr_v4, sizeof(sockaddr_in));
            *fromlen = sizeof(sockaddr_in);
        }
        return result;
    }
    int result = _recvfrom(s, buf, len, flags, from, fromlen);
    if (result > 0) write_debug("original recvfrom: %s:%d, %d, result: %d\n", inet_ntoa(((sockaddr_in *)from)->sin_addr), ntohs(((sockaddr_in *)from)->sin_port), s, result);
    return result;
}

void hook_closesocket()
{
    if (!closesocket_hook) {
        closesocket_hook = new InlineHook(GetModuleHandleA("ws2_32.dll"), "closesocket", (void *)fake_closesocket, CLOSESOCKET_ENTRY_LEN);
        _closesocket = (closesocket_func)closesocket_hook->get_old_entry();
        closesocket_hook->hook();
    }
}

void unhook_closesocket()
{
    if (closesocket_hook) {
        closesocket_hook->unhook();
        delete closesocket_hook;
        closesocket_hook = NULL;
    }
}

void hook_sendto()
{
    if (!sendto_hook) {
        sendto_hook = new InlineHook(GetModuleHandleA("ws2_32.dll"), "sendto", (void *)fake_sendto, SENDTO_ENTRY_LEN);
        _sendto = (sendto_func)sendto_hook->get_old_entry();
        sendto_hook->hook();
    }
}

void unhook_sendto()
{
    if (sendto_hook) {
        sendto_hook->unhook();
        delete sendto_hook;
        sendto_hook = NULL;
    }
}

void hook_select()
{
    if (!select_hook) {
        select_hook = new InlineHook(GetModuleHandleA("ws2_32.dll"), "select", (void *)fake_select, SELECT_ENTRY_LEN);
        _select = (select_func)select_hook->get_old_entry();
        select_hook->hook();
    }
}

void unhook_select()
{
    if (select_hook) {
        select_hook->unhook();
        delete select_hook;
        select_hook = NULL;
    }
}

void hook_recvfrom()
{
    if (!recvfrom_hook) {
        recvfrom_hook = new InlineHook(GetModuleHandleA("ws2_32.dll"), "recvfrom", (void *)fake_recvfrom, RECVFROM_ENTRY_LEN);
        _recvfrom = (recvfrom_func)recvfrom_hook->get_old_entry();
        recvfrom_hook->hook();
    }
}

void unhook_recvfrom()
{
    if (recvfrom_hook) {
        recvfrom_hook->unhook();
        delete recvfrom_hook;
        recvfrom_hook = NULL;
    }
}

void hook()
{
    read_config();
    hook_sendto();
    hook_select();
    hook_recvfrom();
    hook_closesocket();
}

void unhook()
{
    unhook_sendto();
    unhook_select();
    unhook_recvfrom();
    unhook_closesocket();
    for (auto it = socks.begin(); it != socks.end(); it++) {
        closesocket(it->second);
    }
}
