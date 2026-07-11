JavaScript Identifier Mangling
=============================

JavaScript identifier mangling is an optional second pass after normal JavaScript minification. It shortens local variable names while preserving the external interface and semantics of the input unit.

The feature is disabled by default and is enabled through a dedicated CLI flag. When the flag is absent, JavaScript minification behaves as before.


Purpose and scope
-----------------

The mangler is designed for WebMinCer's low-dependency, single-binary architecture. It is deliberately conservative:

- Local JavaScript identifiers are shortened safely.
- Short names can be reused in different functions.
- Names that may be observed from outside the input unit are preserved.
- Property names, class member names, string contents, regular expression contents, source maps, cross-file analysis, and bundling are out of scope.

For the best shortening across several files, bundle the code first and run the mangler on the resulting single JavaScript file.


Input boundary and language modes
---------------------------------

The current input file is the unit of analysis. An identifier may be renamed only when it is provably local to that unit.

In classic script mode, top-level bindings are externally observable and are therefore not renamed. In module mode, top-level non-exported bindings are local and may be renamed. Exported bindings are never renamed.

Imported local bindings may be renamed because only the local binding name changes:

- `import { x as y } from "m"`: `y` may be renamed, but `x` is preserved.
- `import x from "m"`: the local `x` may be renamed.
- `import * as ns from "m"`: the local `ns` may be renamed.
- `import { x } from "m"`: a rename becomes `import { x as a } from "m"`.

For example, `export const foo=1` keeps `foo` unchanged.


Processing pipeline
-------------------

The JavaScript pipeline is:

1. Minify the input with the existing JavaScript minifier.
2. Mangle identifiers in the minified output when the feature is enabled.
3. Return the resulting output.

The mangler uses three scans over the already minified source:

1. The count pass counts tokens, scopes, declarations, references, shorthand-property expansions, and any additional output bytes. It also detects conditions that require mangling to be disabled.
2. The collect pass allocates flat arrays from those counts, then records tokens, scopes, declarations, references, and binding information.
3. The emit pass resolves references, assigns names, and writes the output to one preallocated buffer.

The count-first design avoids `realloc()` during normal mangling and allows the output buffer to account for shorthand-property expansion.


Bindings, scopes, and references
-------------------------------

The implementation uses flat arrays and indices rather than pointer-heavy graphs. The main structures represent tokens, scopes, declarations, references, and mangled names. A rename map is retained so future debug information can report how bindings were renamed.

The scope model contains global, function, block, catch, and named function-expression scopes. The binding rules are:

- `var` belongs to the nearest function scope, or the global scope.
- `let` and `const` belong to the current lexical block scope.
- Function parameters belong to the function scope.
- Catch bindings belong to the catch scope.
- Top-level declarations are top-level bindings.
- Block-level function declarations are block-scoped.
- Named function expressions create a name visible only inside the function.

Declarations include variable and function declarations, parameters, catch parameters, destructuring bindings, and imported local bindings. A reference is an identifier used in an expression, including returns, assignments, calls, computed values, and shorthand object properties.

The following are not variable references:

- A property name after `.` such as `obj.name`.
- An explicit object key such as `{name: value}`.
- Labels, keywords, and identifiers inside strings, template raw segments, regular expressions, or comments.

Every reference resolves to the nearest visible declaration with the same source name. Unresolved identifiers are external and are never renamed.


Shorthand properties and destructuring
---------------------------------------

When a shorthand object property refers to a renamed identifier, it must be expanded. For example:

    let alpha=1;let obj={alpha}

can become:

    let a=1;let obj={a:a}

The count pass includes the additional bytes required for this expansion. Destructuring bindings are collected recursively. In `let {x}=obj`, `x` is a declaration. In `let {x:y}=obj`, only `y` is a declaration. In `let [a,b]=arr`, both `a` and `b` are declarations.


Conservative safety rules
-------------------------

Direct `eval(...)` and `with(...)` can make otherwise local names observable. If a function contains either construct, mangling is disabled for that function and all nested scopes. If either construct occurs in top-level code, mangling is disabled for the entire script or module.

The first implementation favors disabling too much over making an unsafe rename. This rule also applies to any future syntax that cannot be analyzed conservatively.


Mangled name policy
-------------------

Local and global bindings use separate naming spaces.

Local names are reused across different functions, subject to visibility and shadowing rules. The initial order is:

- `a` through `z`.
- `A` through `Z`.
- `_a` through `_z`, then `_A` through `_Z`, followed by `_aa`, `_ab`, and so on.

Global names are never reused and always begin with `G`:

- `G0` through `G9`.
- `Ga` through `Gz`.
- `GA` through `GZ`.
- `G00`, `G01`, and so on.

The prefixes avoid collisions with JavaScript reserved words and make global mangled names visually distinct. The first implementation uses one local name allocator per function tree and prioritizes safe reuse across disjoint functions over maximum reuse across sibling blocks.


Output generation
-----------------

The output starts with the minified input length plus one byte for the terminator. The count pass adds bytes for shorthand-property expansion and other guaranteed growth. Declarations and references emit their mangled name when one is assigned. Shorthand properties emit the expanded `name:name` form. All other tokens retain their original source text.


Implementation and validation
-----------------------------

The implementation can be developed in this order:

1. Add the CLI flag and pass wiring, initially with the mangler disabled internally. The current flag name is `--mangle`.
2. Add count-pass data structures and the counting scan.
3. Add collection of scopes, declarations, and references.
4. Support ordinary declarations and references.
5. Resolve names and emit renamed identifiers.
6. Add shorthand-property expansion and destructuring.
7. Add import and export handling.
8. Add conservative `eval` and `with` disabling.
9. Refine local-name reuse per function tree.

Existing JavaScript tests must continue to pass. New tests should cover local renaming, nested shadowing, `var` hoisting, parameters, named function expressions, catch bindings, shorthand properties, destructuring, classic script top-level bindings, module bindings, import aliases, exports, and `eval` and `with` disabling.

Future bundling or single-file merge support would naturally expose more code to local mangling without changing these core rules.
