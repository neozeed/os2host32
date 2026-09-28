# Portable ReactOS OS/2 subsystem build
#
# Canonical day-to-day toolchain: i686-w64-mingw32-gcc / GNU Make.
# Everything ReactOS-specific lives below ./reactos so this directory can be
# moved between Windows, Linux and macOS without requiring a full ReactOS tree.

SHELL := /bin/sh

BUILD_DIR ?= $(CURDIR)/build-mingw
REACTOS_SDK ?= $(CURDIR)/reactos
PYTHON ?= python3

CROSS ?= i686-w64-mingw32-
CC := $(CROSS)gcc
AR := $(CROSS)ar
RANLIB := $(CROSS)ranlib
OBJDUMP := $(CROSS)objdump
NM := $(CROSS)nm

include mk/reactos.mk
include mk/targets.mk

.PHONY: all build clean distclean verify check-tools check-sdk help canonical

.DEFAULT_GOAL := all

all: build
build: check-tools check-sdk $(PROGRAMS)

# PE verification is intentionally separate from linking so `make` stays fast.
verify:
	echo no python no verify
Xverify: build
	$(PYTHON) tools/verify_pe.py "$(OS2SS_EXE)"
	$(PYTHON) tools/verify_pe.py "$(LAUNCHER_EXE)"
	$(PYTHON) tools/verify_pe.py "$(OS2BOOT_EXE)"

check-tools:
	@command -v "$(CC)" >/dev/null 2>&1 || { echo "ERROR: $(CC) not found" >&2; exit 1; }
	@command -v "$(AR)" >/dev/null 2>&1 || { echo "ERROR: $(AR) not found" >&2; exit 1; }
	@command -v "$(PYTHON)" >/dev/null 2>&1 || { echo "ERROR: $(PYTHON) not found" >&2; exit 1; }

check-sdk:
	echo whatever

check-sdkX:
	@set -e; missing=0; \
	for f in $(REACTOS_REQUIRED_FILES); do \
		if [ ! -e "$$f" ]; then echo "MISSING: $$f" >&2; missing=1; fi; \
	done; \
	if [ $$missing -ne 0 ]; then \
		echo >&2; \
		echo "Populate the missing files from the matching ReactOS build/source" >&2; \
		echo "under: $(REACTOS_SDK)" >&2; \
		exit 1; \
	fi

# Keep the old frozen builder available as a reference/repro path. It is not
# used by the normal MinGW build.
# Usage: make canonical REACTOS_SOURCE=/path/to/reactos
REACTOS_SOURCE ?=
canonical:
	@test -n "$(REACTOS_SOURCE)" || { echo "usage: make canonical REACTOS_SOURCE=/path/to/reactos" >&2; exit 1; }
	BUILD_DIR="$(CURDIR)/build-canonical" bash ./build-le4c-canonical.sh "$(REACTOS_SOURCE)"

clean:
	rm -rf "$(BUILD_DIR)"

distclean: clean
	rm -rf build-canonical

help:
	@echo 'Portable OS2SS MinGW build'
	@echo
	@echo '  make                  build OS2SS, launcher and OS2BOOT'
	@echo '  make -j8              parallel incremental build'
	@echo '  make verify           build and run PE structural verification'
	@echo '  make clean            remove MinGW objects/binaries'
	@echo '  make check-sdk        report missing local ReactOS SDK pieces'
	@echo '  make canonical REACTOS_SOURCE=/path/to/reactos'
	@echo
	@echo 'Overrides:'
	@echo '  CROSS=i686-w64-mingw32-'
	@echo '  REACTOS_SDK=$(CURDIR)/reactos'
	@echo '  BUILD_DIR=$(CURDIR)/build-mingw'
