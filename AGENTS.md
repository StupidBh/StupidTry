# Repository Guidelines

## Project Structure & Module Organization

This is a CMake-based C++ project. The root `CMakeLists.txt` configures C++23, C17, shared output directories, and adds two subprojects:

- `Core/`: main executable target. Entry point is `Core/src/Main.cpp`; shared implementation lives under `Core/Common` and `Core/Utils`.
- `ReaderCGNS/`: shared library target for CGNS inspection. Public headers are in `ReaderCGNS/include/ReaderCGNS`, implementation is in `ReaderCGNS/src`, and local helpers are in `ReaderCGNS/Utils`.
- `Utils/` and `Logger/`: header-only utility and logging helpers used across targets.
- `3rdparty/`, `Core/3rdparty/`, `ReaderCGNS/3rdparty/`: vendored dependencies. Avoid editing these unless updating a dependency intentionally.

Build products are written to `bin/<Debug|Release>/`; generated CMake files belong in `build/` or `cmake-build-*`.

## Build, Test, and Development Commands

- `cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug`: configure a Debug build.
- `cmake --build build/Debug --config Debug`: compile `Core` and `ReaderCGNS`.
- `cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release`: configure an optimized build.
- `cmake --build build/Release --config Release`: compile Release artifacts.
- `ctest --test-dir build/Debug -C Debug`: run tests if CTest tests are added.

Dependencies are vendored, so do not add package-manager downloads to normal build steps without documenting the change.

## Coding Style & Naming Conventions

Use the checked-in `.clang-format` for C++ formatting: 4-space indentation, no tabs, LF line endings, and a 150-column limit. Format touched files before committing, for example `clang-format -i Core/src/Main.cpp ReaderCGNS/src/ReaderCGNS.cpp`.

Follow existing C++ naming: classes use `PascalCase`, private data members use `m_` prefixes, namespaces are lowercase or project-named (`utils`, `ReaderCGNS`), and macros/constants use uppercase where already established.

## Testing Guidelines

There is currently no first-party test suite in the repository. When adding tests, place them in a clear module-local `tests/` directory, register them with CTest from CMake, and name test files after the behavior under test, such as `ReaderCGNS/tests/CgnsCoreTests.cpp`.

## Commit & Pull Request Guidelines

Recent commits use concise conventional-style messages such as `feat(ReaderCGNS): ...`, `refactor(utils): ...`, `build(deps): ...`, and `docs: ...`. Keep commits scoped and use an imperative summary.

Pull requests should describe the change, list build/test commands run, link related issues, and include screenshots or sample output when behavior visible to users changes.
