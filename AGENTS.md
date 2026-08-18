# Repository Guidelines

## Project Structure & Module Organization

This is a CMake-based C++ project. The root `CMakeLists.txt` configures C++23, C17, shared output directories, and adds two subprojects:

- `Core/`: main executable target. Entry point is `Core/src/Main.cpp`; shared implementation lives under `Core/Common` and `Core/Utils`.
- `ReaderCGNS/`: shared library target for CGNS inspection. Public headers are in `ReaderCGNS/include/ReaderCGNS`, implementation is in `ReaderCGNS/src`, and local helpers are in `ReaderCGNS/Utils`.
- `Utils/` and `Logger/`: header-only utility and logging helpers used across targets.
- `3rdparty/`, `Core/3rdparty/`, `ReaderCGNS/3rdparty/`: vendored dependencies. Avoid editing these unless updating a dependency intentionally.

Build products are written to `bin/<Debug|Release>/`; generated CMake files belong in `build/` or `cmake-build-*`.

## Branch Responsibilities

- `dev-ai` is the primary production-development branch. Author and commit new features, production behavior, bug fixes, refactors, production build changes, dependency updates, and their user-facing documentation here. Do not commit first-party tests or test-only changes to `dev-ai`.
- `test` is the primary test-development branch. Direct commits here should focus on test sources, CTest wiring, fixtures, test scripts, and test-only documentation. Bring production changes in from `dev-ai`; do not independently develop or duplicate production behavior on `test`.
- `main` is the user-controlled integration branch and, when updated, covers the complete repository: production content from `dev-ai` plus test content from `test`. Do not use `main` as the primary branch for routine production or test development. Only the user decides when `test` is merged into `main`.

Route each change to its owning branch before committing. The normal agent-controlled flow ends after integrating `dev-ai` into `test`, then returning to `dev-ai`. Agents MUST NOT automatically switch to, merge into, rebase, cherry-pick into, or otherwise update `main`. Stop on `dev-ai` and report that main integration is pending. Update `main` only when the user's current request explicitly instructs that merge.

## Build, Test, and Development Commands

- Visual Studio 18 2026 is the preferred generator. Look for the VS 18 installation and its bundled CMake, MSBuild, and LLVM tools before falling back to older Visual Studio installations or system-wide tools.
- `cmake -S . -B build/Debug -G "Visual Studio 18 2026" -A x64`: configure the Visual Studio 18 build tree.
- `cmake --build build/Debug --config Debug`: compile `Core` and `ReaderCGNS`.
- `cmake --build build/Debug --config Release`: compile Release artifacts from the same multi-config build tree.
- On the `test` or `main` branch, `ctest --test-dir build/Debug -C Debug`: run the Debug test suite.
- On the `test` or `main` branch, `ctest --test-dir build/Debug -C Release`: run the Release test suite.

Dependencies are vendored, so do not add package-manager downloads to normal build steps without documenting the change.

Keep `README.md` synchronized with project-level changes. Updates to dependencies, toolchain requirements, CMake configuration, build options, or build/test commands must include the corresponding README changes in the same change set.

## Coding Style & Naming Conventions

Use the checked-in `.clang-format` for C++ formatting: 4-space indentation, no tabs, LF line endings, and a 150-column limit. Format touched files before committing, for example `clang-format -i Core/src/Main.cpp ReaderCGNS/src/ReaderCGNS.cpp`.

Follow existing C++ naming: classes use `PascalCase`, private data members use `m_` prefixes, namespaces are lowercase or project-named (`utils`, `ReaderCGNS`), and macros/constants use uppercase where already established.

The project language standards are C++23 and C17. When modifying project code, prefer modern, standard-library-based implementations available within those language versions; do not introduce C++26 or later language/library requirements without intentionally updating the project standard and documentation.

The project requires CMake 4.0 or newer. When modifying `CMakeLists.txt` files, prefer modern CMake 4.0+ target-based commands and generator expressions over directory-wide or legacy patterns.

## Testing Guidelines

Author and directly commit first-party tests on the local `test` branch, then integrate them into `main`. Keep test sources in module-local `tests/` directories and register them with CTest from CMake. Name test files after the behavior under test, such as `ReaderCGNS/tests/CgnsCoreTests.cpp`.

The `dev-ai` branch contains production code and must not contain first-party test sources, test-only fixtures or scripts, or CTest registration. Treat documentation and build logic used exclusively to run tests as test content as well.

## Commit & Pull Request Guidelines

Recent commits use concise conventional-style messages such as `feat(ReaderCGNS): ...`, `refactor(utils): ...`, `build(deps): ...`, and `docs: ...`. Keep commits scoped and use an imperative summary.

Before every commit, classify changes by function at hunk level and commit the smallest coherent behavior. Do not combine unrelated production changes, repository guidance, dependency updates, formatting, or tests in one commit.

When a worktree contains both production and test changes, commit the production changes to `dev-ai` first. Then bring those production commits into the local `test` branch and commit test sources, CTest wiring, fixtures, scripts, and test-only documentation there. After `test` is complete, return to `dev-ai` and stop unless the user explicitly requests main integration in the current request. Never merge or cherry-pick test-only commits back into `dev-ai`; production flows from `dev-ai` to `test`, while the combined result flows from `test` to `main` only under user control.

Pull requests should describe the change, list build/test commands run, link related issues, and include screenshots or sample output when behavior visible to users changes.
