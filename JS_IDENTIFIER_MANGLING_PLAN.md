JavaScript Identifier Mangling Plan
===================================

Audience: CodingMarkus

Status
------

- Draft implementation plan only.
- This file is intentionally standalone and not linked from existing documentation.
- It is expected to be replaced by user-facing documentation after the implementation is finished.


Overview
--------

- Add an optional JavaScript identifier mangling pass that runs after normal JavaScript minification.
- Mangling is enabled only by an explicit CLI flag.
- The mangler rewrites identifiers that are provably local to the current input unit.
- The mangler must not rewrite identifiers that are part of the external interface of the current input unit.
- The implementation should avoid growing structures during normal operation by using a count pass before allocation.


Goals
-----

- Shorten local JavaScript identifiers safely.
- Keep the implementation compatible with the current low-dependency single-binary design.
- Reuse short local names across different functions.
- Preserve JavaScript semantics conservatively.


Non-goals
---------

- Full JavaScript AST generation.
- Source map generation.
- Cross-file analysis.
- Rewriting property names, class member names, string contents, or regex contents.
- Whole-project bundling in this feature.


Pipeline
--------

- Existing JavaScript minification stays as the first pass.
- Identifier mangling runs as a second pass on already minified JavaScript.
- If mangling is disabled, the second pass is skipped.

Planned flow:

1. `minify_js(input)`
2. `mangle_js_identifiers(minified_js)`
3. return mangled output


Flag Behavior
-------------

- Identifier mangling is off by default.
- A dedicated CLI flag enables it.
- The existing JavaScript minifier behavior stays unchanged when the flag is not used.


Boundary of What May Be Renamed
-------------------------------

- The current input file is treated as the unit of analysis.
- Anything provably local to this input file may be renamed.
- Anything that may be referenced from outside this input file must not be renamed.
- For maximum shortening across multiple files, code should first be bundled into one JavaScript file and then mangled.


Classic Script vs Module Rules
------------------------------

- In classic script mode, top-level bindings are externally observable and must not be renamed.
- In module mode, top-level non-exported bindings are local to the module and may be renamed.
- Exported bindings must not be renamed.
- Imported local bindings may be renamed because only the local binding name changes.

Examples:

- `import { x as y } from "m"`: `y` may be renamed, `x` must not be renamed.
- `import x from "m"`: `x` may be renamed because it is the local binding name for the default import.
- `import * as ns from "m"`: `ns` may be renamed.
- `import { x } from "m"`: if renamed, it must become `import { x as a } from "m"`.
- `export const foo=1`: `foo` must not be renamed.


Pass Structure
--------------

- Use three scans over the already minified JavaScript.
- Use a count-first approach so arrays can be allocated once at exact or safely bounded size.

Passes:

1. Count pass
2. Collect pass
3. Emit pass

Count pass:

- Count tokens.
- Count scopes.
- Count declarations.
- Count references.
- Count shorthand object-property expansions.
- Count any extra output bytes needed for expansions.
- Detect conservative mangling-disable conditions.

Collect pass:

- Allocate arrays using counts from pass 1.
- Re-scan and fill token, scope, declaration, and reference arrays.
- Record enough structure to resolve bindings and emit rewritten code.

Emit pass:

- Resolve identifier references to declarations.
- Assign mangled names.
- Re-scan or walk tokens and emit rewritten output.


Data Model
----------

- Use flat arrays plus indices instead of pointer-heavy graph structures.
- Keep memory layout simple and C-friendly.
- Avoid `realloc()` during normal mangling work after the count pass.

Planned core arrays:

- tokens
- scopes
- declarations
- references
- mangled names

- rename map from original bindings to mangled names, kept so future debug information can report how variables were renamed



Scope Model
-----------

- Use lexical scopes plus function scopes.
- Track enough scope structure to resolve shadowing correctly.

Scope kinds:

- global scope
- function scope
- block scope
- catch scope
- function-name scope for named function expressions

Rules:

- `var` belongs to the nearest enclosing function scope, or global scope if none exists.
- `let` and `const` belong to the current lexical block scope.
- function parameters belong to the function scope.
- catch bindings belong to the catch scope.
- top-level `function`, `var`, `let`, and `const` are top-level bindings.
- block-level function declarations are treated as block-scoped.
- named function expressions create a function-name binding visible only inside that function.


What Counts as a Declaration
----------------------------

- `var a`
- `let a`
- `const a`
- `function a(...)`
- function parameters
- arrow-function parameters
- catch parameters
- destructuring bindings in declarations and parameters
- imported local bindings

Destructuring examples:

- `let {x}=obj`: `x` is a declaration.
- `let {x:y}=obj`: `y` is a declaration, `x` is not.
- `let [a,b]=arr`: `a` and `b` are declarations.


What Counts as a Reference
--------------------------

- Identifier usage in expressions.
- Identifier usage in returns, assignments, calls, and computed value positions.
- Identifier usage in shorthand object properties.

