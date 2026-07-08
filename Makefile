COMPILER ?= cc
CFLAGS ?= -O2 -g -Wall
ifndef CROSS_TRIPLE
	OUTPUT := cminify
else ifeq '$(CROSS_TRIPLE)' 'x86_64-w64-mingw32'
	OUTPUT := cminify_$(CROSS_TRIPLE).exe
else
	OUTPUT := cminify_$(CROSS_TRIPLE)
endif

.PHONY: build
build: build/$(OUTPUT)

build/$(OUTPUT): cminify.c
	mkdir -p build
	$(COMPILER) $(CFLAGS) -o build/$(OUTPUT) cminify.c

.PHONY: strip
strip: build/$(OUTPUT)
	strip build/$(OUTPUT)

.PHONY: test
test: build
	./test-xml.sh
	./test-css.sh
	./test-html.sh
	./test-js.sh
	./test-js-libs.sh

.PHONY: check
check:
	cppcheck --enable=all --suppress=missingIncludeSystem --check-level=exhaustive cminify.c

.PHONY: clean
clean:
	rm -rf build

.PHONY: crossbuild
crossbuild:
	docker run -e CROSS_TRIPLE=x86_64-w64-mingw32 \
		-v $$(pwd):/workdir:z -u $$(id -u):$$(id -g) multiarch/crossbuild make && \
	docker run -e CROSS_TRIPLE=x86_64-apple-darwin \
	-v $$(pwd):/workdir:z -u $$(id -u):$$(id -g) multiarch/crossbuild make
	docker run -e CROSS_TRIPLE=x86_64-linux-gnu \
		-v $$(pwd):/workdir:z -u $$(id -u):$$(id -g) multiarch/crossbuild make
