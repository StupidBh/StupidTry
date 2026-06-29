# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Configure a Debug build
cmake -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build all targets
cmake --build cmake-build-debug

# Build individual targets
cmake --build cmake-build-debug --target Core
cmake --build cmake-build-debug --target ReaderCGNS

# Configure and build Release
cmake -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
```

The root `CMakeLists.txt` requires CMake 4.0+, sets C to C17 and C++ to C++23, writes runtime/library/archive outputs under `bin/<config>/`, and adds `ReaderCGNS/` before `Core/` because the executable links the DLL target. The README still lists C++20, but the checked-in CMake file is the active build configuration.

`Core/CMakeLists.txt` builds the `Core` executable from `Core/src/Main.cpp` plus `Core/Common/src/*.cpp` and `Core/Utils/src/*.cpp`. `ReaderCGNS/CMakeLists.txt` builds the `ReaderCGNS` shared library from `ReaderCGNS/src/*.cpp` and `ReaderCGNS/Utils/src/*.cpp`; adding compiled DLL code normally means placing the implementation in one of those globbed directories or updating the CMake glob.

## Run

```bash
# Show CLI help
./bin/Debug/Core.exe --help

# Run with an explicit work directory
./bin/Debug/Core.exe --inputPath <path-to-input-file> --workDirectory .

# Run and let workDirectory derive from inputPath
./bin/Debug/Core.exe --inputPath <path-to-input-file>
```

CLI parsing is implemented in `SingletonData::ProcessArguments()`:
- `--help/-h`
- `--inputPath/-i` (required unless showing help)
- `--workDirectory/-w` (optional)
- `--DEBUG` (bool switch; forced on in Debug builds through `#ifndef NDEBUG`)

If `--workDirectory` is omitted, it is derived from the input path's parent directory, or from `./<input-stem>/` when the input has no parent component. `WORK_DIR_PATH` creates the directory on demand.

## Tests and validation

There is no first-party test target wired into CMake right now: no `enable_testing()`, no `add_test()`, and no dedicated `tests/` directory. There is also no single-test command because there are no registered CTest targets.

Current validation is manual:

```bash
cmake --build cmake-build-debug --target Core
./bin/Debug/Core.exe --inputPath <path-to-input-file>
```

To validate only the DLL target:

```bash
cmake --build cmake-build-debug --target ReaderCGNS
```

The current `Core` entry point requires the input path to exist, registers a `ReaderCGNS` log callback, starts CGNS inspection through the DLL, writes and reads `WORK_DIR_PATH / "Try1.h5"` with HighFive, runs a bounded `BlockingQueue<std::size_t>` producer-consumer demo, joins the CGNS thread, clears the DLL callback, then shuts down spdlog.

## Formatting / linting

There is no configured lint target. Formatting is driven by `.clang-format`:
- WebKit-based style, 150-column limit, 4-space indentation, LF line endings
- function / struct / enum / union braces break onto their own lines; class and namespace braces do not
- includes are preserved (`SortIncludes: Never`, `IncludeBlocks: Preserve`)
- short inline functions and inline lambdas may stay on one line, but short `if`/loop blocks should not
- pointer and reference alignment is left (`int* p`, `T& value`)

Practical formatting command:

```bash
git ls-files '*.cpp' '*.h' '*.hpp' | xargs clang-format -i
```

## Architecture

### Target layout

```text
Core                         executable target and integration/demo flow
ReaderCGNS                   shared library target used by Core
Logger                       header-only async spdlog singleton wrapper
Utils                        reusable header-only infrastructure
3rdparty                     vendored Boost / HDF5 / CGNS / HighFive / mio / spdlog
```

`ReaderCGNS` is a DLL subproject. `Core` links the `ReaderCGNS` CMake target and includes the public headers from `ReaderCGNS/include/` via paths such as `ReaderCGNS/ReaderCGNS.h` and `ReaderCGNS/CgnsCore.h`.

### Main layers

```text
Core/src/Main.cpp                 executable entry point and integration demo
Core/Common/                      process-level bootstrap and Win32 helpers
  SingletonData.*                   CLI parsing, work dir derivation, logger bootstrap
  Functions.*                       string/path/process helpers, SCOPED_TIMER macros, subprocess execution
Core/Utils/                       Core-linked file-format helpers
  HighFiveUtils.hpp                 HDF5 dataset/group helper templates
  MioReader.*                       mmap-backed text ingestion
ReaderCGNS/include/ReaderCGNS/    DLL public headers
ReaderCGNS/src/                   exported API and CGNS file inspection implementation
ReaderCGNS/Utils/                 DLL-local logging bridge
Logger/                           async spdlog singleton wrapper and LOG macros
Utils/                            reusable header-only data structures/helpers
```

### Key patterns

**Singleton application context**  
`SingletonData` is the runtime context for the executable. The macros `SINGLE_DATA`, `INPUT_PATH`, `WORK_DIR`, `WORK_DIR_PATH`, and `IS_DEBUG` are the normal access path from `Core`. `ProcessArguments()` initializes the default async spdlog logger via `dylog::Logger`.

**Two logging layers**  
`Logger/logger.hpp` owns the process default logger and the `LOG_*` macros used by `Core`. `ReaderCGNS/Utils/Logger.h` is DLL-local: it stores a `ReaderCGNS_LogCallback` atomically, formats messages with vendored fmt/spdlog headers, and forwards formatted strings across the DLL boundary. `Core/src/Main.cpp` maps `ReaderCGNS_LogLevel` values back to spdlog levels in `ReaderCGNSLogCallback()`.

**ReaderCGNS DLL boundary**  
`ReaderCGNS/include/ReaderCGNS/ReaderCGNS.h` exports `ReaderCGNS::Logger::SetLogCallback`, `ReaderCGNS::Logger::ClearLogCallback`, and `ReaderCGNS::info(const std::string&)` with `READER_CGNS_DLL`. `ReaderCGNS/src/ReaderCGNS.cpp` implements the exported inspection function and callback setters. `ReaderCGNS/include/ReaderCGNS/CgnsCore.h` exposes a small RAII-style `CgnsCore` class for opening and closing CGNS files.

**CGNS inspection flow**  
`ReaderCGNS::info()` validates files with `cg_is_cgns`, opens them read-only, logs version/precision, then walks bases, zones, flow solutions, discrete data, zone subregions, grid entries/coordinate arrays, element connectivity sections, one-to-one/generalized connectivity, holes, and boundary-condition datasets. CGNS API calls in this path go through `CG_INFO(status)` so failures log `cg_get_error()` with source location. `CgnsCore` separately wraps open/close lifetime and resets `m_cg_file_id` after failure or close.

**Windows-specific utilities are centralized**  
`Core/Common/src/Functions.cpp` contains the Win32 boundary: ACP/UTF-8 conversion, environment lookups, executable path helpers, and `CallCmd`. `CallCmd` launches a child process with stdout/stderr redirected into a pipe, streams output line-by-line to a callback, and uses a Job Object to clean up the spawned process tree if early termination is requested.

**HighFive helpers**  
`Core/Utils/HighFiveUtils.hpp` provides HDF5 convenience wrappers used by `Main.cpp`: vector/array/span dataset writes, compound-type dataset writes, extendable chunked datasets, and create-or-open group access.

**Reusable utilities**  
`Utils/BlockingQueue.hpp` is a bounded/unbounded blocking queue for producer-consumer flows. `max_size == 0` means unbounded; nonzero capacity makes `Push()` wait when the queue is full. `Close()` wakes waiters and prevents new writes, but consumers can keep popping already queued data until `Pop()` returns `std::nullopt`.

`Utils/SmartPrefixSum.hpp` keeps a mutable incremental prefix-sum cache over a referenced vector. It is not thread-safe, assumes source data is read-only or append-only between `query()` calls, and requires `invalidate()` after modifying already-cached elements. Large recomputations use `std::execution::par`; `Core/CMakeLists.txt` links `TBB::tbb` only when `find_package(TBB CONFIG QUIET)` succeeds, otherwise the parallel STL path may run serially.

## Platform and dependencies

This is a Windows-oriented codebase using Win32 APIs such as `CreateProcessW`, `CreatePipe`, `MultiByteToWideChar`, `GetModuleFileNameW`, and Job Objects. All listed third-party dependencies are vendored under `3rdparty/`.

Important dependency facts from `README.md` and CMake:
- Boost 1.91 components `program_options` and `container` are linked by `Core`
- CGNS 4.5.1 is linked by `ReaderCGNS` via `CGNS::cgns_static`
- HDF5 is linked via `hdf5::hdf5-static`; CMake chooses debug/release zlib archives based on `CMAKE_BUILD_TYPE`
- HighFive 3.3.0 and mio are header-only; spdlog 1.17.0 is used from vendored headers
- TBB is optional and only affects whether large `std::execution::par` reductions actually parallelize

## Include style

- third-party headers use `#include <...>`
- project headers usually use quoted paths such as `#include "Functions.h"`, `#include "ReaderCGNS/ReaderCGNS.h"`, or `#include "Utils/BlockingQueue.hpp"`
- `Core` include roots include the repo root, `3rdparty/`, `Core/Common`, and `Core/Utils`
- `ReaderCGNS` exposes `ReaderCGNS/include` publicly and uses the repo root, `3rdparty/`, and `ReaderCGNS/Utils` privately
