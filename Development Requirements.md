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

- Write commit messages in the past tense, describing completed changes. Example: "Упорядочена структура по roadmap и обновлены пути сборки".

- Before starting each new development stage, create a Git branch named `subsystem_nn`, where `nn` is the two-digit subsystem/stage number from `road map.md` (for example, `subsystem_00`, `subsystem_01`). Perform the stage's development and commits on that branch.
- Push development changes from the PC only to the corresponding `subsystem_nn` branch. Never push directly to `main` from the PC, including force pushes.
- Integrate changes into `main` only through Contribute in the GitHub web interface, by opening and merging a pull request from the development branch.
- These workflow requirements apply to both the developer and AI agents, even when GitHub does not enforce repository rulesets.

## Code style

- Return operation errors through output reference parameters, not through return values. Return values may indicate success/failure or carry the operation's normal result. Use a single error object or a collection of errors according to the operation's needs.
- Prefer modern C++17 language features and standard-library facilities where practical. Use standard string types for text and standard streams for console output. Avoid arbitrary fixed-size character buffers when the required size can be determined dynamically. Preserve native API signatures and use their required allocation/deallocation functions at the platform boundary. Do not require a newer C++ standard or refactor unrelated code solely for style.
- Keep resource management simple and proportional to the task. Use RAII and smart pointers where they provide a concrete benefit; do not introduce wrappers or custom deleters solely to use modern syntax. Explicit allocation/deallocation pairs are acceptable in short, straightforward native API code with a clear resource lifetime.
- Apply one consistent style to all application-owned C++ code, including both platform implementations. Preserve the exact spelling and required signatures of external APIs, system types, macros, constants, and callbacks (Win32, X11, GLX, OpenGL, standard library).
- Use four spaces for indentation; do not use tabs. Put opening braces of namespaces, functions, classes, and structs on a new line. Keep opening braces of if, else, for, while, and switch blocks on the same line as the statement. Always use braces for control-flow bodies.
- Use `camelCase` for application functions, variables, fields, and parameters; use `PascalCase` for application types, enums, enum values, and named constants. Keep external identifiers unchanged.
- Use meaningful English identifiers. Keep OS-specific names and types inside their platform implementation; expose OS-independent operations through a common platform interface.
- Use spaces around binary operators and after commas; no space between a function name and `(`. Keep related declarations and statements together and separate logical sections with blank lines.
- Use `const` for values that do not change, `const T&` for read-only larger objects, `nullptr` for null pointers, and explicit casts (`static_cast`) when conversion is needed.
- Use `#pragma once` in project headers. Include project headers with quotes and system/library headers with angle brackets. Prefer explicit includes for direct dependencies.
- Write comments in Russian. Treat the project as educational: explain unfamiliar APIs, including the purpose of functions, parameters, return values, and important usage details. Comments should help the reader learn the API directly from the source code. Avoid redundant comments about obvious application logic.
- Avoid unexplained magic numbers: give meaningful names to repeated or domain-significant values. Group related visual constants and configuration values by purpose.
- Format constructor initializer lists with the colon on a new line, followed by the first initializer. Place each subsequent initializer on a separate line, with the comma at the beginning. Align all initializers vertically.
- Do not indent declarations inside namespaces.
