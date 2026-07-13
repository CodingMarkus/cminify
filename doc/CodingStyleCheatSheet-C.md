# C Coding Style Cheat Sheet

## 1. General Rules

- Comments, function names, variable names in English.
- Tabs for indentation (tab = 4 spaces). Spaces for alignment only.

## 2. Includes and Header Structure
- Use "#pragma once".
- Project headers → external headers → system headers.
- Blank line between groups.
- Same spacing rules in headers and implementations.
- Two blank lines between different kinds of sections such as includes,
  type definitions, and function declarations or definitions.

## 3. Line Breaking
- Max 80 characters (line break counts, so max 79 code chars).
- Indent continuation lines when breaking an expression.
- Do not indent again when breaking the same expression multiple times unless breaking a sub-expression.
- Break before operators (+, -, *, /, %, &&, ||, &, |, ^, <<, >>).
- Break after assignment (=).
- Break before comparison operators (==, !=, <, >).
- Break before reference operators (. and ->).
- Break directly after (, [, {.
- Place closing ), ], } on their own unindented line, unless that line starts a new {-block.

## 4. Preprocessor Macros
- Directives start at column 0.
- Indent nested #if/#ifdef like code.
- Function-like macros use function spacing.
- Align backslashes, add at least one space before each, ignore last line.

## 5. Types, Variables, and Constants
- const instead of #define for true constants (file scope only).
- Constants ALL_CAPS, variables camelCase.
- If the same literal value is used in more than one place, define a named
  constant for it instead of repeating the literal.
- Use size_t for array indices and counters.
- Avoid signed unless needed.
- char only for characters, never for byte-sized ints.
- Make no assumption about char being signed or unsigned.
- Enums/structs start uppercase, { on same line, one field per line.
- Enum values suffixed with enum name, separated by underscore.
- One blank line between adjacent multi-line struct definitions.
- Adjacent single-line struct definitions need no blank line.
- Do not typedef all structures and enums.
- typedef only opaque types and enums used as options.
- One blank line between adjacent multi-line typedefs.
- Adjacent single-line typedefs need no blank line.

## 6. Pointers and Arrays
- Space around * in declarations, not in dereference.
- Function pointers must use & when assigned.
- Arrays: space inside [] and {} when size omitted or initializer used.
- Parameters: “int * a” is pointer; “int a[]” is array pointer.

## 7. Functions
- External linkage: Uppercase. File-local: lowercase.
- No space between call name and ( ).
- Space inside ( ) in declarations/definitions.
- One blank line between adjacent function declarations in both header and
  implementation files.
- Two blank lines between groups of function declarations.
- Break declarations/definitions/calls along their parameters.
- Place up to 3 parameters per line for at most 2 lines.
- Place one parameter per line if more than 2 lines are required.
- { on own line in definitions.
- Attributes on line above function.
- Two blank lines after function, three between groups.
- One blank line between instruction groups inside functions.

## 8. Control Flow
- { on same line as if/for/while.
- No braces for single-statement branches (unless broken).
- Use braces for multi-statement branches.
- Multi-line conditions: ) on last line, { on next line.
- In do-while loops, break the while keyword as you would break a function call.
- One line may contain two statements only if second is control flow.
- Prefer early returns.
- goto only for cleanup.
- switch: indent case and body. Use { } unless simple single-line.
- Each case ends with break/return/goto.
- Fallthrough requires comment or the use of a fallthrough statement.
- return only one value; if expression, wrap in ( ).

## 9. Expressions and Operators
- Parentheses around ==, <, >, <=, >= if part of larger expression.
- Add parentheses for clarity even if not required.
- Ternary: Use ?: if possible. Break after ? and before : if needed.
- Parenthesize ternary if inside larger expression.

## 10. Strings
- Break strings before line break, space at end of line.

## 11. Comments
- Use //, /* ... */ only mid-line.
- Prefer comments above a line if referring to the line as a whole.
- Prefer end-of-line comments if explaining an assigned value.
- Documentation: /** ... */ before functions, indented, no * prefix per line.
- Normal comments: do not need full sentences.
- A single-sentence normal comment omits final punctuation.
- Multi-sentence normal comments end each sentence with punctuation.
- Doc comments: must be full sentences.

## 12. Consistency and Readability Win
- Keep similar adjacent blocks visually consistent, even if that slightly breaks the rules.
