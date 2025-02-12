# Extract the current version from the VERSION file
file(STRINGS VERSION _version LIMIT_COUNT 1)

set(US_CMAKE_MINIMUM_REQUIRED_VERSION 3.17)

cmake_minimum_required(VERSION ${US_CMAKE_MINIMUM_REQUIRED_VERSION})

project(CppMicroServices VERSION ${_version})

set(US_GLOBAL_VERSION_SUFFIX ${${PROJECT_NAME}_VERSION_MAJOR})
if (${PROJECT_NAME}_VERSION_MINOR EQUAL 99)
  math(EXPR US_GLOBAL_VERSION_SUFFIX "${US_GLOBAL_VERSION_SUFFIX}+1")
endif()

string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

cmake_policy(VERSION ${US_CMAKE_MINIMUM_REQUIRED_VERSION})

list(APPEND policies_new
  CMP0025 # Compiler id for Apple Clang is now AppleClang (CMake 3.0)
  CMP0054 # Only interpret if() arguments as variables or keywords when unquoted
  CMP0079
  )
foreach(policy ${policies_new})
  if(POLICY ${policy})
    cmake_policy(SET ${policy} NEW)
  endif()
endforeach()

#-----------------------------------------------------------------------------
# Setup git hooks
#-----------------------------------------------------------------------------
find_package(Git)
if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  execute_process(COMMAND ${GIT_EXECUTABLE} config core.hooksPath "${PROJECT_SOURCE_DIR}/.githooks"
                          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                          RESULT_VARIABLE GIT_CONFIGMOD_RESULT)
  if (NOT GIT_CONFIGMOD_RESULT EQUAL "0")
    message(WARNING "Command `git config core.hooksPath ${PROJECT_SOURCE_DIR}/.githooks` failed with ${GIT_CONFIGMOD_RESULT}")
  endif()
endif()

#-----------------------------------------------------------------------------
# Check minimum required compiler versions
#------------------------------------------------------------------------------

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_ENABLE_EXPORTS ON)

# Since version 3.1, CMake pulls in implicit link libraries when compiling
# source files in C mode. Because we mix C and C++ code in the test driver
# executable, we end up with a gcc_eh link dependency when using MinGW
# which breaks exception handling in the executable. See also:
#
# https://stackoverflow.com/questions/46688200/mixing-c-and-c-causing-exceptions-to-terminate
#
set(CMAKE_C_IMPLICIT_LINK_LIBRARIES "")

# We make use of the following C++11 language and library features:
#
# Language feature                 GCC   Clang   Xcode   VisualStudio
# -------------------------------------------------------------------
# Rvalue references                4.3   2.9     6       2012
# Variadic templates               4.3   2.9     6       2013*
# Initializer lists                4.4   3.1     6       2013
# Static assertions                4.3   2.9     6       2010
# auto                             4.4   2.9     6       2010
# Lambdas                          4.5   3.1     6       2012
# decltype                         4.3   2.9     6       2012
# Right angle brackets             4.3   2.9     6       2010
# Null pointer constant            4.6   3.0     6       2010
# Explicit conversion operators    4.5   3.0     6       2013
# Defaulted functions              4.4   3.0     6       2013*
# Deleted functions                4.4   2.9     6       2013*
# Range-based for                  4.6   3.0     6       2012
# Atomic operations                4.4   3.1     6       2012
# thread_local (optional)          4.8   3.3     8       2015
#
# Library features used
# ---------------------
# function
# unique_ptr
# shared_ptr
# unordered_map
# unordered_set
# atomic
# mutex
# wait_condition
# exception_ptr

set(US_COMPILER_GNU_MINIMUM_VERSION          7.5.0         )
set(US_COMPILER_Clang_MINIMUM_VERSION        9.0.0         )
set(US_COMPILER_AppleClang_MINIMUM_VERSION   10.0.0        )
set(US_COMPILER_MSVC_MINIMUM_VERSION         19.10         )

set(US_COMPILER_GNU_MINIMUM_VERSION_PRODUCT        "GCC ${US_COMPILER_GNU_MINIMUM_VERSION}")
set(US_COMPILER_Clang_MINIMUM_VERSION_PRODUCT      "Clang ${US_COMPILER_CLANG_MINIMUM_VERSION}")
set(US_COMPILER_AppleClang_MINIMUM_VERSION_PRODUCT "Xcode 7.3 (Clang ${US_COMPILER_AppleClang_MINIMUM_VERSION})")
set(US_COMPILER_MSVC_MINIMUM_VERSION_PRODUCT       "Visual Studio 2015 (MSVC ${US_COMPILER_MSVC_MINIMUM_VERSION})")

set(US_COMPILER_GNU_MINIMUM_RECOMMENDED_VERSION         9.4.0   )
set(US_COMPILER_Clang_MINIMUM_RECOMMENDED_VERSION       9.0.0   )
set(US_COMPILER_AppleClang_MINIMUM_RECOMMENDED_VERSION  12.0.1  )
set(US_COMPILER_MSVC_MINIMUM_RECOMMENDED_VERSION        19.20   )

set(US_COMPILER_GNU_MINIMUM_RECOMMENDED_VERSION_PRODUCT        "GCC ${US_COMPILER_GNU_MINIMUM_RECOMMENDED_VERSION}")
set(US_COMPILER_Clang_MINIMUM_RECOMMENDED_VERSION_PRODUCT      "Clang ${US_COMPILER_Clang_MINIMUM_RECOMMENDED_VERSION}")
set(US_COMPILER_AppleClang_MINIMUM_RECOMMENDED_VERSION_PRODUCT "Xcode 8.0 (Clang ${US_COMPILER_AppleClang_MINIMUM_RECOMMENDED_VERSION})")
set(US_COMPILER_MSVC_MINIMUM_RECOMMENDED_VERSION_PRODUCT       "Visual Studio 2015 (MSVC ${US_COMPILER_MSVC_MINIMUM_RECOMMENDED_VERSION})")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(US_COMPILER_GNU 1)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(US_COMPILER_CLANG 1)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  set(US_COMPILER_APPLE_CLANG 1)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(US_COMPILER_MSVC 1)
else()
  message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang (Linux or Apple), GCC and MSVC.")
