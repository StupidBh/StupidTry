# Repository Guidelines

## Project Structure & Module Organization

This is a CMake-based C++ project. The root `CMakeLists.txt` configures C++23, C17, shared output directories, and adds two subprojects:

- `Core/`: main executable target. Entry point is `Core/src/Main.cpp`; shared implementation lives under `Core/Common` and `Core/Utils`.
- `ReaderCGNS/`: shared library target for CGNS inspection. Public headers are in `ReaderCGNS/include/ReaderCGNS`, implementation is in `ReaderCGNS/src`, and local helpers are in `ReaderCGNS/Utils`.
- `Utils/` and `Logger/`: header-only utility and logging helpers used across targets.
- `3rdparty/`, `Core/3rdparty/`, `ReaderCGNS/3rdparty/`: vendored dependencies. Avoid editing these unless updating a dependency intentionally.

Build products are written to `bin/<Debug|Release>/`; generated CMake files belong in `build/` or `cmake-build-*`.

## Build, Test, and Development Commands

- Visual Studio 18 2026 is the preferred generator. Look for the VS 18 installation and its bundled CMake, MSBuild, and LLVM tools before falling back to older Visual Studio installations or system-wide tools.
- `cmake -S . -B build/Debug -G "Visual Studio 18 2026" -A x64`: configure the Visual Studio 18 build tree.
- `cmake --build build/Debug --config Debug`: compile `Core` and `ReaderCGNS`.
- `cmake --build build/Debug --config Release`: compile Release artifacts from the same multi-config build tree.

Dependencies are vendored, so do not add package-manager downloads to normal build steps without documenting the change.

Keep `README.md` synchronized with project-level changes. Updates to dependencies, toolchain requirements, CMake configuration, build options, or build/test commands must include the corresponding README changes in the same change set.

## Coding Style & Naming Conventions

Use the checked-in `.clang-format` for C++ formatting: 4-space indentation, no tabs, LF line endings, and a 150-column limit. Format touched files before committing, for example `clang-format -i Core/src/Main.cpp ReaderCGNS/src/ReaderCGNS.cpp`.

Follow existing C++ naming: classes use `PascalCase`, private data members use `m_` prefixes, namespaces are lowercase or project-named (`utils`, `ReaderCGNS`), and macros/constants use uppercase where already established.

The project language standards are C++23 and C17. When modifying project code, prefer modern, standard-library-based implementations available within those language versions; do not introduce C++26 or later language/library requirements without intentionally updating the project standard and documentation.

The project requires CMake 4.0 or newer. When modifying `CMakeLists.txt` files, prefer modern CMake 4.0+ target-based commands and generator expressions over directory-wide or legacy patterns.

## Testing Guidelines

There is currently no first-party test suite in the repository. When adding tests, place them in a clear module-local `tests/` directory, register them with CTest from CMake, and name test files after the behavior under test, such as `ReaderCGNS/tests/CgnsCoreTests.cpp`.

## Commit & Pull Request Guidelines

Recent commits use concise conventional-style messages such as `feat(ReaderCGNS): ...`, `refactor(utils): ...`, `build(deps): ...`, and `docs: ...`. Keep commits scoped and use an imperative summary.

Pull requests should describe the change, list build/test commands run, link related issues, and include screenshots or sample output when behavior visible to users changes.
