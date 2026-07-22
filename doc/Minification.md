Minification
===========

WebMinCer minifies CSS, JavaScript, XML, HTML, and JSON. HTML and XML input can also contain inline CSS, JavaScript, and JSON data, which are minified with the corresponding minifier.

The minifiers are deliberately conservative. They remove or shorten syntax only when WebMinCer can preserve the meaning of standards-conformant input. The program reports recognized syntax errors instead of attempting to repair them.


Common processing
-----------------

All formats remove syntactically insignificant whitespace. Comments are removed where the relevant format permits it. Tokens that require a separator remain separated, so minification does not merge tokens into a different construct.

WebMinCer reads one input file at a time. Use `-` as the input file to read standard input. The `--benchmark` option reports the reduction instead of writing the minified result.


CSS
---

The CSS minifier removes comments and unnecessary whitespace, including whitespace around punctuation where CSS does not require it. It also applies value-level reductions that are known to be safe, including:

- Removing redundant units from zero values where the unit is not required.

- Shortening numbers by removing unnecessary leading and trailing zeroes.

- Choosing shorter equivalent colour spellings for supported named and hexadecimal colours.

- Shortening hexadecimal colours such as `#aabbcc` to `#abc` when possible.

- Converting comma-separated integer and endpoint-percentage `rgb()` colours to shorter named or hexadecimal alternatives.

- Converting selected exact units when their shorter equivalent is known.

CSS is minified both as standalone input and inside HTML and XML style elements.


JavaScript
----------

The JavaScript minifier removes comments and syntactically insignificant whitespace while preserving required token boundaries. It also removes punctuation and statement separators when JavaScript grammar makes them redundant, and handles syntax that needs special care, such as regular expressions, strings, template literals, functions, and control-flow statements.

JavaScript is minified both as standalone input and inside HTML and XML script elements. Event-handler attributes are also minified. HTML `javascript:` URLs are minified after the prefix. The optional `--mangle` pass shortens eligible JavaScript identifiers after normal minification. Its rules, supported language modes, and conservative safety restrictions are documented in [JavaScript Identifier Mangling](JSMangler.md).


JSON
----

The JSON minifier validates the complete document and removes whitespace outside strings. Input is expected to use UTF-8. It shortens Unicode escapes when the decoded UTF-8 representation is shorter and preserves escapes required for valid JSON or safe embedded JSON. It preserves string contents, number spellings, object keys, array order, and literal values. Invalid JSON is rejected with an error.

JSON is minified both as standalone input and in script elements with the `application/json+ld` or `importmap` type.


HTML and XML
------------

The HTML and XML minifiers remove comments and whitespace that is insignificant to markup syntax. The HTML minifier also removes quotes from safe attribute values and omits redundant empty and boolean attribute values. XML attribute syntax is preserved. They recognize embedded script, style, and supported JSON data and pass that content through the relevant minifier. JavaScript modules are recognized through `type="module"`, and classic JavaScript through `type="text/javascript"` or the default script type.

HTML and XML text nodes are preserved by default because CSS can make any element preserve whitespace. Use `--compact-ws` to compact whitespace in text nodes when a layout change is acceptable.

For embedded XML content, WebMinCer decodes entities and CDATA before minifying script or style content, then writes an equivalent compact representation. It preserves content where escaping is necessary for valid markup.


What WebMinCer does not do
---------------------------

WebMinCer does not perform cross-file analysis, bundling, tree shaking, source-map processing, image optimisation, or semantic rewrites. It does not rename CSS classes, HTML identifiers, property names, or externally visible JavaScript bindings. These transformations require project-specific knowledge that a single input file cannot provide safely.


Possible future work
--------------------

- JavaScript source-map generation so minified JavaScript remains debuggable.

- Highly restricted JavaScript property mangling for deliberately closed-world bundler output that follows an explicit property-name convention. Bundling alone does not make it safe because property names can be observed dynamically or used by external code.

- Frequency-based JavaScript mangled-name assignment.

- More CSS colour and function reductions, such as safe `hsl()` alternatives.

- UTF-16 input support.
