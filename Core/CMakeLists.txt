cmake_minimum_required(VERSION 4.0)
project(Core)

file(GLOB CPP_RESOURCES
        "Common/src/*.cpp"
        "Utils/src/*.cpp"
)

add_executable(${PROJECT_NAME}
        src/main.cpp
        ${CPP_RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${3RD_ROOT}
        Common
        Utils
)

set(BOOST_ROOT "${3RD_ROOT}/boost")
set(HDF5_ROOT "${3RD_ROOT}/hdf5")
set(CGNS_ROOT "${3RD_ROOT}/cgns")

message(WARNING "Dir ${CMAKE_SOURCE_DIR} — ${CMAKE_CURRENT_SOURCE_DIR}")


if (CMAKE_BUILD_TYPE MATCHES "[Dd]ebug")
    set(ZLIB_LIBRARY "${3RD_ROOT}/hdf5/debug/lib/libzsd.a")
else ()
    set(ZLIB_LIBRARY "${3RD_ROOT}/hdf5/lib/libzs.a")
endif ()

find_package(HDF5 CONFIG REQUIRED)
find_package(cgns CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS container program_options)

target_link_libraries(${PROJECT_NAME} PRIVATE
        Boost::container
        Boost::program_options
        hdf5::hdf5-static
        CGNS::cgns_static
)

find_package(TBB CONFIG QUIET)
if (TBB_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
    message(STATUS "TBB found (${TBB_DIR}) — std::execution::par enabled")
else ()
    message(WARNING "TBB not found under ${TBB_ROOT} — std::execution::par will silently run serially")
endif ()


add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:${CMAKE_COMMAND}>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:-E>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:copy_if_different>
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:$<TARGET_FILE_DIR:${PROJECT_NAME}>>
        COMMAND_EXPAND_LISTS
)