Does not count as a variable reference:

- `obj.name` where `name` is a property name after `.`
- `{name: value}` where `name` is an explicit object literal key
- labels
- keywords
- identifiers inside strings, template raw segments, regexes, or comments


Shorthand Object Properties
---------------------------

- Shorthand object properties must be expanded when the identifier is renamed.

Example:

- Input: `let alpha=1;let obj={alpha}`
- If `alpha -> a`, output must become `let a=1;let obj={a:a}`

- Pass 1 must count these expansions so the output buffer can be sized correctly.


Conservative Disable Rules
--------------------------

- If a function contains direct `eval(...)`, disable mangling for that function and all nested scopes inside it.
- If a function contains `with(...)`, disable mangling for that function and all nested scopes inside it.
- If top-level code contains either of these, disable mangling for the whole script or module.

- The first implementation should prefer disabling too much over renaming unsafely.


Name Resolution
---------------

- Every reference resolves to the nearest visible declaration of the same source name.
- Resolution walks from the current scope upward through parent scopes.
- If no declaration is found, the identifier is treated as external and must not be renamed.
- Shadowing follows the scope rules defined above.


Mangled Name Policy
-------------------

- Use two independent naming spaces:
  - local names
  - global names

- Local names are reused across different functions.
- Global names are unique within the current input unit.


Local Name Order
----------------

- First use one-character local names:
  - `a` to `z`
  - `A` to `Z`

- After those are exhausted, all further local names must begin with `_`:
  - `_a` to `_z`
  - `_A` to `_Z`
  - `_aa`, `_ab`, and so on

Reason:

- One-character names are shortest.
- Once names are longer than one character, the `_` prefix makes
  collisions with JavaScript reserved words impossible.


Global Name Order
-----------------

- Globals always use a `G` prefix.
- Planned order:
  - `G0` to `G9`
  - `Ga` to `Gz`
  - `GA` to `GZ`
  - `G00`, `G01`, and so on

Reason:

- The `G` prefix makes collisions with JavaScript reserved words
  impossible.
- They stay visually distinct from local mangled names.


Name Assignment Rules
---------------------

- Local names may be reused across different function scopes.
- Local names must not be reused where two bindings are simultaneously visible in the same function tree.
- Global names are never reused.
- Exported bindings are never renamed.
- Classic-script top-level bindings are never renamed.
- Module top-level non-exported bindings may be renamed.

Planned first-version simplification:

- Use one local-name allocator per function tree.
- Do not try to maximize reuse across sibling block scopes if that complicates correctness.
- Prefer simple safe reuse across disjoint functions.


Imports and Exports
-------------------

Imports:

- Imported names from another module are never renamed at the import source side.
- The local binding introduced by an import may be renamed if it is not exported again under the local name.

Examples:

- `import { x as y } from "m"`: rename `y`, never `x`.
- `import x from "m"`: rename local `x`.
- `import * as ns from "m"`: rename local `ns`.
- `import { x } from "m"`: rename by rewriting to `import { x as a } from "m"` when needed.

Exports:

- Exported bindings must not be renamed.
- Re-exports do not introduce local bindings to rename unless the syntax also creates a local alias.


Output Generation
-----------------

- Emit output into one preallocated buffer.
- Start with the minified input length plus one byte for the terminator.
- Add the extra bytes counted for shorthand property expansion and any other guaranteed growth cases.

Emit rules:

- declaration identifiers emit the mangled name when renamed
- reference identifiers emit the mangled name when renamed
- shorthand object references emit expanded `name:name` form when renamed
- all other tokens emit their original source text


Suggested Implementation Order
------------------------------

1. Add the CLI flag and pass wiring, initially with the mangler disabled internally.
2. Add count-pass data structures and counting scan.
3. Add collect-pass structures and basic scope/declaration collection.
4. Support plain declarations and references without destructuring first.
5. Implement name resolution.
6. Implement output emission for ordinary renamed identifiers.
7. Add shorthand object-property expansion.
8. Add destructuring support.
9. Add import handling.
10. Add export detection and rename blocking.
11. Add conservative `eval` and `with` disabling.
12. Refine reuse of local names per function tree.


Validation Plan
---------------

- Before adding new unit tests, ask first.
- Existing JavaScript tests must still pass after integration.
- New tests, once approved, should cover:
  - local renaming
  - nested shadowing
  - `var` hoisting behavior
  - parameter renaming
  - named function expressions
  - catch bindings
  - shorthand object-property expansion
  - destructuring
  - classic-script top-level non-renaming
  - module top-level local renaming
  - import alias rewriting
  - export non-renaming
  - `eval` disabling mangling
  - `with` disabling mangling


Notes for Future Extensions
---------------------------

- A future bundling or single-file merge feature would naturally increase the amount of code that becomes local to one input unit.
- With that future feature, the current mangling design should automatically produce better shortening without changing the core rules.
