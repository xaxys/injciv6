package utils

import (
	"fmt"
	"io"
	"net"
	"net/http"
	"regexp"
)

func IsValidIP(ip string) bool {
	addr := net.ParseIP(ip)
	if addr == nil {
		return false
	}
	return true
}

func IsValidIPv4(ip string) bool {
	ipv4 := net.ParseIP(ip)
	if ipv4 == nil {
		return false
	}
	if ipv4.To4() == nil {
		return false
	}
	return true
}

func IsValidIPv6(ip string) bool {
	ipv6 := net.ParseIP(ip)
	if ipv6 == nil {
		return false
	}
	if ipv6.To4() != nil {
		return false
	}
	return true
}

func IsValidDomainRegex(domain string) bool {
	regex := `^(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$`
	match, _ := regexp.MatchString(regex, domain)
	return match
}

func IsValidDomain(domain string) bool {
	if _, err := net.LookupHost(domain); err == nil {
		return true // Valid domain name
	}
	return false
}

type IPv6Status int

const (
	IPv6StatusUnknown IPv6Status = iota
	IPv6StatusNotSupported
	IPv6StatusOnlineFail
	IPv6StatusOnlineFailMultiple
	IPv6StatusDNSFail
	IPv6StatusDNSFailMultiple
	IPv6StatusDNSNotPreferred
	IPv6StatusDNSNotPreferredMultiple
	IPv6StatusSupported
	IPv6StatusSupportedMultiple
)

func GetMyIPv6() (string, IPv6Status, error) {
	ips, localErr := GetMyIPv6Local() // 本地获取IPv6地址
	if localErr != nil {
		if _, ok := localErr.(*net.OpError); !ok {
			return "", IPv6StatusNotSupported, localErr // 本地非因API调用失败导致获取不到IPv6地址，说明就是没有IPv6地址，返回不支持IPv6
		}
		// 本地因API调用失败导致获取不到IPv6地址，说明可能是因为没有IPv6地址，也可能是因为API调用失败，需要再次尝试在线获取IPv6地址
	}

	ip, onlineErr := GetMyIPv6Online() // 在线获取IPv6地址，如果成功，说明支持IPv6，并且DNS也正常
	if onlineErr == nil {
		status := IPv6StatusSupported
		if len(ips) > 1 {
			status = IPv6StatusSupportedMultiple
		}
		return ip, status, nil // 在线获取到IPv6地址，说明支持IPv6，并且DNS也正常，返回支持IPv6和IPv6上网主地址
	}

	ip, onlineErr = GetMyIPv6OnlineDNSv6() // 在线获取IPv6地址，使用仅AAAA记录的DNS，如果成功，说明支持IPv6，但IPv6地址不是首选地址
	if onlineErr == nil {
		status := IPv6StatusDNSNotPreferred
		if len(ips) > 1 {
			status = IPv6StatusDNSNotPreferredMultiple
		}
		return ip, status, nil // 在线获取到IPv6地址，说明支持IPv6，但IPv6地址不是首选地址，返回支持IPv6和IPv6上网主地址
	}

	ip, onlineErr = GetMyIPv6OnlineNoDNS() // 在线获取IPv6地址，不使用DNS，如果成功，说明支持IPv6，但DNS不正常
	if onlineErr == nil {
		status := IPv6StatusDNSFail
		if len(ips) > 1 {
			status = IPv6StatusDNSFailMultiple
		}
		return ip, status, nil // 在线获取到IPv6地址，说明支持IPv6，但DNS不正常，返回支持IPv6和IPv6上网主地址
	}

	if localErr != nil {
		return "", IPv6StatusNotSupported, localErr // 在线获取IPv6地址失败，本地获取IPv6地址也失败，说明大概率没有IPv6地址，返回不支持IPv6
	}
	status := IPv6StatusOnlineFail
	if len(ips) > 1 {
		status = IPv6StatusOnlineFailMultiple
	}
	return ips[0], status, onlineErr // 在线获取IPv6地址失败，本地获取IPv6地址成功，说明不能上网，返回获取到的本地IPv6地址
}

func GetMyIPv6Local() ([]string, error) {
	ifaces, err := net.Interfaces()
	if err != nil {
		return nil, err
	}
	ips := []string{}
	for _, iface := range ifaces {
		if iface.Flags&net.FlagUp == 0 {
			continue // interface down
		}
		if iface.Flags&net.FlagLoopback != 0 {
			continue // loopback interface
		}
		if iface.Flags&net.FlagPointToPoint != 0 {
			continue // p2p interface
		}
		addrs, err := iface.Addrs()
		if err != nil {
			// ignore error
			continue
		}
		for _, addr := range addrs {
			ip, ok := addr.(*net.IPNet)
			if !ok {
				continue // not an IPNet
			}
			if ip.IP.To4() != nil {
				continue // not an IPv6 address
			}
			if !ip.IP.IsGlobalUnicast() {
				continue // not a global unicast address
			}
			ips = append(ips, ip.IP.String())
		}
	}
	if len(ips) == 0 {
		return nil, fmt.Errorf("No IPv6 address found")
	}
	return ips, nil
}

func GetMyIPv6Online() (string, error) {
	host := "test.ipw.cn"
	ips, err := net.LookupHost(host)
	if err != nil {
		return "", err
	}
	dnsOK := false
	for _, ip := range ips {
		ip := net.ParseIP(ip)
		if ip.To4() != nil {
			continue // not an IPv6 address
		}
		if !ip.IsGlobalUnicast() {
			continue // not a global unicast address
		}
		dnsOK = true
	}
	if !dnsOK {
		return "", fmt.Errorf("No IPv6 DNS record found")
	}

	url := "http://test.ipw.cn/"
	resp, err := http.Get(url)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	ip := net.ParseIP(string(body))
	if ip != nil && ip.To4() == nil && ip.IsGlobalUnicast() {
		return ip.String(), nil
	}
	return "", fmt.Errorf("Not an IPv6 address")
}

func GetMyIPv6OnlineDNSv6() (string, error) {
	url := "http://6.ipw.cn/"
	resp, err := http.Get(url)
	if err != nil {
		return "", err
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	ip := net.ParseIP(string(body))
	if ip != nil && ip.To4() == nil && ip.IsGlobalUnicast() {
		return ip.String(), nil
	}
	return "", fmt.Errorf("Not an IPv6 address")
}

func GetMyIPv6OnlineNoDNS() (string, error) {
	url := "https://[2606:4700:4700::1111]/cdn-cgi/trace"
	resp, err := http.Get(url)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	ipRegex := regexp.MustCompile(`ip=(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}|[0-9a-fA-F:]+)`)
	body = ipRegex.FindSubmatch(body)[1]
	ip := net.ParseIP(string(body))
	if ip.To4() != nil {
		return "", fmt.Errorf("Not an IPv6 address")
	}
	return ip.String(), nil
}
