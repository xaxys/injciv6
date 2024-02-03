# Copyright (C) 2023 xaxys. All rights reserved.
PACKAGE_NAME := injbg3-gui

GO         ?= go.exe
PWD        := ${CURDIR}
TARGET     := $(PACKAGE_NAME).exe
BUILD_TAGS := $(shell git describe --tags --always --dirty="-dev")
BUILD_TIME := $(shell echo %date% %time%)
GIT_COMMIT := $(shell git rev-parse --short HEAD)
GO_VERSION := $(subst go version ,,$(shell go version))
BIN_DIR	   := $(PWD)/../bin

CXX64PREFIX    := x86_64-w64-mingw32-
CC             := $(CXX64PREFIX)gcc
CXX            := $(CXX64PREFIX)g++

ICONS := $(PWD)/assets/inject.ico,$(PWD)/assets/host.ico,$(PWD)/assets/remote.ico,$(PWD)/assets/tool.ico,$(PWD)/assets/about.ico

all: gen build

setup:
	@$(GO) install github.com/tc-hib/go-winres@latest

gen: setup
	@echo Generating resources...
	@go-winres make

build:
	@echo Building $(PACKAGE_NAME) ...
	@$(GO) env -w CGO_ENABLED=1 CC="$(CC)" CXX="$(CXX)"
	@$(GO) build \
		-tags walk_use_cgo \
		-ldflags="-H windowsgui -X 'injbg3-gui/components.BuildTags=$(BUILD_TAGS)' -X 'injbg3-gui/components.BuildTime=$(BUILD_TIME)' -X 'injbg3-gui/components.GitCommit=$(GIT_COMMIT)' -X 'injbg3-gui/components.GoVersion=$(GO_VERSION)'" \
		-o $(BIN_DIR)/$(TARGET)

run: build
	@echo Running $(PACKAGE_NAME) ...
	@$(BIN_DIR)/$(TARGET)

clean:
	@$(GO) clean -cache
	@del *.syso

.PHONY: all gen setup build run cleans
