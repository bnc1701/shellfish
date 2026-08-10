CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -O2
SRC = src/main.c src/shell.c src/parser.c src/exec.c src/builtins.c src/history.c
BIN = shellfish

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

.PHONY: clean install run

run: $(BIN)
	./$(BIN)

install: $(BIN)
	cp $(BIN) /usr/local/bin/$(BIN)

clean:
	rm -f $(BIN)
