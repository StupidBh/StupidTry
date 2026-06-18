# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Configure a Debug build (Ninja)
cmake -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build the executable
cmake --build cmake-build-debug

# Configure a Release build
cmake -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build Release
cmake --build cmake-build-release
```

The project root `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 23` and only adds the `Core/` subdirectory. Build artifacts go to `bin/<Debug|Release>/Core.exe`.

`Core/CMakeLists.txt` auto-collects `.cpp` files from `Core/Common/src/` and `Core/Utils/src/` via `file(GLOB ...)`, so adding a new Core source file usually means placing it under one of those directories.

## Run

```bash
# Show CLI help
./bin/Debug/Core.exe --help

# Run with explicit work directory
./bin/Debug/Core.exe --inputPath <path-to-input-file> --workDirectory .

# Run and let workDirectory auto-derive from inputPath
./bin/Debug/Core.exe --inputPath <path-to-input-file>
```

CLI contract:
- `--help/-h`
- `--inputPath/-i` (required)
- `--workDirectory/-w` (optional)
- `--DEBUG` (bool switch; forced on in Debug builds)

If `--workDirectory` is omitted, `SingletonData` derives it from `inputPath`'s parent directory, or falls back to `./<input-stem>/` when the input has no parent component.

## Tests

There is no first-party test target wired into CMake right now: no `enable_testing()`, no `add_test()`, and no dedicated `tests/` directory. Validation is currently manual:

```bash
cmake --build cmake-build-debug
./bin/Debug/Core.exe --inputPath <path-to-input-file>
```

There is no single-test command because there are no registered CTest targets yet.

## Formatting

Formatting is driven by `.clang-format`:
- WebKit-based style
- 120-column limit
- 4-space indentation
- `#pragma once` headers
- includes are preserved (`SortIncludes: Never`)
- function / struct braces break onto their own lines

## Architecture

### Entry point

`Core/src/Main.cpp` is a thin bootstrap:
1. parse CLI arguments through `SINGLE_DATA.ProcessArguments(argc, argv)`
2. exit early if argument parsing produced an empty `variables_map` (help or parse failure)
3. best-effort CGNS inspection via `CgnsCore`
4. run a HighFive HDF5 write/read round-trip into `WORK_DIR_PATH / "Try1.h5"`
5. run `CallCmd("ipconfig")`
6. call `spdlog::shutdown()` before exit

Notably, CGNS open failure no longer aborts the whole process; the HDF5 demo path still runs.

### Layer map

```text
Core/src/Main.cpp          thin entry point and local demo flow
Core/Common/               process-level bootstrap and Win32 helpers
  SingletonData.*            CLI parsing, work dir derivation, logger bootstrap
  Functions.*                string/path/process helpers, SCOPED_TIMER macros, subprocess execution
Core/Utils/                domain and file-format utilities
  CgnsCore.*                 RAII wrapper over the CGNS mid-level API
  MioReader.*                mmap-backed text ingestion
  HighFiveUtils.hpp          HDF5 dataset/group helper templates
Utils/                     reusable header-only infrastructure, not all linked into Core today
Logger/                    header-only async spdlog singleton wrapper
3rdparty/                  vendored Boost / HDF5 / CGNS / HighFive / mio / spdlog / TBB
```

### Key patterns

**Singleton application context**  
`SingletonData` is the runtime context. The macros `SINGLE_DATA`, `SINGLE_DATA_VM`, `INPUT_PATH`, `WORK_DIR`, `WORK_DIR_PATH`, and `IS_DEBUG` are the normal access path from the rest of the app. `WORK_DIR_PATH` returns a `std::filesystem::path` and creates the directory on demand.

**Logger bootstrap order matters**  
`Logger::InitLog(work_dir, "stupid-bhh", verbose)` is called from `SingletonData::ProcessArguments()`. Anything using `LOG_*` before argument parsing risks hitting an uninitialized default logger.

**Windows-specific utilities are centralized**  
`Core/Common/src/Functions.cpp` contains the Win32 boundary: ACP/UTF-8 conversion, environment lookups, executable path helpers, and `CallCmd`.

`CallCmd` launches a child process with stdout/stderr redirected into a pipe, streams output line-by-line to a callback, and treats a callback return of `true` as an early-stop request. Early stop is cooperative first (close the read pipe so later writes usually hit `ERROR_BROKEN_PIPE`), then forced via a Job Object timeout path so the whole spawned process tree is cleaned up if the child does not exit promptly.

**CGNS inspection flow**  
`CgnsCore` is a thin RAII wrapper around `cgnslib.h`. `OpenCGNS()` validates with `cg_is_cgns`, opens read-only, logs version/precision, and resets `m_cg_file_id` on failure so `IsOpen()` and destruction never see a stale handle.

`CgnsCore::info()` is a read-only traversal/reporting pass. It walks:
- bases
- zones (structured and unstructured)
- flow solutions
- discrete data
- zone subregions
- grid entries and coordinate arrays
- element connectivity sections

Every CGNS API call is expected to go through `CG_INFO(status)` so failures log `cg_get_error()` consistently.

**HighFive helpers**  
`Core/Utils/HighFiveUtils.hpp` provides the HDF5 convenience layer used by `Main.cpp`:
- `WriteDataSet` overloads for vectors/arrays/spans
- compound-type dataset writes
- `WriteDataSet2` for extendable chunked datasets
- `GetGroup` for create-or-open group access

**Optional TBB-backed parallelism**  
`Utils/SmartPrefixSum.hpp` uses `std::execution::par` for large-range reductions. `Core/CMakeLists.txt` links `TBB::tbb` only when `find_package(TBB CONFIG QUIET)` succeeds; otherwise the code still builds, but parallel STL execution silently falls back to serial behavior on this toolchain.

## Platform and dependencies

This is a Windows-only codebase using Win32 APIs (`CreateProcessW`, `CreatePipe`, `MultiByteToWideChar`, `GetModuleFileNameW`, etc.).

Important dependency facts:
- Boost.ProgramOptions / Boost.Container are vendored and linked statically
- CGNS is linked via `CGNS::cgns_static`
- HDF5 is linked via `hdf5::hdf5-static`
- HighFive, mio, and spdlog are header-only in this repo
- TBB is optional and only affects whether parallel STL work is truly parallel

## Include style

- third-party headers: `#include <...>`
- project headers: `#include "Common/Functions.h"`, `#include "Utils/Utils.hpp"`
- repo root, `Core/Common`, `Core/Utils`, and `3rdparty/` are all on the include path for the `Core` target
