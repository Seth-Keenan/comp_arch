CC = gcc
CFLAGS = -Wall -Wextra -O2 -Isrc

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = riscv-hex-converter

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
    $(CC) $(CFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c
    $(CC) $(CFLAGS) -c $< -o $@

run: all
    ./$(BIN)

clean:
    rm -f src/*.o $(BIN)