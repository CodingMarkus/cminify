CC ?= cc

BASE_CFLAGS ?= \
	-Werror \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wconversion \
	-Wsign-conversion
BUILD_CFLAGS ?= -O2 -g
DEBUG_CFLAGS ?= -O0 -g

OUTPUT := webmincer

BUILD_DIR := .build
DEBUG_BUILD_DIR := $(BUILD_DIR)/debug
BUILD_OBJECT_DIR := $(BUILD_DIR)/objects
DEBUG_OBJECT_DIR := $(DEBUG_BUILD_DIR)/objects

SOURCES := $(wildcard src/*.c)
HEADERS := $(wildcard src/*.h)
TEST_SCRIPTS := $(wildcard test/test-*.sh)

BUILD_OBJECTS := $(patsubst src/%.c,$(BUILD_OBJECT_DIR)/%.o,$(SOURCES))
DEBUG_OBJECTS := $(patsubst src/%.c,$(DEBUG_OBJECT_DIR)/%.o,$(SOURCES))



.PHONY: build
build: $(BUILD_DIR)/$(OUTPUT)

$(BUILD_DIR)/$(OUTPUT): $(BUILD_OBJECTS) | $(BUILD_DIR)
	$(CC) $(BASE_CFLAGS) $(BUILD_CFLAGS) -o $@ $(BUILD_OBJECTS)

$(BUILD_OBJECT_DIR)/%.o: src/%.c $(HEADERS) | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(BUILD_CFLAGS) -c -o $@ $<

$(DEBUG_OBJECT_DIR)/%.o: src/%.c $(HEADERS) | $(DEBUG_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(DEBUG_CFLAGS) -c -o $@ $<

$(DEBUG_BUILD_DIR)/$(OUTPUT): $(DEBUG_OBJECTS) | $(DEBUG_BUILD_DIR)
	$(CC) $(BASE_CFLAGS) $(DEBUG_CFLAGS) -o $@ $(DEBUG_OBJECTS)

$(BUILD_DIR):
	mkdir -p $@



.PHONY: debug
debug: $(DEBUG_BUILD_DIR)/$(OUTPUT)

$(DEBUG_BUILD_DIR):
	mkdir -p $@



.PHONY: test
test: build
	for testScript in $(TEST_SCRIPTS); do \
		WEBMINCER_BINARY=./$(BUILD_DIR)/$(OUTPUT) \
		WEBMINCER_OBJECT_DIR=./$(BUILD_OBJECT_DIR) \
		$$testScript || exit 1; \
	done



.PHONY: test-debug
test-debug: debug
	for testScript in $(TEST_SCRIPTS); do \
		WEBMINCER_BINARY=./$(DEBUG_BUILD_DIR)/$(OUTPUT) \
		WEBMINCER_OBJECT_DIR=./$(DEBUG_OBJECT_DIR) \
		$$testScript || exit 1; \
	done



.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
