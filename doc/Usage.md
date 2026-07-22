WebMinCer usage
===============

WebMinCer minifies UTF-8 CSS, JavaScript, XML, HTML, and JSON. It writes the minified result to standard output, so the caller chooses where to save it. For the minification behavior and its scope, see [Minification](Minification.md).


Command-line reference
----------------------

```
USAGE

    webmincer <format> <input-file|-> [options]


FORMATS

    css    Minify CSS input.
    js     Minify JavaScript input.
    xml    Minify XML input.
    html   Minify HTML input.
    json   Minify JSON input.


OPTIONS

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

    --version               Print the version.


NOTES

    Use '-' as the input file to read from standard input.
```


Input and output
----------------

WebMinCer accepts an input format and one input file. The format does not have to match the input file name, but it must match the file contents. For example, to minify a stylesheet or an HTML document:

```
webmincer css site.css > site.min.css
webmincer html index.html > index.min.html
```

Use `-` as the input file to read standard input. This works well in shell pipelines and when an input file needs a different output name:

```
webmincer js - < app.js > app.min.js
```

WebMinCer processes one input at a time. It does not modify the source file or write an output file itself.


Optional processing
-------------------

Use `--mangle` to shorten eligible JavaScript identifier names after normal minification. It applies to JavaScript input and JavaScript embedded in HTML. See [JavaScript Identifier Mangling](JSMangler.md) for its supported language modes and safety restrictions.

HTML and XML text nodes are preserved by default because whitespace can affect their rendered layout. Use `--compact-ws` only when compacting that whitespace is acceptable.


Benchmarking
------------

Use `--benchmark` to print the size reduction instead of the minified result:

```
webmincer js app.js --benchmark
```

This is useful for comparing output sizes, but it does not write the minified content to standard output. See [current size-reduction baselines](CurrentSizeReductionBaselines.md) for benchmark results from supported JavaScript libraries.
