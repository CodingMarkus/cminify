COMPILER ?= cc
CC ?= $(COMPILER)
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter
ifndef CROSS_TRIPLE
OUTPUT := webmincer
else ifeq '$(CROSS_TRIPLE)' 'x86_64-w64-mingw32'
OUTPUT := webmincer_$(CROSS_TRIPLE).exe
else
OUTPUT := webmincer_$(CROSS_TRIPLE)
endif

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
	./test/test-css.sh
	./test/test-html.sh
	./test/test-js.sh
	./test/test-js-mangling.sh
	./test/test-js-libs.sh
	./test/test-json.sh
	./test/test-random-input.sh

.PHONY: check
check:
	cppcheck --enable=all --suppress=missingIncludeSystem --check-level=exhaustive src/webmincer.c

.PHONY: clean
clean:
	rm -rf .build

.PHONY: crossbuild
crossbuild:
	docker run -e CROSS_TRIPLE=x86_64-w64-mingw32 \
		-v $$(pwd):/workdir:z -u $$(id -u):$$(id -g) multiarch/crossbuild make && \
	docker run -e CROSS_TRIPLE=x86_64-apple-darwin \
	-v $$(pwd):/workdir:z -u $$(id -u):$$(id -g) multiarch/crossbuild make
	docker run -e CROSS_TRIPLE=x86_64-linux-gnu \
		-v $$(pwd):/workdir:z -u $$(id -u):$$(id -g) multiarch/crossbuild make
