cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(alpaca-cpp
    VERSION 0.1.0
    LANGUAGES CXX)

# Each package can be built independently so it can be shipped as its own vcpkg port.
# Turning BUILD_ALPACA_CORE off makes the satellites resolve alpaca-cpp via find_package.
option(BUILD_ALPACA_CORE              "Build alpaca-cpp (Trading + Market Data + JSON streams)" ON)
# Default OFF until the satellite libraries land (Phase 4 / Phase 5).
option(BUILD_ALPACA_BROKER            "Build alpaca-broker-cpp"                    OFF)
option(BUILD_ALPACA_OPTIONS_STREAMING "Build alpaca-options-streaming-cpp"         OFF)
option(BUILD_ALPACA_TESTS             "Build tests for the enabled packages"       ${PROJECT_IS_TOP_LEVEL})
option(BUILD_ALPACA_EXAMPLES          "Build examples for the enabled packages"    ${PROJECT_IS_TOP_LEVEL})

if (CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_definitions(-DNDEBUG)
endif()

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00)      # Windows 10
endif()

# Suppresses the ZERO_CHECK project. Only valid for the Visual Studio generator: under
# Ninja it also removes the rule that regenerates build.ninja, so CMakeLists.txt edits
# would be silently ignored.
if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_SUPPRESS_REGENERATION true)
endif()

# ---------------------------------------------------------------------------
# Shared helper: apply the project-wide warning/optimization flags to a target.
# ---------------------------------------------------------------------------
function(alpaca_set_target_options tgt)
    if (MSVC)
        target_compile_options(${tgt} PRIVATE
            /MP
            /FS
            /bigobj
            /W4
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/GL>            # whole program optimization
        )
        target_link_options(${tgt} PRIVATE
            $<$<CONFIG:Debug>:/DEBUG:FASTLINK>
        )
        set_target_properties(${tgt} PROPERTIES LINK_INCREMENTAL ON)
    else()
        target_compile_options(${tgt} PRIVATE
            -Wall -Wextra -Wpedantic
            $<$<CONFIG:Release>:-O3>
        )
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Shared helper: generate + install the CMake package config for one library.
# ---------------------------------------------------------------------------
include(CMakePackageConfigHelpers)

function(alpaca_install_package tgt)
    install(TARGETS ${tgt} EXPORT ${tgt}Targets)

    install(EXPORT ${tgt}Targets
        FILE ${tgt}Targets.cmake
        NAMESPACE slick::
        DESTINATION lib/cmake/${tgt}
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cmake/${tgt}Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${tgt}Config.cmake
        INSTALL_DESTINATION lib/cmake/${tgt}
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${tgt}ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${tgt}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${tgt}ConfigVersion.cmake
        DESTINATION lib/cmake/${tgt}
    )
endfunction()

# ---------------------------------------------------------------------------
# Dependencies shared by every package.
# ---------------------------------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)

find_package(slick-net 3.1.0 CONFIG QUIET)
if (NOT slick-net_FOUND)
    message(STATUS "Fetching slick-net...")
    include(FetchContent)
    set(BUILD_SLICK_NET_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SLICK_NET_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        slick-net
        GIT_REPOSITORY https://github.com/SlickQuant/slick-net.git
        GIT_TAG v3.1.0
    )
    FetchContent_MakeAvailable(slick-net)
else()
    message(STATUS "Found slick-net ${slick-net_VERSION}: ${slick-net_DIR}")
endif()

# ---------------------------------------------------------------------------
# Packages
# ---------------------------------------------------------------------------
if (BUILD_ALPACA_CORE)
    add_subdirectory(core)
endif()

if (BUILD_ALPACA_BROKER)
    add_subdirectory(broker)
endif()

if (BUILD_ALPACA_OPTIONS_STREAMING)
    add_subdirectory(options-streaming)
endif()

if (BUILD_ALPACA_TESTS)
    message(STATUS "Building alpaca-cpp tests")
    enable_testing()
    add_subdirectory(tests)
else()
    message(STATUS "Skipping alpaca-cpp tests")
endif()

if (BUILD_ALPACA_EXAMPLES)
    message(STATUS "Building alpaca-cpp examples")
    add_subdirectory(examples)
else()
    message(STATUS "Skipping alpaca-cpp examples")
endif()

message(STATUS "${PROJECT_NAME}: ${PROJECT_VERSION}")
message(STATUS "  core               : ${BUILD_ALPACA_CORE}")
message(STATUS "  broker             : ${BUILD_ALPACA_BROKER}")
message(STATUS "  options-streaming  : ${BUILD_ALPACA_OPTIONS_STREAMING}")
