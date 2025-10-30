# cmake/libraries/yaml-cpp.cmake
# Added in OPM - Phase 2 Task 2.0
# Integrates yaml-cpp library for bot profile and behavior tree configuration

message(STATUS "Configuring yaml-cpp library")

# Option to use system yaml-cpp if available
option(USE_SYSTEM_YAML_CPP "Use system-installed yaml-cpp" OFF)

if(USE_SYSTEM_YAML_CPP)
    find_package(yaml-cpp REQUIRED)
    if(yaml-cpp_FOUND)
        message(STATUS "Using system yaml-cpp: ${yaml-cpp_VERSION}")
    endif()
else()
    # Fetch from GitHub
    include(FetchContent)

    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.7.0  # Pin to stable version
    )

    # Configure yaml-cpp options
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests")
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools")
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib")
    set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "Build yaml-cpp as static library")

    FetchContent_MakeAvailable(yaml-cpp)

    message(STATUS "yaml-cpp fetched and configured")
endif()

# Create interface target for easier linking
if(NOT TARGET yaml-cpp::yaml-cpp)
    add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()
