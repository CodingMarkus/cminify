CC ?= cc
LDFLAGS ?=
VERSION ?= 1.1
VERSION_CFLAGS := -DWEBMINCER_VERSION=\"$(VERSION)\"

BASE_CFLAGS ?= \
	-Werror \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-Wconversion \
	-Wsign-conversion
BUILD_CFLAGS ?= -Os -g
DEBUG_CFLAGS ?= -O0 -g

OUTPUT := webmincer

BUILD_DIR := .build
DEBUG_BUILD_DIR := $(BUILD_DIR)/debug
BUILD_OBJECT_DIR := $(BUILD_DIR)/obj
DEBUG_OBJECT_DIR := $(DEBUG_BUILD_DIR)/obj

SOURCES := $(wildcard src/*.c)
HEADERS := $(wildcard src/*.h)
TEST_SCRIPTS ?= $(wildcard test/stage*/test-*.sh)

BUILD_OBJECTS := $(patsubst src/%.c,$(BUILD_OBJECT_DIR)/%.o,$(SOURCES))
DEBUG_OBJECTS := $(patsubst src/%.c,$(DEBUG_OBJECT_DIR)/%.o,$(SOURCES))

TEST_BINARY ?= ./$(BUILD_DIR)/$(OUTPUT)
TEST_OBJECT_DIR ?= ./$(BUILD_OBJECT_DIR)
DEBUG_TEST_BINARY ?= ./$(DEBUG_BUILD_DIR)/$(OUTPUT)
DEBUG_TEST_OBJECT_DIR ?= ./$(DEBUG_OBJECT_DIR)

DEPLOY_IMAGE ?= webmincer-deploy



.PHONY: build
build: $(BUILD_DIR)/$(OUTPUT)

.PHONY: version
version:
	@printf '%s\n' "$(VERSION)"

$(BUILD_DIR)/$(OUTPUT): $(BUILD_OBJECTS) | $(BUILD_DIR)
	$(CC) $(BASE_CFLAGS) $(BUILD_CFLAGS) $(LDFLAGS) -o $@ $(BUILD_OBJECTS)

$(BUILD_OBJECT_DIR)/%.o: src/%.c $(HEADERS) Makefile | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(VERSION_CFLAGS) $(BUILD_CFLAGS) -c -o $@ $<

$(DEBUG_OBJECT_DIR)/%.o: src/%.c $(HEADERS) Makefile | $(DEBUG_BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(VERSION_CFLAGS) $(DEBUG_CFLAGS) -c -o $@ $<

$(DEBUG_BUILD_DIR)/$(OUTPUT): $(DEBUG_OBJECTS) | $(DEBUG_BUILD_DIR)
	$(CC) $(BASE_CFLAGS) $(DEBUG_CFLAGS) $(LDFLAGS) -o $@ $(DEBUG_OBJECTS)

$(BUILD_DIR):
	mkdir -p $@



.PHONY: debug
debug: $(DEBUG_BUILD_DIR)/$(OUTPUT)

$(DEBUG_BUILD_DIR):
	mkdir -p $@



.PHONY: test
test: build
	@for testScript in $(TEST_SCRIPTS); do \
		WEBMINCER_BINARY="$(TEST_BINARY)" \
		WEBMINCER_OBJECT_DIR="$(TEST_OBJECT_DIR)" \
		$$testScript || exit 1; \
	done



.PHONY: bench
bench: build
	@for testScript in $(TEST_SCRIPTS); do \
		WEBMINCER_BINARY="$(TEST_BINARY)" \
		WEBMINCER_OBJECT_DIR="$(TEST_OBJECT_DIR)" \
		$$testScript --bench || exit 1; \
	done



.PHONY: test-debug
test-debug: debug
	@for testScript in $(TEST_SCRIPTS); do \
		WEBMINCER_BINARY="$(DEBUG_TEST_BINARY)" \
		WEBMINCER_OBJECT_DIR="$(DEBUG_TEST_OBJECT_DIR)" \
		$$testScript || exit 1; \
	done



.PHONY: deploy
deploy:
	docker build -t $(DEPLOY_IMAGE) -f deploy/Dockerfile deploy
	docker run --rm -v "$(CURDIR):/proj" -w /proj $(DEPLOY_IMAGE) \
		deploy/build.sh



.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)



.PHONY: test-clean
test-clean:
	rm -rf .test



.PHONY: deploy-clean
deploy-clean:
	rm -rf .deploy



.PHONY: clean-all
clean-all: clean test-clean deploy-clean
