CC = gcc
CFLAGS = -Wall -Wextra -std=c11 $(shell pkg-config --cflags sdl2 SDL2_image lua5.4)
LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image lua5.4) -lm

SRC = main.c function.c graphics.c control.c physics.c
OBJ = $(SRC:.c=.o)
TARGET = luaplay

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c function.h graphics.h control.h physics.h
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
