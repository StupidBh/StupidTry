add_executable(ReaderCGNSFluidExtensionsTests FluidExtensionsTests.cpp)
target_compile_features(ReaderCGNSFluidExtensionsTests PRIVATE cxx_std_23)
target_compile_definitions(ReaderCGNSFluidExtensionsTests PRIVATE NOMINMAX)
target_include_directories(ReaderCGNSFluidExtensionsTests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(ReaderCGNSFluidExtensionsTests PRIVATE CGNS::cgns_static)
add_dependencies(ReaderCGNSFluidExtensionsTests ReaderCGNS)

foreach (mode IN ITEMS metadata data aliases logging)
    add_test(NAME ReaderCGNS.FluidExtensions.${mode}
            COMMAND ReaderCGNSFluidExtensionsTests $<TARGET_FILE:ReaderCGNS> ${mode})
    set_tests_properties(ReaderCGNS.FluidExtensions.${mode} PROPERTIES TIMEOUT 30)
endforeach ()

add_executable(ReaderCGNSFieldGroupingTests FieldGroupingTests.cpp)
target_compile_features(ReaderCGNSFieldGroupingTests PRIVATE cxx_std_23)
target_compile_definitions(ReaderCGNSFieldGroupingTests PRIVATE NOMINMAX)
target_include_directories(ReaderCGNSFieldGroupingTests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(ReaderCGNSFieldGroupingTests PRIVATE CGNS::cgns_static)
add_dependencies(ReaderCGNSFieldGroupingTests ReaderCGNS)

add_test(NAME ReaderCGNS.FieldGrouping
         COMMAND ReaderCGNSFieldGroupingTests $<TARGET_FILE:ReaderCGNS>)
set_tests_properties(ReaderCGNS.FieldGrouping PROPERTIES TIMEOUT 30)
