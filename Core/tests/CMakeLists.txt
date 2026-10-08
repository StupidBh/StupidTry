add_executable(CoreReaderLogLifetimeTests
        ReaderLogLifetimeTests.cpp
        ../ReaderCGNS/src/AnalysisCGNS.cpp
        ../Common/src/WindowsFunctions.cpp
)
target_compile_features(CoreReaderLogLifetimeTests PRIVATE cxx_std_23)
target_include_directories(CoreReaderLogLifetimeTests PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty
        ${CMAKE_CURRENT_SOURCE_DIR}/../Common
        ${CMAKE_CURRENT_SOURCE_DIR}/../ReaderCGNS
        ${CMAKE_CURRENT_SOURCE_DIR}/../Utils
)
add_dependencies(CoreReaderLogLifetimeTests ReaderCGNS)
add_test(NAME Core.ReaderLogLifetime
        COMMAND CoreReaderLogLifetimeTests
        $<TARGET_FILE_DIR:ReaderCGNS>
        ${CMAKE_SOURCE_DIR}/Core/src/Main.cpp
)
set_tests_properties(Core.ReaderLogLifetime PROPERTIES TIMEOUT 20)
