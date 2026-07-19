#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "libaec::aec-shared" for configuration "Debug"
set_property(TARGET libaec::aec-shared APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libaec::aec-shared PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/aec.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/aec.dll"
  )

list(APPEND _cmake_import_check_targets libaec::aec-shared )
list(APPEND _cmake_import_check_files_for_libaec::aec-shared "${_IMPORT_PREFIX}/debug/lib/aec.lib" "${_IMPORT_PREFIX}/debug/bin/aec.dll" )

# Import target "libaec::sz-shared" for configuration "Debug"
set_property(TARGET libaec::sz-shared APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(libaec::sz-shared PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/szip.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/szip.dll"
  )

list(APPEND _cmake_import_check_targets libaec::sz-shared )
list(APPEND _cmake_import_check_files_for_libaec::sz-shared "${_IMPORT_PREFIX}/debug/lib/szip.lib" "${_IMPORT_PREFIX}/debug/bin/szip.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
