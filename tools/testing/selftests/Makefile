# SPDX-License-Identifier: GPL-2.0
# Top-level kselftest Makefile (mini-os) — mirrors Linux tools/testing/selftests.

top_srcdir := $(abspath ../../..)

# Override from the root Makefile when embedding into initramfs.
USER_CC     ?= aarch64-unknown-linux-musl-gcc
USER_CFLAGS ?= -Wall -Wextra -O0 -g -static -fno-pie -no-pie
USER_PATH   ?= $(HOME)/toolchains/aarch64-unknown-linux-musl/bin:$(PATH)
OUTPUT      ?= $(CURDIR)/build

# Subdirectories with tests (add new suites here, Linux-style).
TARGETS := yield

export top_srcdir USER_CC USER_CFLAGS USER_PATH OUTPUT

.PHONY: all clean install $(TARGETS)

all: $(TARGETS)

$(TARGETS):
	mkdir -p $(OUTPUT)/$@
	$(MAKE) -C $@ OUTPUT=$(OUTPUT)/$@

# Install built binaries into the initramfs kselftests tree.
INSTALL_PATH ?= $(top_srcdir)/initramfs/root/kselftests

install: all
	mkdir -p $(INSTALL_PATH)
	@for t in $(TARGETS); do \
		mkdir -p $(INSTALL_PATH)/$$t; \
		cp -f $(OUTPUT)/$$t/* $(INSTALL_PATH)/$$t/ 2>/dev/null || true; \
	done
	cp -f run_kselftest.sh $(INSTALL_PATH)/run_kselftest.sh
	chmod +x $(INSTALL_PATH)/run_kselftest.sh
	@# Emit a simple runlist (one binary per line, relative to INSTALL_PATH).
	@{ \
		for t in $(TARGETS); do \
			for f in $(OUTPUT)/$$t/*; do \
				[ -f "$$f" ] && [ -x "$$f" ] || continue; \
				echo "$$t/$$(basename $$f)"; \
			done; \
		done; \
	} > $(INSTALL_PATH)/kselftest-list.txt

clean:
	@for t in $(TARGETS); do \
		$(MAKE) -C $$t OUTPUT=$(OUTPUT)/$$t clean; \
	done
	rm -rf $(OUTPUT)
