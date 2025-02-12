set(US_CMAKE_MINIMUM_REQUIRED_VERSION 3.17)
project(change_namespace)

enable_language(CXX)

include_directories(../../third_party)

set(srcs main.cpp "FileHandler.cpp" "PathUtility.cpp" "AddPaths.cpp" "CopyPaths.cpp" "ChangeNamespaceImpl.cpp")

add_executable(change_namespace ${srcs})
set_property(TARGET change_namespace PROPERTY OUTPUT_NAME change_namespace)

if(NOT US_NO_INSTALL)
    install(TARGETS change_namespace
            EXPORT ${PROJECT_NAME}Targets
            FRAMEWORK DESTINATION . ${US_SDK_INSTALL_COMPONENT}
            RUNTIME DESTINATION ${TOOLS_INSTALL_DIR} ${US_SDK_INSTALL_COMPONENT})
endif()

include_directories(${PROJECT_BINARY_DIR}/include)
add_subdirectory(test)

if (US_BUILD_TESTING)
  add_compile_options(-DUSING_GTEST)
endif()