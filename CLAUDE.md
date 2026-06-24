# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# Configure a Debug build
cmake -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build the executable
cmake --build cmake-build-debug

# Configure and build Release
cmake -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
```

The root `CMakeLists.txt` requires CMake 4.0+, sets C to C17 and C++ to C++23, writes runtime/library/archive outputs to `bin/<Debug|Release>/`, and only adds the `Core/` subdirectory. The main executable is `bin/<Debug|Release>/Core.exe`.

`Core/CMakeLists.txt` builds `Core` from `Core/src/main.cpp` plus every `.cpp` collected from `Core/Common/src/` and `Core/Utils/src/` with `file(GLOB ...)`. Adding a new compiled Core source file usually means placing it under one of those two `src/` directories.

## Run

```bash
# Show CLI help
./bin/Debug/Core.exe --help

# Run with explicit work directory
./bin/Debug/Core.exe --inputPath <path-to-input-file> --workDirectory .

# Run and let workDirectory auto-derive from inputPath
./bin/Debug/Core.exe --inputPath <path-to-input-file>
```

CLI contract, implemented in `SingletonData::ProcessArguments()`:
- `--help/-h`
- `--inputPath/-i` (required unless showing help)
- `--workDirectory/-w` (optional)
- `--DEBUG` (bool switch; forced on in Debug builds through `#ifndef NDEBUG`)

If `--workDirectory` is omitted, `SingletonData` derives it from `inputPath`'s parent directory, or falls back to `./<input-stem>/` when the input has no parent component.

## Tests and validation

There is no first-party test target wired into CMake right now: no `enable_testing()`, no `add_test()`, and no dedicated `tests/` directory. There is also no single-test command because there are no registered CTest targets yet.

Current validation is manual:

```bash
cmake --build cmake-build-debug
./bin/Debug/Core.exe --inputPath <path-to-input-file>
```

The current entry point does more than inspect the input: after best-effort CGNS inspection it writes and reads `WORK_DIR_PATH / "Try1.h5"`, runs `ipconfig` through `CallCmd`, logs executable paths, runs a bounded `BlockingQueue<std::size_t>` producer-consumer demo, then shuts down spdlog.

## Formatting / linting

There is no configured lint target. Formatting is driven by `.clang-format`:
- WebKit-based style
- 120-column limit
- 4-space indentation, no tabs
- LF line endings
- function / struct / enum braces break onto their own lines; class and namespace braces do not
- includes are preserved (`SortIncludes: Never`, `IncludeBlocks: Preserve`)
- short inline lambdas/functions may stay on one line, but short `if`/loop blocks should not

A practical formatting command is:

```bash
git ls-files '*.cpp' '*.h' '*.hpp' | xargs clang-format -i
```

## Architecture

### Entry point

`Core/src/main.cpp` is a local demo/bootstrap flow:
1. parse CLI arguments through `SINGLE_DATA.ProcessArguments(argc, argv)` and exit on failure/help
2. run best-effort CGNS inspection via `CgnsCore` when the input is a valid CGNS file
3. run a HighFive HDF5 write/read round-trip into `WORK_DIR_PATH / "Try1.h5"`
4. run `CallCmd("ipconfig")`
5. log executable path information
6. run a two-thread producer-consumer queue demo with `BlockingQueue<std::size_t>`
7. call `spdlog::shutdown()` before exit

CGNS open failure does not abort the whole process; the later demo paths still run unless they hit their own errors.

### Layer map

```text
Core/src/main.cpp          entry point and local demo flow
Core/Common/               process-level bootstrap and Win32 helpers
  SingletonData.*            CLI parsing, work dir derivation, logger bootstrap
  Functions.*                string/path/process helpers, SCOPED_TIMER macros, subprocess execution
Core/Utils/                Core-linked domain and file-format utilities
  CgnsCore.*                 RAII wrapper over the CGNS mid-level API
  MioReader.*                mmap-backed text ingestion
  HighFiveUtils.hpp          HDF5 dataset/group helper templates
Utils/                     reusable header-only infrastructure
  BlockingQueue.hpp          bounded/unbounded blocking queue for producer-consumer flows
  SyncController.hpp         single-slot producer/consumer synchronization helper
  SmartPrefixSum.hpp         prefix-sum helper with optional parallel STL backend
Logger/                    header-only async spdlog singleton wrapper
3rdparty/                  vendored Boost / HDF5 / CGNS / HighFive / mio / spdlog / TBB
```

