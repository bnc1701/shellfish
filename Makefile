cc = gcc
cflags = -Wall -Wextra -std=gnu11 -O2 -Iinclude
asflags = -c
src = src/main.c src/shell.c src/parser.c src/exec.c src/builtins.c src/history.c
asm = src/token_scan.S
obj = src/token_scan.o
bin = shellfish

.PHONY: all clean install run debug

all: $(bin)

$(bin): $(src) $(obj)
	$(cc) $(cflags) -o $(bin) $(src) $(obj)

$(obj): $(asm)
	$(cc) $(asflags) -o $(obj) $(asm)

run: $(bin)
	./$(bin)

debug: cflags += -g -fsanitize=address,undefined -fno-omit-frame-pointer

debug: clean $(bin)

install: $(bin)
	cp $(bin) /usr/local/bin/$(bin)

clean:
	rm -f $(bin) $(obj)
