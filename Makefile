CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror
CROSS ?= m68k-elf-
CROSS_CFLAGS = -mcpu=54455 -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie -fomit-frame-pointer -Wall -Wextra -Werror

.PHONY: test cross-check
out:
	mkdir -p out
out/test_sophie: src/sophie.c src/sophie.h tests/test_sophie.c | out
	$(CC) $(CFLAGS) -Isrc src/sophie.c tests/test_sophie.c -o $@
test: out/test_sophie
	./out/test_sophie
cross-check: | out
	mkdir -p out/cross
	$(CROSS)as -mcpu=54455 -I src -o out/cross/glue.o src/glue.s
	$(CROSS)gcc $(CROSS_CFLAGS) -Isrc -c src/digitakt.c -o out/cross/digitakt.o
	$(CROSS)gcc $(CROSS_CFLAGS) -Isrc -c src/sophie.c -o out/cross/sophie.o
