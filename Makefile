CC ?= gcc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/linux_device_uplink_demo
SRC_DIR := src
NANO_INCLUDE_DIR := 3rd/include
NANO_LIB := 3rd/lib/libtirtc.a

SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/device_demo_streamer.c

OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CFLAGS := -std=c11 -Wall -Wextra -Werror -I$(SRC_DIR) -I$(NANO_INCLUDE_DIR)
LDFLAGS := $(NANO_LIB) -pthread -lm

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(NANO_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
