# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Configure (Ninja generator)
cmake -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build cmake-build-debug
```

CMake 4.0+, C++20 required. The root `CMakeLists.txt` sets up third-party paths and `add_subdirectory(Core)`. The build output is `bin/<Debug|Release>/Core.exe`. HDF5 and CGNS are linked statically (`hdf5::hdf5-static`, `CGNS::cgns_static`); Boost is the only runtime DLL dependency. The `POST_BUILD` step in `Core/CMakeLists.txt` auto-copies `$<TARGET_RUNTIME_DLLS:Core>` (the Boost DLLs) next to the executable, so the binary is self-contained after a build.

## Run

```bash
# Show CLI help
./bin/Debug/Core.exe --help

# Run the app (inputPath is required)
./bin/Debug/Core.exe --inputPath <path-to-input-file> --workDirectory .
```

The CLI currently exposes `--help/-h`, `--inputPath/-i` (required), `--workDirectory/-w` (default `.`), and `--DEBUG`. In Debug builds, `--DEBUG` is forced on in `SingletonData::ProcessArguments()`.

## Tests

There is no first-party test target wired into CMake right now: the repo does not define `enable_testing()`, `add_test()`, or a dedicated `tests/` directory. Validation is currently by building the `Core` executable and running it manually against sample inputs.

## Formatting

`.clang-format` — WebKit-based, 120-column, 4-space indent, C++20, braces on own line for functions/structs/namespaces. Use clang-format 20.0.0.

## Code Conventions

- **Header guards**: `#pragma once` throughout (no `#ifndef` guards)
- **Member variables**: `m_` prefix (e.g., `m_vm`, `m_mmap`, `m_is_ready`)
- **Comments**: Code comments use a mix of English and Chinese; utility-level files tend toward English, business-logic files toward Chinese
- **Namespaces**: `utils::` (generic utilities), `dylog::` (logger), `stupid::` (SmartPrefixSum)
- **`this->`**: Used consistently when accessing members in templates and some classes

## Architecture

### Entry Point

`Core/src/Main.cpp` — creates a `SCOPED_TIMER`, calls `SINGLE_DATA.ProcessArguments(argc, argv)`, exits if `variables_map` is empty (help or parse error), then constructs a `CgnsCore` from `INPUT_PATH`, opens the CGNS file, and dumps its structure via `cgns.info()`.

### Layer Map

```
Core/src/Main.cpp          ← thin entry point; parse args, init logger, then hand off to app logic
Core/Common/               ← application boundary and process-level helpers
    SingletonData.h/.cpp      owns Boost.ProgramOptions state, CLI contract, work dir, logger bootstrapping
    Functions.h/.cpp          shared Win32/string/process utilities and SCOPED_TIMER macros
Core/Utils/
    MioReader.h/.cpp          mmap-backed sequential/batch line reader plus numeric parsing helpers
    CgnsCore.h/.cpp           thin RAII wrapper over the CGNS mid-level (cgnslib) API; opens a file and walks base→zone→section→solution
Utils/                      ← repo-level header-only building blocks, reusable outside Core
    SingletonHolder.hpp      CRTP singleton base + helper macros
    ScopedTimer.hpp          RAII elapsed-time reporter used by SCOPED_TIMER macros
    Utils.hpp                generic vector/type-trait/math helpers
    ThreadPool.hpp           jthread-based pool (currently not linked into Core)
    SyncController.hpp       producer/consumer coordination primitive (currently not linked into Core)
    SmartPrefixSum.hpp       cached prefix-sum helper with incremental recompute (currently not linked into Core)
Logger/                     ← header-only async spdlog wrapper singleton (logger.hpp + logger_formatter.hpp)
3rdparty/                   ← vendored headers/libs (Boost, HDF5, HighFive, CGNS, mio, spdlog)
```

### Key Patterns

**Bootstrap flow**: `main()` is intentionally thin: it starts a process-wide `SCOPED_TIMER`, calls `SINGLE_DATA.ProcessArguments(argc, argv)`, and exits early when argument parsing produced an empty `variables_map` (help or parse failure). Business logic hangs off this bootstrap path — today that is the `CgnsCore` open-and-inspect sequence.

**Singleton**: `utils::SingletonHolder<T>` (CRTP, `Utils/SingletonHolder.hpp`) is the project-wide singleton pattern. Classes inherit from `SingletonHolder<Self>` with `SINGLETON_CLASS(Self)` (`DELETE_COPY_AND_MOVE` + friend declaration) and are accessed via `T::get_instance()`. Current concrete singletons are `SingletonData` and `dylog::Logger`.

