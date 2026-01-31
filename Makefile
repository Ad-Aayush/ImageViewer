CC = gcc
CXX = g++

SRC_DIR = src
BUILD_DIR = build

CFLAGS = -g -Wall -Wextra -D_POSIX_C_SOURCE=200112L -I$(SRC_DIR)
CXXFLAGS = -g -Wall -Wextra -std=c++17 -I$(SRC_DIR)

LIBS = -lwayland-client -lrt

TARGET = $(BUILD_DIR)/my_viewer
ARGS ?= image.bmp

OBJS = $(BUILD_DIR)/xdg-shell-protocol.o $(BUILD_DIR)/client.o \
	$(BUILD_DIR)/image_viewer.o $(BUILD_DIR)/image.o \
	$(BUILD_DIR)/image_loader.o $(BUILD_DIR)/bmp.o \
	$(BUILD_DIR)/qoi.o $(BUILD_DIR)/png_decoder.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/image_viewer.o: $(SRC_DIR)/image_viewer.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/image_viewer.cpp -o $(BUILD_DIR)/image_viewer.o

$(BUILD_DIR)/image.o: $(SRC_DIR)/image.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/image.cpp -o $(BUILD_DIR)/image.o

$(BUILD_DIR)/image_loader.o: $(SRC_DIR)/image_loader.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/image_loader.cpp -o $(BUILD_DIR)/image_loader.o

$(BUILD_DIR)/bmp.o: $(SRC_DIR)/bmp.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/bmp.cpp -o $(BUILD_DIR)/bmp.o

$(BUILD_DIR)/qoi.o: $(SRC_DIR)/qoi.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/qoi.cpp -o $(BUILD_DIR)/qoi.o

$(BUILD_DIR)/png_decoder.o: $(SRC_DIR)/png_decoder.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/png_decoder.cpp -o $(BUILD_DIR)/png_decoder.o

$(BUILD_DIR)/client.o: $(SRC_DIR)/client.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRC_DIR)/client.c -o $(BUILD_DIR)/client.o

$(BUILD_DIR)/xdg-shell-protocol.o: $(SRC_DIR)/xdg-shell-protocol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-pedantic -c $(SRC_DIR)/xdg-shell-protocol.c -o $(BUILD_DIR)/xdg-shell-protocol.o

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	./$(TARGET) $(ARGS)

.PHONY: all clean run
