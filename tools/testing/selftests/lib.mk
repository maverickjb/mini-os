# Shared kselftest build rules for mini-os (shaped like Linux lib.mk).
#
# Consumed by per-directory Makefiles. Expects from the parent:
#   USER_CC, USER_CFLAGS, USER_PATH, OUTPUT, top_srcdir

SELFTEST_HDR := $(top_srcdir)/tools/testing/selftests

CFLAGS  += -I$(SELFTEST_HDR) $(USER_CFLAGS)
CC      := $(USER_CC)

# Default: build TEST_GEN_PROGS into OUTPUT/
all: $(addprefix $(OUTPUT)/,$(TEST_GEN_PROGS))

$(OUTPUT)/%: %.c $(SELFTEST_HDR)/kselftest.h
	mkdir -p $(OUTPUT)
	PATH="$(USER_PATH)" $(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(addprefix $(OUTPUT)/,$(TEST_GEN_PROGS))

.PHONY: all clean
