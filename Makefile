CC ?= cc
CFLAGS ?= -O2 -g -Werror -Wall -Wextra -Wno-unused-parameter
OUTPUT := webmincer

.PHONY: build
build: .build/$(OUTPUT)

.build/$(OUTPUT): src/webmincer.c
	mkdir -p .build
	$(CC) $(CFLAGS) -o .build/$(OUTPUT) src/webmincer.c


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
