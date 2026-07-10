WebMinCer
=========

WebMinCer is a minifier for CSS, JavaScript, XML, HTML and JSON, written in C. It can also minify inline scripts, stylesheets and JSON data embedded in XML and HTML markup. JSON input is validated completely. For the other formats, many, though not all, classes of syntax errors are detected. When invalid input is recognized, precise error messages are printed.

This project is mirrored on [GitHub](https://github.com/CodingMarkus/WebMinCer).


The name WebMinCer is a play on web, minification, and C, as well as the word mincer. The name reflects the tool's purpose: it effectively feeds web-language source files through a virtual mincer to produce compact output.


Origins and relationship to cminify
-----------------------------------

WebMinCer started as a fork of [cminify](https://codeberg.org/Jumping-Beaver/cminify), the minifier originally developed by Jumping-Beaver (aka Lerchensporn). The original project and its design rationale are preserved in the [original cminify README](doc/cminify-README.md).

We are grateful to Jumping-Beaver for the substantial work that made this project possible. WebMinCer builds on that foundation, while its goals may differ slightly from those of cminify and it is intended to be developed more actively.

WebMinCer is an independent project. It is not intended to be a drop-in replacement for cminify, and its goal is not to push changes back into cminify. Users should therefore review the behavior and goals of WebMinCer before adopting it in place of cminify.


Design objectives
-----------------

- Released as single binary with no dependencies except `libc`.

- Easy to build with a simple `make` invocation.

- The only build requirements are a POSIX shell environment, make and a modern C compiler (clang recommended, gcc possible as well).

- It is not intended to provide bindings for scripting languages.

- Also minify inline CSS, JavaScript and JSON in HTML and XML documents.

- Extensive unit testing of all features.

- Produce standards-conformant output from any standards-conformant input.

- The program should fail on syntax errors and must not attempt to fix them.

- This minifier is not a cleaner. It should not modify the semantics of the markup.

The design of optional JavaScript identifier mangling is documented in [JavaScript Identifier Mangling](doc/JSMangler.md).
