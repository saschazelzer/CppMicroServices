function(usFunctionCheckResourceLinking)
  if(NOT DEFINED US_RESOURCE_LINKING_AVAILABLE)
    set(_suffix )
    # Check linking capability
    set(_linking_available 0)
    if(APPLE)
      set(_result )
      # section name is "us_resources" because max length for section names in Mach-O format is 16 characters.
      usFunctionCheckCompilerFlags("-Wl,-sectcreate,__TEXT,us_resources,CMakeLists.txt" _result)
      if(_result)
        set(_linking_available 1)
      endif()
      set(_suffix .o)
    elseif(WIN32 AND CMAKE_RC_COMPILER)
      set(_linking_available 1)
      set(_suffix .rc)
    elseif(UNIX)
      if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} "-Wl,-r -Wl,-b binary -o" "${CMAKE_CURRENT_BINARY_DIR}/us_resource_link.o" "${CMAKE_COMMAND}"
        RESULT_VARIABLE _result
      )
      else()
      execute_process(
        COMMAND ${CMAKE_LINKER} -r -b binary -o "${CMAKE_CURRENT_BINARY_DIR}/us_resource_link.o" "${CMAKE_COMMAND}"
        RESULT_VARIABLE _result
      )
      endif()
      
      
      if(_result EQUAL 0)
        set(_linking_available 1)
      endif()
      set(_suffix .o)
    endif()

    set(US_RESOURCE_SOURCE_SUFFIX_LINK ${_suffix} CACHE INTERNAL "CppMicroServices resource source suffix (link)" FORCE)
    set(US_RESOURCE_SOURCE_SUFFIX_APPEND ".cpp" CACHE INTERNAL "CppMicroServices resource source suffix (append)" FORCE)

    set(_success "no")
    set(_default_mode "APPEND")
    if(_linking_available)
      set(_success "yes")
      if(APPLE)
        set(_default_mode "LINK")
       endif()
    endif()

    message(STATUS "---${CMAKE_LINKER}---")
    message(STATUS "---${CMAKE_CXX_LINK_EXECUTABLE}---")
    message("Checking for CppMicroServices resource linking capability...${_success}")

    set(US_RESOURCE_LINKING_AVAILABLE ${_linking_available} CACHE INTERNAL "CppMicroServices resource linking" FORCE)
    set(US_DEFAULT_RESOURCE_MODE ${_default_mode} CACHE INTERNAL "CppMicroServices default resource mode" FORCE)
    if(_default_mode STREQUAL "LINK")
      set(US_RESOURCE_SOURCE_SUFFIX ${US_RESOURCE_SOURCE_SUFFIX_LINK} CACHE INTERNAL "CppMicroServices resource source suffix" FORCE)
    else()
      set(US_RESOURCE_SOURCE_SUFFIX ${US_RESOURCE_SOURCE_SUFFIX_APPEND} CACHE INTERNAL "CppMicroServices resource source suffix" FORCE)
    endif()
  endif()
endfunction()
