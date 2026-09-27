REACTOS ?= ../reactos
BUILD_DIR ?= $(CURDIR)/build
REPRO_DIR ?= $(CURDIR)/build-repro
IMAGE ?= $(CURDIR)/fixtures/hi.exe
PYTHON ?= python3

.PHONY: all build verify repro host-oracle fixture-oracles check-image inspect-image clean help

all: build

build:
	BUILD_DIR="$(BUILD_DIR)" bash ./build-le4c-canonical.sh "$(REACTOS)"

repro:
	rm -rf "$(REPRO_DIR)"
	BUILD_DIR="$(REPRO_DIR)" bash ./build-le4c-canonical.sh "$(REACTOS)"

host-oracle:
	mkdir -p evidence
	rm -rf evidence/host-oracle-make
	IMAGE="$(IMAGE)" bash ./build-host-le4x.sh evidence/host-oracle-make | tee evidence/LE4X-HOST-ORACLE-GCC.txt


fixture-oracles:
	mkdir -p evidence/oracle-tool
	$(CC) -std=c99 -O2 -Wall -Wextra -Werror \
		tools/le4x_oracle.c common/os2loader.c common/os2image.c \
		common/os2veneer.c common/os2startup.c common/os2sha256.c \
		-o evidence/oracle-tool/le4io_oracle
	@for f in hi.exe hi2.exe phoon.exe infocom.exe; do \
		rm -rf "evidence/oracle-$$f"; mkdir -p "evidence/oracle-$$f"; \
		evidence/oracle-tool/le4io_oracle "$(CURDIR)/fixtures/$$f" "evidence/oracle-$$f" > "evidence/oracle-$$f.txt"; \
		grep -q "LE4IO host construction oracle PASS" "evidence/oracle-$$f.txt" || exit 1; \
	done

check-image:
	@echo "Checking image: $(IMAGE)"
	rm -rf evidence/check-image
	IMAGE="$(IMAGE)" bash ./build-host-le4x.sh evidence/check-image
	@echo "PASS: image satisfies the currently supported LE4C loader/oracle shape"


inspect-image:
	@echo "Inspecting image: $(IMAGE)"
	mkdir -p evidence/inspect-image
	$(CC) -std=c99 -Wall -Wextra -Werror -Icommon tools/leplan.c common/os2loader.c -o evidence/inspect-image/leplan
	evidence/inspect-image/leplan --summary "$(IMAGE)" | tee evidence/inspect-image/summary.txt

verify: build repro fixture-oracles
	$(PYTHON) tools/static_verify_le4io.py "$(REACTOS)" "$(REPRO_DIR)"

clean:
	rm -rf build build-repro evidence/host-oracle-make evidence/check-image evidence/inspect-image evidence/oracle-* evidence/oracle-tool

help:
	@echo 'make REACTOS=/path/to/reactos'
	@echo 'make verify REACTOS=/path/to/reactos'
	@echo 'make check-image IMAGE=/path/to/supported-le.exe'
	@echo 'make inspect-image IMAGE=/path/to/any-le-lx.exe'
	@echo 'make clean'
