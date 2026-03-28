CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Isrc -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm

SRC = src/YMLParser.c src/_da.c src/_hm.c src/_lexer.c
OBJ = $(SRC:.c=.o)

all: example

example: $(OBJ) example.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) example
