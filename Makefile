cc = gcc
cflags = -Wall -Wextra -std=gnu11 -O2 -Iinclude
src = src/main.c src/shell.c src/parser.c src/exec.c src/builtins.c src/history.c
bin = shellfish

.PHONY: all clean install run

all: $(bin)

$(bin): $(src)
	$(cc) $(cflags) -o $(bin) $(src)

run: $(bin)
	./$(bin)

install: $(bin)
	cp $(bin) /usr/local/bin/$(bin)

clean:
	rm -f $(bin)
