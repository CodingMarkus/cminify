WebMinCer
=========

WebMinCer is a minifier for CSS, JavaScript, XML, HTML and JSON, written in C. It can also minify inline scripts, stylesheets and JSON data embedded in XML and HTML markup. JSON input is validated completely. For the other formats, many, though not all, classes of syntax errors are detected. When invalid input is recognized, precise error messages are printed.

This project is mirrored on [GitHub](https://github.com/CodingMarkus/WebMinCer).

The name WebMinCer is a play on web, minification, and C, as well as the word mincer. The name reflects the tool's purpose: it effectively feeds web-language source files through a virtual mincer to produce compact output.


Command-line reference
----------------------

```
Usage:
    webmincer <format> <input-file|-> [options]


Formats:
    css    Minify CSS input.
    js     Minify JavaScript input.
    xml    Minify XML input.
    html   Minify HTML input.
    json   Minify JSON input.


Options:
    -h, -help, --help       Show this help page.

    --benchmark             Print size reduction statistics.

    --mangle                Enable output mangling.

                            Currently only JavaScript code is
                            mangled, so this has no effect unless
                            the format is js or the input is HTML
                            with embedded JavaScript.

    --compact-ws
                            Compact whitespace in HTML and XML text
                            nodes. This can change the rendered layout.

    --version               Print version 1.0.


Notes:
    Use '-' as the input file to read from standard input.
```

Building and testing
--------------------

WebMinCer uses a small `Makefile` with separate targets for optimized,
debug, and test builds.

- `make` or `make build` builds the optimized binary at
  `.build/webmincer`.

- `make debug` builds a debug binary at `.build/debug/webmincer`.

- `make test` builds the optimized binary and runs all test scripts
  against `.build/webmincer`.

- `make bench` runs the optimized test suite with benchmarking enabled. See [Tests](doc/dev/Tests.md) for details.

- `make test-debug` builds the debug binary and runs all test scripts
  against `.build/debug/webmincer`.

- `make clean` removes the entire `.build` directory.

Object files are written to `.build/obj` for optimized builds and to
`.build/debug/obj` for debug builds.

Build requirements are a POSIX shell environment, `make`, and a modern C
compiler. The build uses the `cc` command by default, but you can
override it, for example with `make CC=clang` or `make CC=gcc`.

Design objectives
-----------------

- Released as single binary with no dependencies except `libc`.

- Easy to build with a simple `make` invocation.

- The only build requirements are a POSIX shell environment, make and a modern C compiler (clang recommended, gcc possible as well, others might work).

- It is not intended to provide bindings for scripting languages.

- Also minify inline CSS, JavaScript and JSON in HTML and XML documents.

- Extensive unit testing of all features.

- Produce standards-conformant output from any standards-conformant input.

- The program should fail on syntax errors and must not attempt to fix them.

- This minifier is not a cleaner. It should not modify the semantics of the markup.


Documentation
-------------

- Technical details about the current minifiers and their scope are documented in [Minification](doc/Minification.md).

- [Current size-reduction baselines](doc/CurrentSizeReductionBaselines.md) compare original, minified, and mangled JavaScript library sizes.

- [Development](doc/Development.md) covers building, tests, and development utilities.



Origins and relationship to cminify
-----------------------------------

WebMinCer started as a fork of [cminify](https://codeberg.org/Jumping-Beaver/cminify), the minifier originally developed by Jumping-Beaver (aka Lerchensporn). The original project and its design rationale are preserved in the [original cminify README](doc/cminify-README.md).

We are grateful to Jumping-Beaver for the substantial work that made this project possible. WebMinCer builds on that foundation, while its goals may differ slightly from those of cminify and it is intended to be developed more actively.

WebMinCer is an independent project. It is not intended to be a drop-in replacement for cminify, and its goal is not to push changes back into cminify. Users should therefore review the behavior and goals of WebMinCer before adopting it in place of cminify.
