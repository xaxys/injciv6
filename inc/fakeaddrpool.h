#pragma once

#include <sockaddrin.h>
#include <map>
#include <variant>

struct FakeAddrPool {

    struct IPv4Addr {
        in_addr addr;

        IPv4Addr() { memset(&this->addr, 0, sizeof(in_addr)); }

        IPv4Addr(const in_addr &addr) : addr(addr) {}

        IPv4Addr(ULONG addr) { this->addr.s_addr = addr; }

        bool operator<(const IPv4Addr &other) const {
            return memcmp(&addr, &other.addr, sizeof(in_addr)) < 0;
        }

        bool operator==(const IPv4Addr &other) const {
            return memcmp(&addr, &other.addr, sizeof(in_addr)) == 0;
        }
    };

    struct IPv6Addr {
        in6_addr addr;

        IPv6Addr() { memset(&addr, 0, sizeof(in6_addr)); }

        IPv6Addr(const in6_addr &addr) : addr(addr) {}

        bool operator<(const IPv6Addr &other) const {
            return memcmp(&addr, &other.addr, sizeof(in6_addr)) < 0;
        }

        bool operator==(const IPv6Addr &other) const {
            return memcmp(&addr, &other.addr, sizeof(in6_addr)) == 0;
        }
    };

    struct IPAddr {
        short family;
        std::variant<IPv4Addr, IPv6Addr> ip;

        IPAddr() : family(AF_INET), ip(IPv4Addr()) {}
        IPAddr(const in_addr &addr) : family(AF_INET), ip(IPv4Addr(addr)) {}
        IPAddr(const IPv4Addr &addr) : family(AF_INET), ip(addr) {}
        IPAddr(const in6_addr &addr) : family(AF_INET6), ip(IPv6Addr(addr)) {}
        IPAddr(const IPv6Addr &addr) : family(AF_INET6), ip(addr) {}

        bool operator<(const IPAddr &other) const {
            if (family != other.family) {
                return family < other.family;
            }
            if (family == AF_INET) {
                return std::get<IPv4Addr>(ip) < std::get<IPv4Addr>(other.ip);
            }
            return std::get<IPv6Addr>(ip) < std::get<IPv6Addr>(other.ip);
        }

        bool operator==(const IPAddr &other) const {
            if (family != other.family) {
                return false;
            }
            if (family == AF_INET) {
                return std::get<IPv4Addr>(ip) == std::get<IPv4Addr>(other.ip);
            }
            return std::get<IPv6Addr>(ip) == std::get<IPv6Addr>(other.ip);
        }
    };

    std::map<IPv4Addr, IPAddr> fake_to_real;
    std::map<IPAddr, IPv4Addr> real_to_fake;
    in_addr start;
    in_addr end;
    in_addr current;

    FakeAddrPool(in_addr start, u_long range) {
        this->start = start;
        this->end = next(start, range);
        this->current = start;
    }

    sockaddr_in get_fake_addr(const SockAddrIn &addr) {
        switch (addr.get_family()) {
        case AF_INET:
            return get_fake_addr(IPv4Addr(addr.addr.addr_v4.sin_addr));
        case AF_INET6:
            return get_fake_addr(IPv6Addr(addr.addr.addr_v6.sin6_addr));
        default:
            sockaddr_in fake_addr;
            memset(&fake_addr, 0, sizeof(fake_addr));
            return fake_addr;
        }
    }

    sockaddr_in get_fake_addr(const sockaddr_in6 *addr) {
        return get_fake_addr(IPv6Addr(addr->sin6_addr));
    }

    sockaddr_in get_fake_addr(const sockaddr_in *addr) {
        return get_fake_addr(IPv4Addr(addr->sin_addr));
    }

    sockaddr_in get_fake_addr(const IPAddr &addr) {
        in_addr fake_ip;
        if (real_to_fake.count(addr)) {
            fake_ip = real_to_fake[addr].addr;
        } else {
            fake_ip = search_available_addr(current);
            IPv4Addr addr4(fake_ip);
            real_to_fake[addr] = addr4;
            fake_to_real[addr4] = addr;
        }
        sockaddr_in fake_addr;
        memset(&fake_addr, 0, sizeof(fake_addr));
        fake_addr.sin_family = AF_INET;
        fake_addr.sin_addr = fake_ip;
        return fake_addr;
    }

    SockAddrIn get_origin_addr(const sockaddr_in *addr) {
        SockAddrIn origin_addr;
        IPv4Addr addr4(addr->sin_addr);
        if (fake_to_real.count(addr4)) {
            IPAddr addr = fake_to_real[addr4];
            switch (addr.family) {
            case AF_INET:
                origin_addr.addr.addr_v4.sin_family = AF_INET;
                origin_addr.addr.addr_v4.sin_addr = std::get<IPv4Addr>(addr.ip).addr;
                break;
            case AF_INET6:
                origin_addr.addr.addr_v6.sin6_family = AF_INET6;
                origin_addr.addr.addr_v6.sin6_addr = std::get<IPv6Addr>(addr.ip).addr;
                break;
            }
        }
        return origin_addr;
    }

    bool has_origin_addr(const sockaddr_in *addr) {
        return fake_to_real.count(IPv4Addr(addr->sin_addr));
    }

    in_addr search_available_addr(in_addr search_start) {
        if (!fake_to_real.count(IPv4Addr(search_start))) {
            return search_start;
        }
        in_addr addr = next(search_start);
        while (memcmp(&addr, &search_start, sizeof(in_addr)) != 0) {
            if (!fake_to_real.count(IPv4Addr(addr))) {
                return addr;
            }
            addr = next(addr);
            if (memcmp(&addr, &search_start, sizeof(in_addr)) == 0) {
                addr = start;
            }
        }
        return search_start;
    }

    in_addr next(const in_addr &addr, int step = 1) {
        ULONG hl = (ULONG)addr.S_un.S_un_b.s_b1 << 24 |
                   (ULONG)addr.S_un.S_un_b.s_b2 << 16 |
                   (ULONG)addr.S_un.S_un_b.s_b3 << 8 |
                   (ULONG)addr.S_un.S_un_b.s_b4;
        hl += step;
        in_addr next_addr;
        next_addr.S_un.S_un_b.s_b1 = (unsigned char)(hl >> 24);
        next_addr.S_un.S_un_b.s_b2 = (unsigned char)(hl >> 16);
        next_addr.S_un.S_un_b.s_b3 = (unsigned char)(hl >> 8);
        next_addr.S_un.S_un_b.s_b4 = (unsigned char)hl;
        return next_addr;
    }
};