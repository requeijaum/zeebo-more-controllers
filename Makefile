# ZeeHID Probe: host build of the portable core and its component tests.
#   make test            run CT-001..CT-010 under ASan and UBSan (SAN=0 to disable)
#   make check-portable  the core, fs and hidport sources use no allocation, stdio or BREW header
CC ?= cc
SAN ?= 1
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g
ifeq ($(SAN),1)
CFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all
endif
SRC := src/core/zhp_core.c src/fs/zhp_fs.c src/hidport/zhp_hid.c
BUILD := build

.PHONY: test check-portable clean all
all: test check-portable

$(BUILD)/ct: tests/test_main.c tests/ztest.h $(SRC) $(wildcard src/*/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_main.c $(SRC)

test: $(BUILD)/ct
	$(BUILD)/ct

check-portable:
	@! grep -nE 'malloc|calloc|realloc|free\(|stdio|printf|fopen|#include *"?AEE|#include *<AEE' $(SRC) $(wildcard src/*/*.h) \
	  && echo "check-portable: ok"

clean:
	rm -rf $(BUILD)
