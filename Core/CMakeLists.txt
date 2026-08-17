cmake_minimum_required(VERSION 4.0)
project(Core)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
        src/Main.cpp
        Common/src/Functions.cpp
        Common/src/SingletonData.cpp
        Utils/src/MioReader.cpp
        Utils/src/ReaderCGNSLogGuard.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${3RD_ROOT}
        Common
        Utils
)

set(BOOST_ROOT "${3RD_ROOT}/boost")
set(HDF5_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/hdf5")
find_package(HDF5 CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS container program_options)
target_link_libraries(${PROJECT_NAME} PRIVATE
        Boost::container
        Boost::program_options
        hdf5::hdf5-shared
        ReaderCGNS
)

if (BUILD_TESTING)
    add_executable(CoreFunctionsTests
            tests/FunctionsTests.cpp
            Common/src/Functions.cpp
    )
    target_include_directories(CoreFunctionsTests PRIVATE
            ${CMAKE_SOURCE_DIR}
            ${3RD_ROOT}
            Common
    )
    add_test(NAME Core.Functions COMMAND CoreFunctionsTests)
endif ()

find_package(TBB CONFIG QUIET)
if (TBB_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)
    message(STATUS "TBB found (${TBB_DIR}) — std::execution::par enabled")
else ()
    message(WARNING "TBB not found under ${TBB_ROOT} — std::execution::par will silently run serially")
endif ()

set(HDF5_RUNTIME_DLL_DIR "$<IF:$<CONFIG:Debug>,${HDF5_ROOT}/debug/bin,${HDF5_ROOT}/bin>")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:${CMAKE_COMMAND}>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:-E>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:copy_if_different>
        $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        $<$<BOOL:$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>>:$<TARGET_FILE_DIR:${PROJECT_NAME}>>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${HDF5_RUNTIME_DLL_DIR}/z$<$<CONFIG:Debug>:d>.dll
        ${HDF5_RUNTIME_DLL_DIR}/aec.dll
        ${HDF5_RUNTIME_DLL_DIR}/szip.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND_EXPAND_LISTS
)
