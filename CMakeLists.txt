cmake_minimum_required(VERSION 4.3)
project(StupidBhh LANGUAGES CXX)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
    add_compile_options(/utf-8 /EHsc)
endif ()

# ---- Output directories: bin/<Debug|Release>/ at project root ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")

# ---- Common paths ----
set(3RD_ROOT "${CMAKE_SOURCE_DIR}/3rdparty")

# ---- Sub-projects ----
add_subdirectory(ReaderCGNS)
add_subdirectory(Core)
