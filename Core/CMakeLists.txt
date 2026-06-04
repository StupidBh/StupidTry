cmake_minimum_required(VERSION 4.0)
project(Core)

file(GLOB CPP_RESOURCES
        "Common/src/*.cpp"
        "Utils/src/*.cpp"
)

add_executable(Core
        src/main.cpp
        ${CPP_RESOURCES}
)

target_include_directories(Core PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${3RD_ROOT}
        Common
        Utils
)

set(BOOST_ROOT "${3RD_ROOT}/boost")
set(HDF5_ROOT "${3RD_ROOT}/hdf5")
set(CGNS_ROOT "${3RD_ROOT}/cgns")

if (CMAKE_BUILD_TYPE MATCHES "[Dd]ebug")
    set(ZLIB_LIBRARY "${3RD_ROOT}/hdf5/debug/lib/libzsd.a")
else ()
    set(ZLIB_LIBRARY "${3RD_ROOT}/hdf5/lib/libzs.a")
endif ()

find_package(HDF5 CONFIG REQUIRED)
find_package(cgns CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS container program_options)

target_link_libraries(Core PRIVATE
        Boost::container
        Boost::program_options
        hdf5::hdf5-static
        CGNS::cgns_static
)


add_custom_command(TARGET Core POST_BUILD
        COMMAND $<$<BOOL:$<TARGET_RUNTIME_DLLS:Core>>:${CMAKE_COMMAND}>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:Core>>:-E>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:Core>>:copy_if_different>
        $<TARGET_RUNTIME_DLLS:Core>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:Core>>:$<TARGET_FILE_DIR:Core>>
        COMMAND_EXPAND_LISTS
)