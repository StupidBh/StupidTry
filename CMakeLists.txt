cmake_minimum_required(VERSION 4.0)
project(StupidBhh)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Output directories: bin/<Debug|Release>/ at project root ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")

# ---- Common paths ----
set(3RD_ROOT "${CMAKE_SOURCE_DIR}/3rdparty")

# ---- Sub-projects ----
add_subdirectory(Core)
