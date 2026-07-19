WebMinCer
=========

WebMinCer is a minifier for CSS, JavaScript, XML, HTML and JSON, written in C. It can also minify inline scripts, stylesheets and JSON data embedded in XML and HTML markup. JSON input is validated completely. For the other formats, many, though not all, classes of syntax errors are detected. When invalid input is recognized, precise error messages are printed.

The original and actively developed project is hosted on [Codeberg](https://codeberg.org/CodingMarkus/WebMinCer). \
Pre-built binary releases are also available on [Codeberg](https://codeberg.org/CodingMarkus/WebMinCer/releases). \
Additionally this project is also mirrored on [GitHub](https://github.com/CodingMarkus/WebMinCer).

The name WebMinCer is a play on web, minification, and C, as well as the word mincer. The name reflects the tool's purpose: it effectively feeds web-language source files through a virtual mincer to produce compact output.


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

- [Usage](doc/Usage.md) documents the command-line interface.

- [Download](doc/Download.md) lists the available binary releases.

- [Feedback](doc/Feedback.md) explains how to report bugs and request features.

- [Development](doc/Development.md) covers building, tests, and development utilities.



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

- `make test-clean` removes the persistent `.test` test-data directory.

- `make deploy-clean` removes the entire `.deploy` directory.

- `make clean-all` removes `.build`, `.test`, and `.deploy`.

Object files are written to `.build/obj` for optimized builds and to
`.build/debug/obj` for debug builds.

Build requirements are a POSIX shell environment, `make`, and a modern C
compiler. The build uses the `cc` command by default, but you can
override it, for example with `make CC=clang` or `make CC=gcc`.



Pre-built releases
------------------

Pre-built releases are available from the [Codeberg release page](https://codeberg.org/CodingMarkus/WebMinCer/releases). Download the binary-only archive for your operating system and processor architecture.

- Linux x86: 32-bit i686. Download `WebMinCer_1.0_Linux-x86.tar.xz`, or `WebMinCer_1.0_Linux-x86-static.tar.xz` for a self-contained build.

- Linux x64: 64-bit x86_64. Download `WebMinCer_1.0_Linux-x64.tar.xz`, or `WebMinCer_1.0_Linux-x64-static.tar.xz` for a self-contained build.

- Linux arm64: 64-bit ARM. Download `WebMinCer_1.0_Linux-arm64.tar.xz`, or `WebMinCer_1.0_Linux-arm64-static.tar.xz` for a self-contained build.

- macOS x64: Intel Macs. Download `WebMinCer_1.0_macOS-x64.tar.xz`.

- macOS arm64: Apple silicon Macs. Download `WebMinCer_1.0_macOS-arm64.tar.xz`.

- Windows x64: 64-bit x86_64 systems. Download `WebMinCer_1.0_Windows-x64.zip`.

- Windows arm64: 64-bit ARM systems. Download `WebMinCer_1.0_Windows-arm64.zip`.

Linux and macOS archives use the `.tar.xz` format. Windows archives use the `.zip` format. Binary-only archives contain only the release binary. Static Linux archives have `-static` in their names and do not require a system glibc installation.

Developer archives have names such as `webmincer_1.0_dev_x86_64-linux-gnu.tar.xz`. They include the release binary, debugging symbols, and build information for developers who need them.


Origins and relationship to cminify
-----------------------------------

WebMinCer started as a fork of [cminify](https://codeberg.org/Jumping-Beaver/cminify), the minifier originally developed by Jumping-Beaver (aka Lerchensporn). The original project and its design rationale are preserved in the [original cminify README](doc/cminify-README.md).

We are grateful to Jumping-Beaver for the substantial work that made this project possible. WebMinCer builds on that foundation, while its goals may differ slightly from those of cminify and it is intended to be developed more actively.

WebMinCer is an independent project. It is not intended to be a drop-in replacement for cminify, and its goal is not to push changes back into cminify. Users should therefore review the behavior and goals of WebMinCer before adopting it in place of cminify.