endif()

# Detect if we are using lld, if so, set US_USING_LLD to ON, OFF otherwise
string(REGEX MATCH ".*(ld\.lld)$" _matches_lld "${CMAKE_LINKER}")
if (NOT _matches_lld STREQUAL "")
  set(US_USING_LLD ON)
else()
  set(US_USING_LLD OFF)
endif()

message(STATUS "Detected Linker: ${CMAKE_LINKER}")

if (CMAKE_CXX_COMPILER_VERSION AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${US_COMPILER_${CMAKE_CXX_COMPILER_ID}_MINIMUM_VERSION})
  message(FATAL_ERROR "${US_COMPILER_${CMAKE_CXX_COMPILER_ID}_MINIMUM_VERSION_PRODUCT} or higher required (you are using ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}).")
endif()

STRING(SUBSTRING "${CMAKE_SYSTEM_PROCESSOR}" 0 3 SYSTEM_PROCESSOR_SUBSTR)
if("${SYSTEM_PROCESSOR_SUBSTR}" STREQUAL "arm")
  if(US_COMPILER_GNU)
    set(US_COMPILER_GNU_ARM 1)
endif()
endif()


#-----------------------------------------------------------------------------
# Update CMake module path
#------------------------------------------------------------------------------

set(US_CMAKE_DIR ${PROJECT_SOURCE_DIR}/cmake)

set(CMAKE_MODULE_PATH
  ${US_CMAKE_DIR}
  ${CMAKE_MODULE_PATH}
  )

#-----------------------------------------------------------------------------
# CMake function(s) and macro(s)
#-----------------------------------------------------------------------------

include(CMakeParseArguments)
include(CMakePackageConfigHelpers)
include(CheckCXXSourceCompiles)
include(usFunctionAddResources)
include(usFunctionEmbedResources)
include(usFunctionGetResourceSource)
include(usFunctionCheckResourceLinking)
include(usFunctionCheckCompilerFlags)
include(usFunctionGenerateBundleInit)
include(usMacroCreateBundle)
include(usFunctionCreateTestBundle)
include(usFunctionCreateDSTestBundle)
include(usFunctionBoostPath)

if (US_COMPILER_CLANG OR US_COMPILER_APPLE_CLANG)
  check_cxx_compiler_flag(-Wno-missing-braces HAS_MISSING_BRACES_FLAG)
  if (HAS_MISSING_BRACES_FLAG)
    add_compile_options(-Wno-missing-braces)
  endif()
endif()

if(US_BUILD_TESTING)
  include(usFunctionCompileSnippets)
endif()

#-----------------------------------------------------------------------------
# Init output directories
#-----------------------------------------------------------------------------

