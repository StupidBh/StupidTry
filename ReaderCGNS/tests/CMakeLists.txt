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
