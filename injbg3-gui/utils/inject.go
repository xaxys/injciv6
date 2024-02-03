package utils

/*
#cgo CFLAGS: -I ${SRCDIR}/../../inc -I ${SRCDIR}/../../src
#cgo CPPFLAGS: -I ${SRCDIR}/../../inc -I ${SRCDIR}/../../src
#cgo CXXFLAGS: -I ${SRCDIR}/../../inc -I ${SRCDIR}/../../src
#cgo LDFLAGS: -static -l ws2_32 -l iphlpapi
#include <stdio.h>
#include "inject_util.cpp"
#include "inject.cpp"

bool is_null(HINSTANCE handle) {
	return handle == NULL;
}
*/
import "C"

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"unsafe"
)

type BG3Status int

const (
	BG3StatusNotFound    BG3Status = -1
	BG3StatusUnknown     BG3Status = 0
	BG3StatusRunningDX11 BG3Status = 1
	BG3StatusRunningDX12 BG3Status = 2
)

func IsBG3Running() BG3Status {
	dx11 := C.get_bg3_dx11_proc()
	if dx11 != 0 {
		return BG3StatusRunningDX11
	}

	dx12 := C.get_bg3_dx12_proc()
	if dx12 != 0 {
		return BG3StatusRunningDX12
	}

	return BG3StatusNotFound
}

type InjectStatus int

const (
	InjectStatusUnknown     InjectStatus = 0
	InjectStatusNotInjected InjectStatus = 1
	InjectStatusInjected    InjectStatus = 2
	InjectStatusRunningIPv6 InjectStatus = 3
)

func GrantSeDebugPrivilege() bool {
	success := bool(C.grant_se_debug_privilege())
	return success
}

func RunAsAdmin(exepath string) (bool, error) {
	pExePath, err := syscall.UTF16PtrFromString(exepath)
	if err != nil {
		return false, err
	}
	ret := bool(C.runas_admin((*C.wchar_t)(unsafe.Pointer(pExePath))))
	return ret, nil
}

func IsAdmin() bool {
	isAdmin := C.IsUserAnAdmin() != C.int(0)
	return isAdmin
}

func Isbg3Injected() InjectStatus {
	dllCstr := C.CString("hookdll64.dll")
	defer C.free(unsafe.Pointer(dllCstr))
	pid := C.get_bg3_proc()
	handle := C.find_module_handle_from_pid(pid, dllCstr)
	if C.is_null(handle) {
		return InjectStatusNotInjected
	}
	if C.is_injbg3_running(pid) {
		return InjectStatusRunningIPv6
	}
	return InjectStatusInjected
}

func GetBG3Path() (string, error) {
	buf := make([]uint16, 4096) // LPWSTR
	success := C.get_bg3_path((*C.wchar_t)(unsafe.Pointer(&buf[0])), C.ulong(len(buf)))
	str := syscall.UTF16ToString(buf)
	if !success {
		str = strings.TrimSpace(str)
		str = strings.TrimRight(str, "。")
		str = strings.TrimRight(str, ".")
		return "", fmt.Errorf(str)
	}
	return str, nil
}

func Getbg3Dir() (string, error) {
	path, err := GetBG3Path()
	if err != nil {
		return "", err
	}
	return filepath.Dir(path), nil
}

func GetCurrentDir() string {
	exePath, err := os.Executable()
	if err != nil {
		panic(err)
	}
	return filepath.Dir(exePath)
}

func GetInjectorPath() (string, bool) {
	path := filepath.Join(GetCurrentDir(), "injbg3.exe")
	_, err := os.Stat(path)
	if err != nil {
		return "", false
	}
	return path, true
}

func GetInjectRemoverPath() (string, bool) {
	path := filepath.Join(GetCurrentDir(), "bg3remove.exe")
	_, err := os.Stat(path)
	if err != nil {
		return "", false
	}
	return path, true
}

func WriteConfig(address string) error {
	dir, err := Getbg3Dir()
	if err != nil {
		return err
	}
	path := filepath.Join(dir, "injbg3-config.txt")

	// Modify or create the file
	if err := os.WriteFile(path, []byte(address), 0644); err != nil {
		return err
	}

	return nil
}

func ReadConfig() (string, error) {
	dir, err := Getbg3Dir()
	if err != nil {
		return "", err
	}
	path := filepath.Join(dir, "injbg3-config.txt")

	if _, err := os.Stat(path); os.IsNotExist(err) {
		return "", nil // File not exist
	}

	// Read the file
	data, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}

	return string(data), nil
}
