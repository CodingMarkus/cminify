Tests
=====

Run `make test` to build the optimized binary and run the complete test suite. Run `make test-debug` to test the debug build.


Test scripts
------------

- `test-build-layout.sh` verifies that the binary and object files are written to the expected build directories and that no object files are left in `src/`.

- `test-cli.sh` verifies help and version output, including that the README contains the complete help text.

- `test-css.sh` verifies CSS whitespace, comments, values, colours, units, and error handling.

- `test-html.sh` verifies HTML minification, inline script and JSON handling, event attributes, and `--compact-ws`.

- `test-js.sh` verifies JavaScript syntax preservation, whitespace and comment removal, and JavaScript-specific output reductions.

- `test-js-libs.sh` minifies pinned third-party JavaScript libraries and validates the generated JavaScript. Its baseline workflow is documented in [Size reduction baseline](SizeReductionBaseline.md).

- `test-js-mangling.sh` verifies optional JavaScript identifier mangling in JavaScript, HTML, and XML input.

- `test-json.sh` verifies JSON minification and validation.

- `test-random-input.sh` sends random input to every supported format and verifies that invalid input reports an error instead of crashing.

- `test-xml.sh` verifies XML minification, entities, CDATA, inline script and style handling, and `--compact-ws`.

- `lib-assert.sh` provides shared assertions for the format-specific tests.
