Tests
=====

Run `make test` to build the optimized binary and run the complete test suite. It checks correctness only. Run `make test-debug` to test the debug build.

Run `make bench` to run the optimized test suite with the `--bench` option. Most test scripts currently ignore the option. `test-js-libs.sh` uses it to compare the JavaScript library size-reduction results to the documented baseline.

Every test script prints its name followed by `PASSED` when it succeeds. Failed tests print the test name followed by `FAILED`, then their diagnostics. `test-js-libs.sh` prints `SKIPPED` and its reason when neither Bun nor Node.js, or neither wget nor curl, is available. It also prints the JavaScript runtime it used. If the terminal supports colours, the PASSED and FAILED statuses are coloured green or red.


Test configuration
------------------

Test scripts use the following environment variables. The Makefile supplies the two `WEBMINCER_` variables; they are useful when running individual test scripts against another build.

- `WEBMINCER_BINARY` sets the WebMinCer executable path. The default is `./.build/webmincer`.

- `WEBMINCER_OBJECT_DIR` sets the object-file directory for `test-build.sh`. The default is `./.build/obj`.

- `TMPDIR` sets the directory used for temporary test files. If it is unset, `mktemp` selects the system default.

- `LC_ALL`, `LC_CTYPE`, or `LANG` select whether test statuses use UTF-8 symbols.

- `TERM` controls colour output. Set it to `dumb` to disable colours on an interactive terminal.

The scripts use the normal shell `PATH` to locate required commands, including `bun` or `node`, `wget`, and `curl`. `test-js-libs.sh` prefers Bun for JavaScript validation and falls back to Node.js. It skips instead of failing when neither JavaScript runtime or downloader is available.

Tests with persistent test data store it in `.test/stageX/test-name/`, matching the test's stage and name.

Run `make test-clean` to remove the persistent test data.


Test stages
-----------

The Makefile discovers `test-*.sh` scripts in `test/stage*/` and runs them in stage and filename order. Place a new test in the stage that matches its cost and diagnostic value. Use zero-padded stage names if there are ten or more stages.


### Stage 1

- `test-build.sh` verifies that the binary and object files are written to the expected build directories and that no object files are left in `src/`.

- `test-cli.sh` verifies help and version output, including that the README contains the complete help text.


### Stage 2

- `test-css.sh` verifies CSS whitespace, comments, values, colours, units, and error handling.

- `test-html.sh` verifies HTML minification, inline script and JSON handling, event attributes, and `--compact-ws`.

- `test-js.sh` verifies JavaScript syntax preservation, whitespace and comment removal, and JavaScript-specific output reductions.

- `test-js-mangling.sh` verifies optional JavaScript identifier mangling in JavaScript, HTML, and XML input.

- `test-json.sh` verifies JSON minification and validation.

- `test-xml.sh` verifies XML minification, entities, CDATA, inline script and style handling, and `--compact-ws`.


### Stage 3

- `test-js-libs.sh` minifies pinned third-party JavaScript libraries and validates the generated JavaScript. It also fails if either output grows beyond its input, or if mangling makes the output bigger than normal minification. With `--bench`, it verifies the size-reduction baseline. `--print-sizes` prints the generated table without comparing it to the baseline. Combine both options to print and verify the sizes. `-h`, `-help`, and `--help` print the supported options. Its baseline workflow is documented in [Size reduction baseline](SizeReductionBaseline.md).


### Stage 4

- `test-random-input.sh` sends random input to every supported format and verifies that invalid input reports an error instead of crashing.


Shared test helpers
-------------------

- `lib-assert.sh` provides shared assertions for the format-specific tests.
