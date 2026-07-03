cmake_minimum_required(VERSION 4.0)
project(ReaderCGNS)

file(GLOB CPP_RESOURCES
        "src/*.cpp"
        "Utils/src/*.cpp"
)

add_library(${PROJECT_NAME} SHARED
        ${CPP_RESOURCES}
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${3RD_ROOT}
        Utils
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        READERCGNS_BUILD
)

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
endif ()

set(CGNS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cgns")
if (CMAKE_BUILD_TYPE MATCHES "[Dd]ebug")
    set(ZLIB_LIBRARY "${CGNS_ROOT}/debug/lib/libzsd.a")
else ()
    set(ZLIB_LIBRARY "${CGNS_ROOT}/lib/libzs.a")
endif ()
find_package(cgns CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
        CGNS::cgns_static
)
