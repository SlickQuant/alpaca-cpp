# The in-tree target when alpaca-cpp is built alongside, the installed package otherwise —
# which is the configuration a vcpkg port of this library uses.
if (NOT TARGET slick::alpaca-cpp)
    find_package(alpaca-cpp CONFIG REQUIRED)
endif()

# msgpack-cxx is msgpack's C++ header-only library, the vcpkg port `msgpack`. The port named
# `msgpack-c` is the separate C library: it ships none of the C++ headers the decoder uses, and
# the two ports cannot be installed side by side.
find_package(msgpack-cxx CONFIG QUIET)
if (msgpack-cxx_FOUND)
    message(STATUS "Found msgpack-cxx ${msgpack-cxx_VERSION}: ${msgpack-cxx_DIR}")
else()
    find_package(msgpack-c CONFIG QUIET)
    if (msgpack-c_FOUND)
        message(WARNING "Found msgpack-c, msgpack's C library, but not msgpack-cxx, which "
                        "alpaca-options-streaming-cpp needs. With vcpkg: `vcpkg remove "
                        "msgpack-c` then `vcpkg install msgpack`. Fetching msgpack-cxx instead.")
    endif()
    message(STATUS "Fetching msgpack-cxx...")
    include(FetchContent)
    # msgpack-cxx is header-only and only its headers are wanted. SOURCE_SUBDIR names a
    # directory with no CMakeLists.txt, so the sources are populated without running
    # msgpack-c's own build: that would look for Boost and add its install rules to ours.
    FetchContent_Declare(
        msgpack-cxx
        GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
        GIT_TAG cpp-9.0.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR include
    )
    FetchContent_MakeAvailable(msgpack-cxx)

    add_library(msgpack-cxx INTERFACE IMPORTED)
    target_include_directories(msgpack-cxx INTERFACE ${msgpack-cxx_SOURCE_DIR}/include)
    target_compile_definitions(msgpack-cxx INTERFACE MSGPACK_NO_BOOST)
endif()

add_library(alpaca-options-streaming-cpp STATIC
    src/option_data_stream.cpp
)
add_library(slick::alpaca-options-streaming-cpp ALIAS alpaca-options-streaming-cpp)

target_include_directories(alpaca-options-streaming-cpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(alpaca-options-streaming-cpp PUBLIC slick::alpaca-cpp)

# msgpack is used inside the library only and is header-only, so nothing of it has to
# reach a consumer. BUILD_INTERFACE keeps it out of the exported target as well: a plain
# PRIVATE link on a static library is still exported as $<LINK_ONLY:msgpack-cxx>, which
# would make every consumer find msgpack just to resolve the name.
target_link_libraries(alpaca-options-streaming-cpp PRIVATE $<BUILD_INTERFACE:msgpack-cxx>)

alpaca_set_target_options(alpaca-options-streaming-cpp)

install(DIRECTORY include/ DESTINATION include)
alpaca_install_package(alpaca-options-streaming-cpp)
