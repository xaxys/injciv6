package utils

/*
#cgo CFLAGS: -I ${SRCDIR}/../../inc -I ${SRCDIR}/../../src
#cgo CPPFLAGS: -I ${SRCDIR}/../../inc -I ${SRCDIR}/../../src
#cgo CXXFLAGS: -I ${SRCDIR}/../../inc -I ${SRCDIR}/../../src
#cgo LDFLAGS: -static
#include <stdio.h>
#include "inject_util.h"
#include "inject.cpp"
#include "inject_util.cpp"

bool is_null(HINSTANCE handle) {
	return handle == NULL;
}
*/
import "C"

import (
	"fmt"
	"os"
	"path/filepath"
	"syscall"
	"unsafe"
)

type Civ6Status int

const (
	Civ6StatusNotFound    Civ6Status = -1
	Civ6StatusUnknown     Civ6Status = 0
	Civ6StatusRunningDX11 Civ6Status = 1
	Civ6StatusRunningDX12 Civ6Status = 2
)

func IsCiv6Running() Civ6Status {
	dx11 := C.get_civ6_dx11_proc()
	if dx11 != 0 {
		return Civ6StatusRunningDX11
	}

	dx12 := C.get_civ6_dx12_proc()
	if dx12 != 0 {
		return Civ6StatusRunningDX12
	}

	return Civ6StatusNotFound
}

func IsCiv6Injected() bool {
	dllCstr := C.CString("hookdll64.dll")
	defer C.free(unsafe.Pointer(dllCstr))
	pid := C.get_civ6_proc()
	handle := C.find_module_handle_from_pid(pid, dllCstr)
	if !C.is_null(handle) {
		return true
	}
	return false
}

func GetCiv6Path() (string, error) {
	buf := make([]uint16, 4096) // LPWSTR
	success := C.get_civ6_path((*C.wchar_t)(unsafe.Pointer(&buf[0])), C.ulong(len(buf)))
	str := syscall.UTF16ToString(buf)
	if !success {
		return "", fmt.Errorf(str)
	}
	return str, nil
}

func GetCiv6Dir() (string, error) {
	path, err := GetCiv6Path()
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
	path := filepath.Join(GetCurrentDir(), "injciv6.exe")
	_, err := os.Stat(path)
	if err != nil {
		return "", false
	}
	return path, true
}

func GetInjectRemoverPath() (string, bool) {
	path := filepath.Join(GetCurrentDir(), "civ6remove.exe")
	_, err := os.Stat(path)
	if err != nil {
		return "", false
	}
	return path, true
}

func WriteConfig(address string) error {
	dir, err := GetCiv6Dir()
	if err != nil {
		return err
	}
	path := filepath.Join(dir, "injciv6-config.txt")

	// Modify or create the file
	if err := os.WriteFile(path, []byte(address), 0644); err != nil {
		return err
	}

	return nil
}

func ReadConfig() (string, error) {
	dir, err := GetCiv6Dir()
	if err != nil {
		return "", err
	}
	path := filepath.Join(dir, "injciv6-config.txt")

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