set(US_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(US_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(US_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

foreach(_type ARCHIVE LIBRARY RUNTIME)
  if(NOT CMAKE_${_type}_OUTPUT_DIRECTORY)
    set(CMAKE_${_type}_OUTPUT_DIRECTORY ${US_${_type}_OUTPUT_DIRECTORY})
  endif()
endforeach()

#-----------------------------------------------------------------------------
# Set a default build type if none was specified
#-----------------------------------------------------------------------------

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to 'Debug' as none was specified.")
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)

  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
          STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

#-----------------------------------------------------------------------------
# CMake options
#-----------------------------------------------------------------------------

function(us_cache_var _var_name _var_default _var_type _var_help)
  set(_advanced 0)
  set(_force)
  foreach(_argn ${ARGN})
    if(_argn STREQUAL ADVANCED)
      set(_advanced 1)
    elseif(_argn STREQUAL FORCE)
      set(_force FORCE)
    endif()
  endforeach()

  set(${_var_name} ${_var_default} CACHE ${_var_type} "${_var_help}" ${_force})
  if(_advanced)
    mark_as_advanced(${_var_name})
  endif()
endfunction()

# Determine the name of the install component for "SDK" artifacts.
# The default is "sdk"
if(NOT DEFINED US_SDK_INSTALL_COMPONENT)
  set(US_SDK_INSTALL_COMPONENT COMPONENT sdk)
elseif(US_SDK_INSTALL_COMPONENT)
  set(US_SDK_INSTALL_COMPONENT COMPONENT ${US_SDK_INSTALL_COMPONENT})
endif()

us_cache_var(BUILD_SHARED_LIBS ON BOOL "Build shared libraries")
us_cache_var(CMAKE_DEBUG_POSTFIX d STRING "Executable and library debug name postfix" ADVANCED)

us_cache_var(US_ENABLE_THREADING_SUPPORT ON BOOL "Enable threading support")
us_cache_var(US_ENABLE_TSAN OFF BOOL "Enable tsan (thread sanitizer, Linux only)" ADVANCED)
us_cache_var(US_ENABLE_ASAN OFF BOOL "Enable asan (address sanitizer)" ADVANCED)
us_cache_var(US_ASAN_USER_DLL "" STRING "Path to ASAN DLL (Windows only)" ADVANCED)
us_cache_var(US_ENABLE_MSAN OFF BOOL "Enable msan (memory sanitizer, Clang only)" ADVANCED)
us_cache_var(US_ENABLE_UBSAN OFF BOOL "Enable ubsan (undefined behavior sanitizer, Clang only)" ADVANCED)
us_cache_var(US_ENABLE_COVERAGE OFF BOOL "Enable code coverage" ADVANCED)
us_cache_var(US_BUILD_TESTING OFF BOOL "Build tests")
us_cache_var(US_BUILD_EXAMPLES OFF BOOL "Build example projects")
us_cache_var(US_USE_SYSTEM_GTEST OFF BOOL "Build using an external GTest installation" ADVANCED)
us_cache_var(US_USE_SYSTEM_BOOST OFF BOOL "Build using an external Boost installation" ADVANCED)
us_cache_var(US_USE_DETERMINISTIC_BUNDLE_BUILDS OFF BOOL "Build bundles deterministically" ADVANCED)

set(_us_build_shared ${BUILD_SHARED_LIBS})

if(NOT US_ENABLE_THREADING_SUPPORT AND US_ENABLE_TSAN)
  message(SEND_WARN "Thread-sanitizer enabled without threading support. Forcing US_ENABLE_TSAN to OFF")
  set(US_ENABLE_TSAN OFF)
endif()

if (US_ENABLE_MSAN)
  message(FATAL_ERROR "Currently MSAN is not supported for CppMicroServices. Enabling \
it requires that we link against a msan-instrumented libc++ or libstdc++. The CMake infrasture \
still exists in the event we decide to fully implement it later")
endif()

# Count number of sanitizers that are enabled
set(xsan_enabled_count 0)
foreach(_xsan US_ENABLE_TSAN;US_ENABLE_ASAN;US_ENABLE_MSAN;US_ENABLE_UBSAN)
  if (${_xsan})
    math(EXPR xsan_enabled_count "${xsan_enabled_count}+1")

    # Extract name of the enabled sanitizer and store it in selected_sanitizer
    string(FIND "${_xsan}" "_" _sanitizer_name_start REVERSE)
    math(EXPR _sanitizer_name_start "${_sanitizer_name_start}+1")
    string(SUBSTRING "${_xsan}" ${_sanitizer_name_start} -1 _sanitizer_name)
    set(selected_sanitizer ${_sanitizer_name})
  endif()
endforeach()

# If more than one sanitizer is turned on, print an error stating that only one
# sanitizer can be turned on at any given point
if (xsan_enabled_count GREATER 1)
  message(FATAL_ERROR "More than one sanitizer is enabled. Please only have one sanitizer enabled at a time.")
endif()

# If MSAN or UBSAN are turned on, make sure clang is being used. If not,
# print an error stating that those sanitizers are only available if compiling
# with clang and lld
if ((NOT(US_COMPILER_CLANG) OR NOT(US_USING_LLD)) AND (US_ENABLE_MSAN OR US_ENABLE_UBSAN))
  message(FATAL_ERROR "You are trying to use msan or ubsan either without clang as your compiler or \
             lld as your linker. Please use clang and lld if you wish to use msan or ubsan")
endif()

# Display the selected sanitizer if one was enabled
if (NOT ${selected_sanitizer} STREQUAL "")
  message(STATUS "Selected sanitizer: ${selected_sanitizer}")
endif()

if(WIN32 AND NOT CYGWIN)
  set(default_runtime_install_dir bin/)
  set(default_tools_install_dir bin/)
  set(default_library_install_dir bin/)
  set(default_archive_install_dir lib/)
  set(default_header_install_dir include/${PROJECT_NAME_LOWER}${US_GLOBAL_VERSION_SUFFIX})
  set(default_auxiliary_install_dir share/${PROJECT_NAME_LOWER}${US_GLOBAL_VERSION_SUFFIX})
  set(default_doc_install_dir ${default_auxiliary_install_dir}/doc)
  set(default_man_install_dir share/man)
else()
  set(default_runtime_install_dir bin/)
  set(default_tools_install_dir bin/)
  set(default_library_install_dir lib/)
  set(default_archive_install_dir lib/)
  set(default_header_install_dir include/${PROJECT_NAME_LOWER}${US_GLOBAL_VERSION_SUFFIX})
  set(default_auxiliary_install_dir share/${PROJECT_NAME_LOWER}${US_GLOBAL_VERSION_SUFFIX})
  set(default_doc_install_dir ${default_auxiliary_install_dir}/doc)
  set(default_man_install_dir share/man)
endif()

us_cache_var(RUNTIME_INSTALL_DIR ${default_runtime_install_dir} STRING "Relative install location for binaries" ADVANCED)
us_cache_var(TOOLS_INSTALL_DIR ${default_tools_install_dir} STRING "Relative install location for tools" ADVANCED)
us_cache_var(LIBRARY_INSTALL_DIR ${default_library_install_dir} STRING "Relative install location for libraries" ADVANCED)
us_cache_var(ARCHIVE_INSTALL_DIR ${default_archive_install_dir} STRING "Relative install location for archives" ADVANCED)
us_cache_var(HEADER_INSTALL_DIR ${default_header_install_dir} STRING "Relative install location for headers" ADVANCED)
us_cache_var(AUXILIARY_INSTALL_DIR ${default_auxiliary_install_dir} STRING "Relative install location for auxiliary files" ADVANCED)
us_cache_var(DOC_INSTALL_DIR ${default_doc_install_dir} STRING "Relative install location for documentation files" ADVANCED)
us_cache_var(MAN_INSTALL_DIR ${default_man_install_dir} STRING "Relative install location for man pages" ADVANCED)
set(AUXILIARY_CMAKE_INSTALL_DIR ${AUXILIARY_INSTALL_DIR}/cmake)

set(US_BUNDLE_INIT_TEMPLATE "${US_CMAKE_DIR}/BundleInit.cpp" CACHE INTERNAL "The bundle initialization template code")
set(US_RESOURCE_RC_TEMPLATE "${US_CMAKE_DIR}/cppmicroservices_resources.rc.in" CACHE INTERNAL "The Windows RC resource template")
set(US_CMAKE_RESOURCE_DEPENDENCIES_CPP "${US_CMAKE_DIR}/CMakeResourceDependencies.cpp" CACHE INTERNAL "The dummy resource dependencies code")
if(APPLE)
  # Avoid RPATH warning in cmakegui
  set(CMAKE_MACOSX_RPATH ON)
endif()

# Cross-compiled builds need access to the host version of usResourceCompiler.
# ImportExecutables.cmake is generated by the host build and contains the cmake target
# for the host built usResourceCompiler. ImportExecutables.cmake can be included by the
# cross-compiled build to access the host built usResourceCompiler.
#
set(IMPORT_EXECUTABLES "${CMAKE_BINARY_DIR}/ImportExecutables.cmake" CACHE FILEPATH "A file containing the CMake target definition for the host build version of usResourceCompiler.")

# global options based on compiler
if(NOT APPLE AND US_COMPILER_CLANG)
  # If we are using ld.lld instead of ld, which we will do from now on
  # when building with clang on Linux because of a bug in CMake 3.21.1 (and possibly
  # other version), we need to specify additional linker flags which specify
  # the target emulator for ld.lld to use.
  if (US_USING_LLD)
    set(_ADDITIONAL_LINKER_FLAGS -m elf_x86_64)
  endif()
endif()

if (APPLE AND US_COMPILER_APPLE_CLANG)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
endif()

#-----------------------------------------------------------------------------
# Set stack as non executable
#-----------------------------------------------------------------------------

# Add flag to force non executable stack on gcc builds
# The -Wl,-z noexecstack link option on all gcc builds will
# disallow executable stacks within CppMicroServices.
if(UNIX AND NOT APPLE)
  link_libraries("-Wl,-z,noexecstack")
endif()

#-----------------------------------------------------------------------------
# 3rd Party Library configuration
#-----------------------------------------------------------------------------

if(US_USE_SYSTEM_BOOST)
    find_package(Boost 1.74.0 REQUIRED)
endif()

#-----------------------------------------------------------------------------
# Testing configuration
#-----------------------------------------------------------------------------

function(us_add_tests test_driver)
  foreach(test ${ARGN})
    add_test(NAME ${test} COMMAND ${test_driver} ${test}
      WORKING_DIRECTORY ${CppMicroServices_BINARY_DIR})
    set_property(TEST ${test} PROPERTY LABELS regular)
  if(US_MEMCHECK_COMMAND)
    add_test(NAME memcheck_${test} COMMAND ${US_MEMCHECK_COMMAND} --error-exitcode=1 ${US_RUNTIME_OUTPUT_DIRECTORY}/${test_driver} ${test}
      WORKING_DIRECTORY ${CppMicroServices_BINARY_DIR})
    set_property(TEST memcheck_${test} PROPERTY LABELS valgrind memcheck)
    if(US_ENABLE_HELGRIND)
      # By default, these tests are not active until the concurrent bundle
      # start / stop mechanism has been properly implemented (GitHub Issue #25)
      add_test(NAME helgrind_${test} COMMAND ${US_MEMCHECK_COMMAND} --tool=helgrind --error-exitcode=1 ${US_RUNTIME_OUTPUT_DIRECTORY}/${test_driver} ${test}
   WORKING_DIRECTORY ${CppMicroServices_BINARY_DIR})
      set_property(TEST helgrind_${test} PROPERTY LABELS valgrind helgrind)
    endif()
  endif()
  if(US_COVERAGE_COMMAND)
    file(TO_NATIVE_PATH ${PROJECT_SOURCE_DIR} PROJECT_SOURCE_NATIVE_PATH)
    file(TO_NATIVE_PATH ${US_RUNTIME_OUTPUT_DIRECTORY} US_RUNTIME_OUTPUT_NATIVE_DIRECTORY)
    # skip the unicode test bundle because opencppcoverage tool fails if a module is located in a path with unicode characters
    add_test(NAME coverage_${test}
        COMMAND ${US_COVERAGE_COMMAND} --verbose --cover_children --sources=${PROJECT_SOURCE_NATIVE_PATH} --export_type=binary:${test}.cov --continue_after_cpp_exception --excluded_modules *TestBundleU*.dll -- $<TARGET_FILE:${test_driver}> ${test}
        WORKING_DIRECTORY ${CppMicroServices_BINARY_DIR})
    set_property(TEST coverage_${test} PROPERTY LABELS opencppcoverage)
  endif()
  endforeach()
endfunction()

# Valgrind rc file
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/valgrindrc
  ${CppMicroServices_BINARY_DIR}/.valgrindrc
  @ONLY
  )

if(US_BUILD_TESTING)
  enable_testing()
  find_program(US_MEMCHECK_COMMAND valgrind)
  mark_as_advanced(US_MEMCHECK_COMMAND)
  if(WIN32 AND US_ENABLE_COVERAGE)
    find_program(US_COVERAGE_COMMAND NAMES OpenCppCoverage OpenCppCoverage.exe DOC "path to the code coverage tool" HINTS "C:/Program Files/OpenCppCoverage" "C:/Program Files (x86)/OpenCppCoverage")
    mark_as_advanced(US_COVERAGE_COMMAND)
  endif()

  if(US_USE_SYSTEM_GTEST)
    find_package(GTest REQUIRED)
  else()
    # This keeps GTest CMake variables hidden from users unless they explicitly want to view/modify them.
    us_cache_var(BUILD_GMOCK ON BOOL "Build GMock" ADVANCED FORCE)
    us_cache_var(BUILD_GTEST ON BOOL "Build GTest" ADVANCED FORCE)

    us_cache_var(gtest_build_samples OFF BOOL "Build GTest samples" ADVANCED)
    us_cache_var(gtest_build_tests OFF BOOL "Build GTest tests" ADVANCED)
    us_cache_var(gtest_disable_pthreads OFF BOOL "Disable the use of pthreads" ADVANCED)

    # Prevent overriding the parent project's compiler/linker
    # settings on Windows
    us_cache_var(gtest_force_shared_crt ON BOOL "" ADVANCED FORCE)

    us_cache_var(gtest_hide_internal_symbols OFF BOOL "" ADVANCED)

    # pthreads on MinGW is not supported, turn it off. Refer to
    # https://github.com/google/googletest/pull/721/files for details.
    if(MINGW)
     set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
    endif()

    set(GTEST_BOTH_LIBRARIES gtest gtest_main)
    set(GMOCK_BOTH_LIBRARIES gmock gmock_main)

    # Prevent GTest/GMock from installing libraries into our install dir.
    set(GTEST_NO_INSTALL 1)
    set(GMOCK_NO_INSTALL 1)

    # This project also sets BUILD_SHARED_LIBS. We want to save what we initially
    # set.
    set(_us_build_shared ${BUILD_SHARED_LIBS})
    add_subdirectory(third_party/googletest)

    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/benchmark)

    set(BUILD_SHARED_LIBS ${_us_build_shared} CACHE BOOL "Build shared libraries" FORCE)
  endif()
