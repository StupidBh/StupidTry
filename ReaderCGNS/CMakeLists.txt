project(ReaderCGNS LANGUAGES CXX)

add_library(${PROJECT_NAME} MODULE
        src/ExportFunctions.cpp
        Utils/src/Logger.cpp
        Core/src/FileManager.cpp
        Core/src/ReaderMeshData.cpp
        Core/src/CgnsCore.cpp
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

if (MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE /NOIMPLIB)
endif ()

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Common"
        "${CMAKE_CURRENT_SOURCE_DIR}/Utils"
        "${CMAKE_CURRENT_SOURCE_DIR}/Core"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        READER_CGNS_EXPORTS
)

set(CGNS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cgns")
# HDF5's package config uses FindZLIB; specify the vendored multi-config libraries explicitly.
set(ZLIB_LIBRARY_DEBUG "${CGNS_ROOT}/debug/lib/zsd.lib")
set(ZLIB_LIBRARY_RELEASE "${CGNS_ROOT}/lib/zs.lib")
find_package(cgns CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
        CGNS::cgns_static
)

if (BUILD_TESTING)
    add_executable(ReaderCGNSLoggerTests)
    target_sources(ReaderCGNSLoggerTests PRIVATE
            tests/LoggerCallbackTests.cpp
    )
    target_compile_features(ReaderCGNSLoggerTests PRIVATE cxx_std_23)
    target_include_directories(ReaderCGNSLoggerTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    add_dependencies(ReaderCGNSLoggerTests ReaderCGNS)
    add_test(NAME ReaderCGNS.LoggerCallback COMMAND ReaderCGNSLoggerTests)
endif ()
