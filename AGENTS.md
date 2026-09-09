# Repository Guidelines

## Project Structure & Module Organization

This is a CMake-based C++ project with two build targets:

- `Core/`: main executable. It includes the ReaderCGNS public headers and loads `ReaderCGNS.dll` at runtime without linking its import library.
- `ReaderCGNS/`: loadable CGNS inspection module. Public headers are in `ReaderCGNS/include/ReaderAPI`.
- `Utils/` and `Logger/`: header-only utility and logging helpers used across targets.
- `3rdparty/`, `Core/3rdparty/`, `ReaderCGNS/3rdparty/`: vendored dependencies. Avoid editing these unless updating a dependency intentionally.

Build products are written to `bin/<Debug|Release>/`; generated CMake files belong in `build/` or `cmake-build-*`.

## Branch Responsibilities

- `dev` is the primary development branch. Develop production code, tests, build changes, dependencies, and related documentation here.
- `main` is the user-controlled integration branch. Do not use `main` for routine development. Only the user decides when `dev` is integrated into `main`.

Do not switch branches, create commits, merge, rebase, cherry-pick, or otherwise update branches unless the user's current request explicitly authorizes that action.

## Build, Test, and Development Commands

- Visual Studio 18 2026 is the preferred generator.
- `cmake -S . -B build/Debug -G "Visual Studio 18 2026" -A x64`: configure the Visual Studio 18 build tree.
- `cmake --build build/Debug --config Debug`: compile `Core` and `ReaderCGNS`.
- `cmake --build build/Debug --config Release`: compile Release artifacts from the same multi-config build tree.
- A target-only `Core` build does not build `ReaderCGNS.dll`; build the default all target, or explicitly build both `ReaderCGNS` and `Core`, before running the executable.
- After tests are registered, `ctest --test-dir build/Debug -C Debug`: run the Debug test suite.
- After tests are registered, `ctest --test-dir build/Debug -C Release`: run the Release test suite.

Required dependencies are vendored. Do not add package-manager downloads to normal build steps without documenting the change.

Update the relevant README when changing dependencies, build requirements, configuration, or user-facing commands.

## Coding Style & Naming Conventions

Format touched C and C++ files with the checked-in `.clang-format` before committing.

Follow existing C++ naming: classes use `PascalCase`, private data members use `m_` prefixes, namespaces are lowercase or API-named (`utils`, `ReaderAPI`), and macros/constants use uppercase where already established.

The project language standard is C++23. When modifying project code, prefer modern, standard-library-based implementations available within that language version; do not introduce C++26 or later language/library requirements without intentionally updating the project standard and documentation.

The project requires CMake 4.3 or newer. When modifying `CMakeLists.txt` files, prefer modern CMake 4.3+ target-based commands and generator expressions over directory-wide or legacy patterns.

## Testing Guidelines

Develop first-party tests on `dev`, keep them in module-local `tests/` directories, and register them with CTest. Name test files after the behavior under test, such as `ReaderCGNS/tests/CgnsCoreTests.cpp`.

## Commit Guidelines

Recent commits use concise conventional-style messages such as `feat(ReaderCGNS): ...`, `refactor(utils): ...`, `build(deps): ...`, and `docs: ...`. Keep commits scoped and use an imperative summary.

Each commit should contain one coherent behavior. A production change and its corresponding tests may be committed together. Keep dependency updates, repository guidance, and unrelated formatting in separate commits.