endif()


#-----------------------------------------------------------------------------
# US C/CXX Flags
#-----------------------------------------------------------------------------

set(US_HAVE_VISIBILITY_ATTRIBUTE 0)
set(US_CXX11_FLAGS )
set(US_CXX_COVERAGE_FLAGS )

# Flags for all supported sanitizers
set(US_CXX_ASAN_FLAGS "")
set(US_ASAN_LINK_FLAGS "")
set(US_CXX_TSAN_FLAGS "")
set(US_TSAN_LINK_FLAGS "")
set(US_CXX_MSAN_FLAGS "")
set(US_MSAN_LINK_FLAGS "")
set(US_CXX_UBSAN_FLAGS "")
set(US_UBSAN_LINK_FLAGS "")

if(MSVC)
  # Since we are compiling with boost, there exists some code which looks
  # at the value of this define. Since we previously did not specify the
  # define, boost assumed a value (0x0601, meaning Windows 7). We will
  # now explicitly include this define to remove the compiler messages
  # it generates.
  set(msvc_version_define
    "_WIN32_WINNT=0x0601"
  )

  set(disabled_warnings
    4251 # 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
  )

  if (${MSVC_VERSION} LESS 1910)
    # For googletest on MSVC versions less than MSVC 2017
    # - (4503) 'identifier' : decorated name length exceeded, name was truncated
    # Additionally, on MSVC versions less than MSVC 2015
    # - (4996) all deprecation warnings (pragma push/pop for disabling the warning does not work)
    set(disabled_warnings ${disabled_warnings} 4503 4996)
  endif()

  foreach(wd ${disabled_warnings})
    set(US_CXX_FLAGS "/wd${wd} ${US_CXX_FLAGS}")
  endforeach()

  if (${US_ENABLE_ASAN} AND NOT "${US_ASAN_USER_DLL}" STREQUAL "")
    usFunctionCheckCompilerFlags("/fsanitize=address" US_CXX_ASAN_FLAGS)
    if(NOT US_CXX_ASAN_FLAGS)
      message(WARNING "ASAN requested, but compiler does not recognize it")
      set(US_ENABLE_ASAN OFF)
    else()
      set(US_CXX_FLAGS "${US_CXX_ASAN_FLAGS} ${US_CXX_FLAGS}")
    endif()
  elseif (${US_ENABLE_ASAN} AND "${US_ASAN_USER_DLL}" STREQUAL "")
    message(WARNING "ASAN requested, but user DLL was not specified")
    set(US_ENABLE_ASAN OFF)
  endif()

  set(US_CXX_FLAGS "/MP /WX /D${msvc_version_define} ${US_CXX_FLAGS}")

  # UTF-8 support
  # Differences in character sets will cause these sort orders to differ. For this
  # reason, we set the LC_ALL environment variable to 'C' with the utf-8 character set to ensure
  # sort ordering is correct.
  set(US_CXX_FLAGS "/utf-8 ${US_CXX_FLAGS}")
  set(US_C_FLAGS "/utf-8 ${US_C_FLAGS}")
