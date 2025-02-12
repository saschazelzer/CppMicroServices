#-----------------------------------------------------------------------------
# Build and run the GTest Suite of tests
#-----------------------------------------------------------------------------

set(change_namespace_test_exe_name ChangeNamespaceTests)

# Make sure that the correct paths separators are used on each platform
if(WIN32)
  set(DIR_SEP "\\\\")
  string(REPLACE "/" "\\\\" CMAKE_LIBRARY_OUTPUT_DIRECTORY_NATIVE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  string(REPLACE "/" "\\\\" CMAKE_RUNTIME_OUTPUT_DIRECTORY_NATIVE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
else()
  set(DIR_SEP "/")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_NATIVE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_NATIVE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/TestingConfig.h.in" "${PROJECT_BINARY_DIR}/include/CNTestingConfig.h")

include_directories(
  ${GTEST_INCLUDE_DIRS}
  )

#-----------------------------------------------------------------------------
# Add test source files
#-----------------------------------------------------------------------------
set(_change_namespace_tests
  ChangeNamespaceTest.cpp
  main.cpp
)

#-----------------------------------------------------------------------------
# Build the main test driver executable
#-----------------------------------------------------------------------------

add_executable(${change_namespace_test_exe_name}
  ${_change_namespace_tests}
)

set_property(
  TARGET ${change_namespace_test_exe_name}
  APPEND PROPERTY COMPILE_DEFINITIONS US_BUNDLE_NAME=main
  )

set_property(
  TARGET ${change_namespace_test_exe_name}
  PROPERTY US_BUNDLE_NAME main
  )

# Disable deprecation warnings.
if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR
    ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang") OR
    ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
  set_source_files_properties(${_change_namespace_tests}
    PROPERTIES
    COMPILE_FLAGS -Wno-error=deprecated-declarations
    COMPILE_FLAGS -Wno-sign-compare
    )
endif()


target_link_libraries(${change_namespace_test_exe_name}
  PRIVATE
  ${GTEST_BOTH_LIBRARIES}
  ${${PROJECT_NAME}_LINK_LIBRARIES}
  gtest
)

# Run the GTest EXE from ctest.
add_test(NAME ${change_namespace_test_exe_name}
  COMMAND ${change_namespace_test_exe_name}
  WORKING_DIRECTORY ${CppMicroServices_BINARY_DIR}
)

# Copy the Google Test libraries into the same folder as the
# executable so that they can be seen at runtime on Windows.
# Mac and Linux use RPATHs and do not need to do this.
if (WIN32 AND US_USE_SYSTEM_GTEST)
  foreach(lib_fullpath ${GTEST_BOTH_LIBRARIES})
    get_filename_component(dir ${lib_fullpath} DIRECTORY)
    get_filename_component(name_no_ext ${lib_fullpath} NAME_WE)
    set(dll_file "${dir}/${name_no_ext}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    add_custom_command(TARGET ${change_namespace_test_exe_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${dll_file}"
        $<TARGET_FILE_DIR:${change_namespace_test_exe_name}>)
  endforeach(lib_fullpath)
endif()