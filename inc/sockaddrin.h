#pragma once

#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <string>

namespace std {
    static inline std::string to_string(const struct in_addr &addr) {
        char str[INET_ADDRSTRLEN];
        memset(str, 0, sizeof(str));
        inet_ntop(AF_INET, &addr, str, sizeof(str));
        return str;
    }

    static inline std::string to_string(const struct in6_addr &addr) {
        char str[INET6_ADDRSTRLEN];
        memset(str, 0, sizeof(str));
        inet_ntop(AF_INET6, &addr, str, sizeof(str));
        return str;
    }

    static inline std::string to_string(const struct sockaddr_in &addr) {
        return to_string(addr.sin_addr) + ":" +
               to_string(ntohs(addr.sin_port));
    }

    static inline std::string to_string(const struct sockaddr_in6 &addr) {
        return to_string(addr.sin6_addr) + ":" +
               to_string(ntohs(addr.sin6_port));
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
        getsockname(sock, (sockaddr *)&addr_in.addr, &namelen);
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

    u_short get_family() const {
        return get_sockaddr()->sa_family;
    }

    int get_len() const {
        return get_len(get_family());
    }

    static int get_len(u_short family) {
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
        u_short family = get_family();
        switch (family) {
        case AF_INET:
            return std::to_string(addr.addr_v4.sin_addr);
        case AF_INET6:
            return std::to_string(addr.addr_v6.sin6_addr);
        default:
            return "";
        }
    }

    u_short get_port_raw() const {
        u_short family = get_family();
        switch (family) {
        case AF_INET:
            return addr.addr_v4.sin_port;
        case AF_INET6:
            return addr.addr_v6.sin6_port;
        default:
            return 0;
        }
    }

    void set_family(u_short family) {
        get_sockaddr()->sa_family = family;
    }

    void set_ip_any() {
        u_short family = get_family();
        if (family == AF_INET) {
            addr.addr_v4.sin_addr.s_addr = INADDR_ANY;
        } else if (family == AF_INET6) {
            addr.addr_v6.sin6_addr = in6addr_any;
        }
    }

    void set_port_raw(u_short port) {
        u_short family = get_family();
        if (family == AF_INET) {
            addr.addr_v4.sin_port = port;
        } else if (family == AF_INET6) {
            addr.addr_v6.sin6_port = port;
        }
    }
};

namespace std {
    static inline std::string to_string(const SockAddrIn &addr) {
        return addr.get_ip() + ":" + std::to_string(ntohs(addr.get_port_raw()));
    }
}