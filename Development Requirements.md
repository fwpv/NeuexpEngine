# Development Requirements

- Implementation language: C++17.
- Graphics API: OpenGL 1.1.
- Target platforms: Windows and Linux.
- Build system: CMake.
- Develop the engine through small, incremental changes.
- Write clear, readable code; avoid unnecessarily complex solutions.
- Verify new functionality after adding it.
- Preserve existing working examples and verify them after making changes.
- Use Git to commit changes and revert to previous working states.
- Every task must leave the project in a state no worse than before, preserving existing functionality, buildability, code quality, readability, and architectural consistency.
- Avoid unrelated refactoring. Make only the changes necessary to complete the current task, unless additional changes are explicitly requested or required for correctness.

## Git workflow

- Before starting each new development stage, create a Git branch named `subsystem_nn`, where `nn` is the two-digit subsystem/stage number from `road map.md` (for example, `subsystem_00`, `subsystem_01`). Perform the stage's development and commits on that branch.
- Push development changes from the PC only to the corresponding `subsystem_nn` branch. Never push directly to `main` from the PC, including force pushes.
- Integrate changes into `main` only through Contribute in the GitHub web interface, by opening and merging a pull request from the development branch.
- These workflow requirements apply to both the developer and AI agents, even when GitHub does not enforce repository rulesets.

## Code style

- Apply one consistent style to all application-owned C++ code, including both platform implementations. Preserve the exact spelling and required signatures of external APIs, system types, macros, constants, and callbacks (Win32, X11, GLX, OpenGL, standard library).
- Use four spaces for indentation; do not use tabs. Put opening braces of namespaces, functions, classes, and structs on a new line. Keep opening braces of if, else, for, while, and switch blocks on the same line as the statement. Always use braces for control-flow bodies.
- Use `camelCase` for application functions, variables, fields, and parameters (`processEvents`, `openGLContext`); use `PascalCase` for application types, enums, enum values, and named constants (`State`, `Key::Fire`, `ReferenceScreenWidth`). Keep external identifiers unchanged.
- Use meaningful English identifiers. Keep OS-specific names and types inside their platform implementation; expose OS-independent operations through `platform.h` and the `Platform` namespace.
- Use spaces around binary operators and after commas; no space between a function name and `(`. Keep related declarations and statements together and separate logical sections with blank lines.
- Use `const` for values that do not change, `const T&` for read-only larger objects, `nullptr` for null pointers, and explicit casts (`static_cast`) when conversion is needed.
- Use `#pragma once` in project headers. Include project headers with quotes and system/library headers with angle brackets. Prefer explicit includes for direct dependencies.
- Write comments in Russian. Treat the project as educational: explain unfamiliar APIs, including the purpose of functions, parameters, return values, and important usage details. Comments should help the reader learn the API directly from the source code. Avoid redundant comments about obvious application logic.
- Avoid unexplained magic numbers: give meaningful names to repeated or domain-significant values. Group related visual constants and configuration values by purpose.
- Format constructor initializer lists with the colon on a new line, followed by the first initializer. Place each subsequent initializer on a separate line, with the comma at the beginning. Align all initializers vertically.
- Do not indent declarations inside namespaces.