else()

  # If not cross-compiling, turn on Stack Smashing Protection.
  # This is done in cases where libssp.so may not be available in the
  # cross build toolchain. The dependency with libssp.so was originally
  # discovered building with the Android ARM gcc toolchain. It is
  # being applied to any cross compilation as a precaution.
  if(NOT CMAKE_CROSSCOMPILING)
    usFunctionCheckCompilerFlags(-fstack-protector-all US_CXX_FLAGS)
  endif()

  foreach(_cxxflag  -Werror -Wall -Wextra -Wpointer-arith -Winvalid-pch -Wcast-align
          -Wwrite-strings -Woverloaded-virtual -Wnon-virtual-dtor -Wold-style-cast
          -Wstrict-null-sentinel -Wsign-promo -fdiagnostics-show-option )
    usFunctionCheckCompilerFlags(${_cxxflag} US_CXX_FLAGS)
  endforeach()

  # Force clang compiler to use deterministic timestamp when linking
  # shared libraries. In order to do so, we need to set the environment variable ZERO_AR_DATE to 1
  # before invoking the linker. To do that we use the CMAKE_CXX_LINKER_LAUNCHER variable to wrap the
  # launch of the linker in a shell script that sets that variable. Only do this for deterministic
  # builds. 
  
  if(US_COMPILER_APPLE_CLANG AND US_USE_DETERMINISTIC_BUNDLE_BUILDS)
    set(CMAKE_CXX_LINKER_LAUNCHER ${CMAKE_SOURCE_DIR}/tools/mac_deterministic_link_wrapper.sh)
  endif()

  if(US_COMPILER_GNU_ARM)
    # arm gcc warns that several reinterpret_cast need increased alignemnt in reinterpret_cast<>
    # but it appears to be warning due to unknow/unguaranteed alignment of char*
    usFunctionCheckCompilerFlags(-Wno-error=cast-align US_CXX_FLAGS)
  endif()

  if(US_COMPILER_CLANG OR US_COMPILER_APPLE_CLANG)
    # The check does not work reliably for gcc
    usFunctionCheckCompilerFlags(-Wno-mismatched-tags US_CXX_FLAGS)
  endif()

  # Issue #56: Workaround a clang compiler issue (https://llvm.org/bugs/show_bug.cgi?id=26749).
  # The typeinfo symbols do not have reliable symbol visibility. Instead of exporting only
  # symbols and associated typeinfo symbols which have explicit "default" visibility,
  # export all typeinfo symbols by default and maintain hidden symbol visibility for all other
  # symbols (unless explicitly overriden). See
  # https://developer.apple.com/library/mac/technotes/tn2185/_index.html#//apple_ref/doc/uid/DTS10004200-CH1-SUBSECTION2
  # to understand what -fvisibility-ms-compat does.
  if(US_COMPILER_CLANG OR US_COMPILER_APPLE_CLANG)
    usFunctionCheckCompilerFlags("-fvisibility-ms-compat" _have_visibility)
  else()
    usFunctionCheckCompilerFlags("-fvisibility=hidden -fvisibility-inlines-hidden" _have_visibility)
  endif()

  if(_have_visibility)
    set(US_HAVE_VISIBILITY_ATTRIBUTE 1)
    set(US_CXX_FLAGS "${US_CXX_FLAGS} ${_have_visibility}")
    set(US_C_FLAGS "${US_C_FLAGS} -fvisibility=hidden")
  endif()

  usFunctionCheckCompilerFlags("-O1 -D_FORTIFY_SOURCE=2" _fortify_source_flag)
  if(_fortify_source_flag)
    set(US_CXX_FLAGS_RELEASE "${US_CXX_FLAGS_RELEASE} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
  endif()

  # ================================================================================
  # Check compiler flag availability for all supported sanitizers
  # ================================================================================
  set(_orig_req_libs ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES "-fsanitize=address")
  usFunctionCheckCompilerFlags("-O1 -fsanitize=address -fno-omit-frame-pointer" US_CXX_ASAN_FLAGS)
  set(CMAKE_REQUIRED_LIBRARIES "${_orig_req_libs}")
  if(NOT US_CXX_ASAN_FLAGS AND US_ENABLE_ASAN)
    message(WARNING "ASAN requested, but compiler does not recognize it")
  endif()

  set(_orig_req_libs ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES "-fsanitize=thread")
  usFunctionCheckCompilerFlags("-O1 -fsanitize=thread" US_CXX_TSAN_FLAGS)
  set(CMAKE_REQUIRED_LIBRARIES "${_orig_req_libs}")
  if(NOT US_CXX_TSAN_FLAGS AND US_ENABLE_TSAN)
    message(WARNING "TSAN requested, but compiler does not recognize it")
  endif()

  set(_orig_req_libs ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES "-fsanitize=memory -fsanitize-memory-track-origins=2 -fPIE -fno-omit-frame-pointer")
  usFunctionCheckCompilerFlags("-fsanitize=memory -fsanitize-memory-track-origins=2 -fPIE -fno-omit-frame-pointer" US_CXX_MSAN_FLAGS)
  set(CMAKE_REQUIRED_LIBRARIES "${_orig_req_libs}")
  if(NOT US_CXX_MSAN_FLAGS AND US_ENABLE_MSAN)
    message(WARNING "MSAN requested, but compiler does not recognize it")
  endif()

  set(_orig_req_libs ${CMAKE_REQUIRED_LIBRARIES})
  set(CMAKE_REQUIRED_LIBRARIES "-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer")
  usFunctionCheckCompilerFlags("-fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer" US_CXX_UBSAN_FLAGS)
  set(CMAKE_REQUIRED_LIBRARIES "${_orig_req_libs}")
  if(NOT US_CXX_UBSAN_FLAGS AND US_ENABLE_UBSAN)
    message(WARNING "UBSAN requested, but compiler does not recognize it")
  endif()
  # ================================================================================

  set(CMAKE_REQUIRED_LIBRARIES "-fprofile-arcs -ftest-coverage")
  usFunctionCheckCompilerFlags("-fprofile-arcs -ftest-coverage" US_CXX_COVERAGE_FLAGS)
  set(CMAKE_REQUIRED_LIBRARIES "${_orig_req_libs}")
  if(NOT US_CXX_COVERAGE_FLAGS AND US_ENABLE_COVERAGE)
    message(WARNING "Code coverage requested, but compiler does not recognize it")
  endif()
endif()

# Allow forcing of bundle builds to be deterministic
if (US_USE_DETERMINISTIC_BUNDLE_BUILDS)
  set(US_CXX_FLAGS "${US_CXX_FLAGS} -DMINIZ_DETERMINISTIC_ZIP_FILES")
  set(US_C_FLAGS "${US_C_FLAGS} -DMINIZ_DETERMINISTIC_ZIP_FILES")
endif()

if(MINGW)
  # suppress warnings about auto imported symbols
  set(US_CXX_FLAGS "-Wl,--enable-auto-import ${US_CXX_FLAGS}")
  # we need to define a Windows version (Windows Vista)
  set(US_CXX_FLAGS "-D_WIN32_WINNT=0x0600 -DNTDDI_VERSION=0x06000000 ${US_CXX_FLAGS}")
  # silence ignored attributes warnings
  add_compile_options(-Wno-attributes)
endif()

# Include the selected sanitizer's flags in US_CXX_FLAGS if the compiler supports the required flags
# Note: TSAN not included here because TSAN should only be included if US_ENABLE_THREADING_SUPPORT
#       is turned on
foreach(_sanitizer_short_name ASAN;MSAN;UBSAN)
  if (US_ENABLE_${_sanitizer_short_name} AND US_CXX_${_sanitizer_short_name}_FLAGS)
    set(US_CXX_FLAGS "${US_CXX_FLAGS} ${US_CXX_${_sanitizer_short_name}_FLAGS}")
  endif()
endforeach()

if(US_ENABLE_THREADING_SUPPORT)
  if(NOT MINGW)
    set(THREADS_PREFER_PTHREAD_FLAG 1)
  endif()
  find_package(Threads REQUIRED)

  # Include TSAN flags in US_CXX_FLAGS if TSAN was requested and the compiler supports the flags
  if(US_ENABLE_TSAN AND US_CXX_TSAN_FLAGS)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(US_CXX_FLAGS "${US_CXX_FLAGS} ${US_CXX_TSAN_FLAGS}")
  endif()
endif()

if(US_ENABLE_COVERAGE AND US_CXX_COVERAGE_FLAGS)
  # US_CXX_COVERAGE_FLAGS is used for both the compiler and the linker
  set(US_CXX_FLAGS "${US_CXX_FLAGS} ${US_CXX_COVERAGE_FLAGS}")
endif()

#-----------------------------------------------------------------------------
# Compile libraries here if you do not want -Werror or /WX on
#-----------------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
add_subdirectory(third_party/boost/nowide)
set(BUILD_SHARED_LIBS ${_us_build_shared} CACHE BOOL "Build shared libraries" FORCE)
#-----------------------------------------------------------------------------

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${US_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${US_CXX_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${US_C_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${US_C_FLAGS_RELEASE}")

#-----------------------------------------------------------------------------
# US Link Flags
#-----------------------------------------------------------------------------

set(US_LINK_FLAGS)

if(CMAKE_COMPILER_IS_GNUCXX AND NOT APPLE)
  foreach(_linkflag -Wl,--no-undefined)
    set(_add_flag)
    usFunctionCheckCompilerFlags("${_linkflag}" _add_flag)
    if(_add_flag)
      set(US_LINK_FLAGS "${US_LINK_FLAGS} ${_linkflag}")
    endif()
  endforeach()
  if(US_COMPILER_GNU_ARM)
    # atomic intrinsics are in libatomic on ARM GCC
    set(US_LINK_FLAGS "${US_LINK_FLAGS} -latomic")
  endif()
endif()

if(NOT MSVC)
  # If clang is used and lld is the linker, specify -fuse-ld=lld
  if (US_COMPILER_CLANG AND US_USING_LLD)
    set(US_LINK_FLAGS "${US_LINK_FLAGS} -fuse-ld=lld")
  endif()

  if (US_ENABLE_ASAN AND US_CXX_ASAN_FLAGS)
    set(US_ASAN_LINK_FLAGS "-fsanitize=address")
  endif()

  if (US_ENABLE_TSAN AND US_CXX_TSAN_FLAGS)
    set(US_TSAN_LINK_FLAGS "-fsanitize=thread")
  endif()

  if (US_ENABLE_MSAN AND US_CXX_MSAN_FLAGS)
    set(US_MSAN_LINK_FLAGS "-fsanitize=memory")
  endif()

  if (US_ENABLE_UBSAN AND US_CXX_UBSAN_FLAGS)
    set(US_UBSAN_LINK_FLAGS "-fsanitize=undefined")
  endif()
else()
  # Force MSVC compiler to use deterministic timestamp when linking
  # shared libraries.
  if (US_USE_DETERMINISTIC_BUNDLE_BUILDS)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /Brepro" CACHE STRING "Forced shared linker flags" FORCE)
  endif()
endif()


# Update US_LINK_FLAGS with sanitizer link flags if one was requested
# Note: TSAN is individually checked because it should only be included
#       if US_ENABLE_THREADING_SUPPORT is turned on
if(US_ENABLE_THREADING_SUPPORT AND US_TSAN_LINK_FLAGS)
  set(US_LINK_FLAGS "${US_LINK_FLAGS} ${US_TSAN_LINK_FLAGS}")
endif()
foreach(_sanitizer_short_name ASAN;MSAN;UBSAN)
  if (US_${_sanitizer_short_name}_LINK_FLAGS)
    set(US_LINK_FLAGS "${US_LINK_FLAGS} ${US_${_sanitizer_short_name}_LINK_FLAGS}")
  endif()
endforeach()

if(US_ENABLE_COVERAGE AND US_CXX_COVERAGE_FLAGS)
  # US_CXX_COVERAGE_FLAGS is used for both the compiler and the linker
  set(US_LINK_FLAGS "${US_LINK_FLAGS} ${US_CXX_COVERAGE_FLAGS}")
endif()

usFunctionCheckResourceLinking()

# For building with Xcode, "CMAKE_*_LINKER_FLAGS" need to include US_LINK_FLAGS
# to include coverage flags "-fprofile-arcs" and "-ftest-coverage" if US_ENABLE_COVERAGE
# is set.
# Previously, macOS builds using Unix Makefiles automatically set compile flags
# to be used as linker flags (see the following CMake bug report for more details:
# https://cmake.org/Bug/view.php?id=15427), so linker flags didn't need to be set.

if(APPLE AND CMAKE_GENERATOR MATCHES "Xcode")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${US_LINK_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${US_LINK_FLAGS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${US_LINK_FLAGS}")
endif()

#-----------------------------------------------------------------------------
# Deterministic builds environment variables
#-----------------------------------------------------------------------------
if(US_USE_DETERMINISTIC_BUNDLE_BUILDS)
  # On windows, setlocale() is used to set these variables in the ResourceCompiler.
  # Differences in character sets will cause these sort orders to differ. For this
  # reason, we set the LC_ALL environment variable to 'C' so that sort ordering is consistent. 
  if(NOT MSVC)
    set(ENV{LC_ALL} "C.UTF-8")
  endif()
endif()

#-----------------------------------------------------------------------------
# US Header Checks
#-----------------------------------------------------------------------------

include(CheckIncludeFileCXX)

CHECK_INCLUDE_FILE_CXX(cxxabi.h US_HAVE_CXXABI_H)

#-----------------------------------------------------------------------------
# C++ language support
#-----------------------------------------------------------------------------

set(disabled_func )

file(GLOB try_compile_srcs ${CMAKE_CURRENT_SOURCE_DIR}/cmake/try_compile/*.cpp)
foreach(try_compile_src ${try_compile_srcs})
  get_filename_component(try_compile_var ${try_compile_src} NAME_WE)
  string(TOUPPER ${try_compile_var} try_compile_var)
  try_compile(
    ${try_compile_var}
    ${CMAKE_CURRENT_BINARY_DIR}/try_compile/${try_compile_var}
    SOURCES ${try_compile_src}
    CMAKE_FLAGS -DCMAKE_CXX_EXTENSIONS:BOOL=OFF -DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON -DCMAKE_CXX_STANDARD:STRING=${CMAKE_CXX_STANDARD}
    # The line below only works on CMake 3.8 and higher
    # CXX_STANDARD ${CMAKE_CXX_STANDARD}
  )
endforeach()

if(MINGW)
  #
  # The MinGW winpthread implementation does not properly destruct
  # non-POD thread-local objects, leading to segmentation faults on
  # thread exit. See
  #
  # https://github.com/Alexpux/MINGW-packages/issues/2519
  # https://sourceforge.net/p/mingw-w64/bugs/527/
  #
  set(US_HAVE_THREAD_LOCAL 0)
  set(disabled_func "${disabled_func}MinGW winpthread: ")
endif()

if(NOT US_HAVE_THREAD_LOCAL AND US_ENABLE_THREADING_SUPPORT)
  set(disabled_func "${disabled_func}thread_local support unavailable. Recursive service factory calls will not be detected. See also https://github.com/CppMicroServices/CppMicroServices/issues/213\n")
endif()

#-----------------------------------------------------------------------------
# System Information
#-----------------------------------------------------------------------------

include(TestBigEndian)

test_big_endian(US_BIG_ENDIAN)

if(NOT US_BIG_ENDIAN)
  set(US_LITTLE_ENDIAN 1)
endif()



#-----------------------------------------------------------------------------
# Source directory
#-----------------------------------------------------------------------------

# Configure CppMicroServicesConfig.cmake for the build tree.
# The file is used in sub-directories.

set(PACKAGE_CONFIG_RUNTIME_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(PACKAGE_CONFIG_CMAKE_DIR ${US_CMAKE_DIR})

set(US_RCC_EXECUTABLE_TARGET usResourceCompiler CACHE INTERNAL "The target name of the usResourceCompiler executable.")
set(US_RCC_EXECUTABLE_OUTPUT_NAME ${US_RCC_EXECUTABLE_TARGET}${US_GLOBAL_VERSION_SUFFIX})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  @ONLY
  )

set(global_config_h_file "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME_LOWER}/GlobalConfig.h")
set(US_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
configure_file(${US_CMAKE_DIR}/GlobalConfig.h.in ${global_config_h_file})

# Version information
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}ConfigVersion.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  @ONLY
  )

add_subdirectory(tools/rc)
add_subdirectory(tools/jsonschemavalidator)
add_subdirectory(tools/change_namespace)

export(TARGETS ${US_RCC_EXECUTABLE_TARGET}
       FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake)

add_subdirectory(util)
add_subdirectory(framework)
add_subdirectory(httpservice)
add_subdirectory(webconsole)
add_subdirectory(shellservice)

# Declarative Services requires threading to function.
# Static linkage is not yet supported for declarative services.
if (US_ENABLE_THREADING_SUPPORT AND US_BUILD_SHARED_LIBS)
  add_subdirectory(compendium)
else()
  add_subdirectory(compendium/LogService)
endif()

add_subdirectory(tools/shell)

#-----------------------------------------------------------------------------
# Documentation
#-----------------------------------------------------------------------------

add_subdirectory(doc)

#-----------------------------------------------------------------------------
# Installation
#-----------------------------------------------------------------------------

if(NOT US_NO_INSTALL)

install(EXPORT ${PROJECT_NAME}Targets
	FILE ${PROJECT_NAME}Targets.cmake
	DESTINATION ${AUXILIARY_CMAKE_INSTALL_DIR})

  set(_install_cmake_scripts
    ${US_BUNDLE_INIT_TEMPLATE}
    ${US_CMAKE_RESOURCE_DEPENDENCIES_CPP}
    ${US_CMAKE_DIR}/usFunctionGenerateBundleInit.cmake
    ${US_CMAKE_DIR}/usFunctionAddResources.cmake
    ${US_CMAKE_DIR}/usFunctionEmbedResources.cmake
    ${US_CMAKE_DIR}/usFunctionGetResourceSource.cmake
    ${US_CMAKE_DIR}/usFunctionCheckResourceLinking.cmake
    ${US_CMAKE_DIR}/usFunctionCheckCompilerFlags.cmake
    ${US_CMAKE_DIR}/usFunctionBoostPath.cmake
    )

  install(FILES ${_install_cmake_scripts}
	  DESTINATION ${AUXILIARY_CMAKE_INSTALL_DIR})

  install(FILES ${global_config_h_file}
	  DESTINATION ${HEADER_INSTALL_DIR}/${PROJECT_NAME_LOWER})

  # Configure CppMicroServicesConfig.cmake for the install tree
  set(CONFIG_INCLUDE_DIR ${HEADER_INSTALL_DIR})
  set(CONFIG_RUNTIME_DIR ${RUNTIME_INSTALL_DIR})
  set(CONFIG_CMAKE_DIR ${AUXILIARY_CMAKE_INSTALL_DIR})
  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in
    ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${AUXILIARY_CMAKE_INSTALL_DIR}
    PATH_VARS CONFIG_INCLUDE_DIR CONFIG_RUNTIME_DIR CONFIG_CMAKE_DIR
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${PROJECT_NAME}Config.cmake
		${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
	  DESTINATION ${AUXILIARY_CMAKE_INSTALL_DIR}
	  ${US_SDK_INSTALL_COMPONENT}
    )

endif()
