CC ?= cc
CFLAGS ?= -O2 -g -Werror -Wall -Wextra -Wno-unused-parameter
OUTPUT := webmincer
SOURCES := \
	src/js-mangler.c \
	src/minifier-common.c \
	src/minifier-css.c \
	src/minifier-js.c \
	src/minifier-json.c \
	src/minifier-xml-html.c \
	src/webmincer.c
HEADERS := \
	src/js-mangler.h \
	src/minifier.h

.PHONY: build
build: .build/$(OUTPUT)

.build/$(OUTPUT): $(SOURCES) $(HEADERS)
	mkdir -p .build
	$(CC) $(CFLAGS) -o .build/$(OUTPUT) $(SOURCES)


.PHONY: strip
strip: .build/$(OUTPUT)
	strip .build/$(OUTPUT)


.PHONY: test
test: build
	./test/test-xml.sh
	./test/test-html.sh
	./test/test-css.sh
	./test/test-json.sh
	./test/test-js.sh
	./test/test-js-mangling.sh
	./test/test-js-libs.sh
	./test/test-random-input.sh


.PHONY: clean
clean:
	rm -rf .build
