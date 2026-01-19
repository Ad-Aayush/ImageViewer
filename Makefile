CC = gcc
CXX = g++

CFLAGS = -g -Wall -Wextra -D_POSIX_C_SOURCE=200112L
CXXFLAGS = -g -Wall -Wextra -std=c++17

LIBS = -lwayland-client -lrt

TARGET = my_viewer
ARGS ?= image.bmp

OBJS = xdg-shell-protocol.o client.o image_viewer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

image_viewer.o: image_viewer.cpp
	$(CXX) $(CXXFLAGS) -c image_viewer.cpp -o image_viewer.o

client.o: client.c
	$(CC) $(CFLAGS) -c client.c -o client.o

xdg-shell-protocol.o: xdg-shell-protocol.c
	$(CC) $(CFLAGS) -Wno-pedantic -c xdg-shell-protocol.c -o xdg-shell-protocol.o

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	./$(TARGET) $(ARGS)

.PHONY: all clean run