### Key patterns

**Singleton application context**  
`SingletonData` is the runtime context. The macros `SINGLE_DATA`, `INPUT_PATH`, `WORK_DIR`, `WORK_DIR_PATH`, and `IS_DEBUG` are the normal access path from the rest of the app. `WORK_DIR_PATH` returns a `std::filesystem::path` and creates the directory on demand.

**Logger bootstrap order matters**  
`dylog::Logger::get_instance().InitLog(work_dir, "stupid-bhh", verbose)` is called from `SingletonData::ProcessArguments()`. Anything using `LOG_*` before successful argument parsing risks hitting an uninitialized default logger.

**Windows-specific utilities are centralized**  
`Core/Common/src/Functions.cpp` contains the Win32 boundary: ACP/UTF-8 conversion, environment lookups, executable path helpers, and `CallCmd`.

`CallCmd` launches a child process with stdout/stderr redirected into a pipe, streams output line-by-line to a callback, and treats a callback return of `true` as an early-stop request. Early stop is cooperative first (close the read pipe so later writes usually hit `ERROR_BROKEN_PIPE`), then forced through a Job Object timeout path so the spawned process tree is cleaned up if the child does not exit promptly.

**CGNS inspection flow**  
`CgnsCore` is a thin RAII wrapper around `cgnslib.h`. `OpenCGNS()` validates with `cg_is_cgns`, opens read-only, logs version/precision, and resets `m_cg_file_id` on failure so `IsOpen()` and destruction never see a stale handle.

`CgnsCore::info()` is a read-only traversal/reporting pass. It walks bases, zones, flow solutions and their fields, discrete data, zone subregions, grid entries/coordinate arrays, and element connectivity sections. Every CGNS API call in this path should go through `CG_INFO(status)` so failures log `cg_get_error()` consistently with source location.

**HighFive helpers**  
`Core/Utils/HighFiveUtils.hpp` provides the HDF5 convenience layer used by `main.cpp`:
- `WriteDataSet` overloads for vectors/arrays/spans
- compound-type dataset writes
- `WriteDataSet2` for extendable chunked datasets
- `GetGroup` for create-or-open group access

**Queue-based producer/consumer helper**  
`Utils/BlockingQueue.hpp` is a header-only blocking queue. `max_size == 0` means unbounded; nonzero capacity makes `Push()` wait when the queue is full, giving backpressure when consumption is slower than production. `Close()` wakes waiters and prevents new writes, but does not discard already queued data; consumers keep calling `Pop()` until it returns `std::nullopt`.

**Optional TBB-backed parallelism**  
`Utils/SmartPrefixSum.hpp` uses `std::execution::par` for large-range reductions. `Core/CMakeLists.txt` links `TBB::tbb` only when `find_package(TBB CONFIG QUIET)` succeeds; otherwise the code still builds, but parallel STL execution silently falls back to serial behavior on this toolchain.

## Platform and dependencies

This is a Windows-only codebase using Win32 APIs (`CreateProcessW`, `CreatePipe`, `MultiByteToWideChar`, `GetModuleFileNameW`, Job Objects, etc.). The README lists the vendored third-party dependency set; CMake resolves them from `3rdparty/`.

Important dependency facts:
- Boost.ProgramOptions / Boost.Container are vendored and linked statically
- CGNS is linked via `CGNS::cgns_static`
- HDF5 is linked via `hdf5::hdf5-static`; `Core/CMakeLists.txt` chooses debug/release zlib archives based on `CMAKE_BUILD_TYPE`
- HighFive, mio, and spdlog are header-only in this repo
- TBB is optional and only affects whether parallel STL work is truly parallel

## Include style

- third-party headers: `#include <...>`
- project headers usually use quoted paths such as `#include "Common/Functions.h"` or `#include "Utils/BlockingQueue.hpp"`
- repo root, `Core/Common`, `Core/Utils`, and `3rdparty/` are all on the include path for the `Core` target
