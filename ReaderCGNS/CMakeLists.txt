cmake_minimum_required(VERSION 4.0)
project(ReaderCGNS)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED)
target_sources(${PROJECT_NAME} PRIVATE
        src/ReaderCGNS.cpp
        Utils/src/Logger.cpp
        Core/src/CgnsCore.cpp
        Core/src/FileManager.cpp
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${3RD_ROOT}
        Utils
        Core
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        READER_CGNS_EXPORTS
)

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
endif ()

set(CGNS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cgns")
set(ZLIB_LIBRARY_DEBUG "${CGNS_ROOT}/debug/lib/zsd.lib")
set(ZLIB_LIBRARY_RELEASE "${CGNS_ROOT}/lib/zs.lib")
find_package(cgns CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
        CGNS::cgns_static
)