**SingletonData as the application context** (`Core/Common/SingletonData.h`, `Core/Common/src/SingletonData.cpp`): wraps Boost `variables_map`, owns the CLI contract, forces verbose mode in Debug, and initializes the global logger. The convenience macros `SINGLE_DATA`, `SINGLE_DATA_VM`, `INPUT_PATH`, `WORK_DIR`, and `IS_DEBUG` are the expected way to access parsed runtime configuration from the rest of the app.

**Logging initialization order matters**: `Logger::InitLog(work_dir, "stupid-bhh", verbose)` is called from `SingletonData::ProcessArguments()`. Anything that uses `LOG_*` before argument parsing/logger bootstrap risks logging through an uninitialized default logger. The logger writes both to the console and to `work_dir/logs/` via async spdlog sinks.

**MioReader for large-file ingestion** (`Core/Utils/MioReader.h`, `Core/Utils/src/MioReader.cpp`): the codebase’s main text-file data-access primitive is memory-mapped file reading via `mio::mmap_source`. `GetLine()` provides sequential single-line iteration; `GetLineBatch()` is the bulk path, scanning with `memchr` to amortize work across many lines. `parse_line<T>()` is paired with it for extracting numeric values, including scientific notation.

**CgnsCore for CGNS mesh/solution files** (`Core/Utils/CgnsCore.h`, `Core/Utils/src/CgnsCore.cpp`): RAII wrapper over the CGNS mid-level library (`cgnslib.h`). The constructor stores the path; `OpenCGNS()` validates via `cg_is_cgns`, opens read-only, and logs version/precision; the destructor closes any open file id. `info()` walks the CGNS hierarchy (base → zone → section → flow solution), branching on structured vs. unstructured zones. Every CGNS call is wrapped in the static `CG_INFO(status)` helper, which logs `cg_get_error()` on non-`CG_OK` returns — follow that pattern for new CGNS calls. The `*Name` lookup arrays (`SimulationTypeName`, `ZoneTypeName`, `ElementTypeName`, etc.) used for pretty-printing come from `3rdparty/cgns/include/cgnslib.h`.

**Windows utility boundary** (`Core/Common/Functions.h`, `Core/Common/src/Functions.cpp`): Win32-specific concerns are concentrated here — GBK/UTF-8 conversion, subprocess execution with redirected pipes (`CallCmd`), environment variable lookup, and case-insensitive/trim helpers. Keep platform-specific code in this layer instead of spreading raw Win32 calls through app logic.

**Repo-level utilities are broader than the current executable**: `Utils/ThreadPool.hpp`, `Utils/SyncController.hpp`, and `Utils/SmartPrefixSum.hpp` are reusable C++20 helpers present in the repo but not currently wired into the `Core` target. They are library-style infrastructure, not part of today’s executable path.

### Platform

Windows-only. Code uses `windows.h`, Win32 API (`CreateProcessW`, `MultiByteToWideChar`, `CreatePipe`, `SetConsoleOutputCP`, etc.), and links against GCC 13-built Boost DLLs.

### Third-party Libraries

| Library | Usage | Notes |
|---------|-------|-------|
| Boost.ProgramOptions 1.91 | CLI argument parsing in SingletonData | DLL, debug/release variants |
| Boost.Container 1.91 | (dependency of ProgramOptions) | DLL |
| CGNS | Mesh/solution file reading in CgnsCore | Static lib, linked via `CGNS::cgns_static`; built on HDF5 |
| HDF5 1.14 | CGNS storage backend (CGNS HDF5 files) | Static lib, linked via `hdf5::hdf5-static`; ZLIB resolved from `3rdparty/hdf5/.../libzs[d].a` |
| HighFive | HDF5 C++ wrapper (header-only) | Not yet used |
| mio | Memory-mapped file I/O used by MioReader | Header-only |
| spdlog | Async logging in Logger | Header-only, async mode with thread pool |

### Adding a New Source File

Source files under `Core/Common/src/` and `Core/Utils/src/` are auto-collected via `file(GLOB CPP_RESOURCES ...)` in `Core/CMakeLists.txt`. Headers are found via `target_include_directories` which exposes `Core/Common/`, `Core/Utils/`, the repo root, and `3rdparty/`. To add a new `.cpp` to the Core target, place it under one of those `src/` directories (or update the GLOB in `Core/CMakeLists.txt`).

### Header Include Style

- Third-party headers (HDF5, Boost, mio): `#include <...>` — resolved via `target_include_directories` pointing to `3rdparty/`
- Project headers: `#include "Utils/Utils.hpp"`, `#include "Common/Functions.h"` — repo root is in the include path
- spdlog: `#include "spdlog/spdlog.h"` — finds it under `3rdparty/spdlog/`
