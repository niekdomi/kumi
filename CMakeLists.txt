cmake_minimum_required(VERSION 3.30)
project(
    kumi
    VERSION 0.1.0
    DESCRIPTION "C/C++ build system generator"
    LANGUAGES
        CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

# This warning is a false positive
if(CMAKE_TOOLCHAIN_FILE)
    mark_as_advanced(CMAKE_TOOLCHAIN_FILE)
endif()

# ------------------------------------------------------------------
# Compiler Flags
# ------------------------------------------------------------------

# Release optimizations
add_compile_options(
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-flto=full>
)

# Release link-time optimizations
add_link_options($<$<CONFIG:Release>:-flto=full>)

# Warning flags
add_compile_options(
    # Core
    -WCL4
    # -Werror
    -Wpedantic
    -Wdeprecated
    #
    # Safety
    -Wnon-virtual-dtor
    -Wsuggest-override
    -Wunsafe-buffer-usage
    -Wexperimental-lifetime-safety
    -Wconsumed
    -Wthread-safety
    -Wformat=2
    #
    # Type Safety & Performance
    -Wconversion
    -Wcast-align
    -Wold-style-cast
    -Wfloat-equal
    -Wdouble-promotion
    -Wcast-function-type
    -Wzero-as-null-pointer-constant
    #
    # Logical & Control Flow
    -Wimplicit-fallthrough
    -Wshadow-all
    -Wunreachable-code-aggressive
    -Wloop-analysis
    -Wcovered-switch-default
    #
    # Cross-Platform
    -Wpoison-system-directories
    #
    # Ignore non-standard C++ features
    -Wno-gnu-statement-expression
    -Wno-gnu-case-range
)

# ------------------------------------------------------------------
# Project Configuration
# ------------------------------------------------------------------

message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "  Version:  ${PROJECT_VERSION}")
message(STATUS "  Build:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  Standard: C++${CMAKE_CXX_STANDARD}")
message(
    STATUS
    "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "  Cache:    ccache (${CCACHE_PROGRAM})")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

find_program(MOLD_LINKER mold)
if(MOLD_LINKER)
    message(STATUS "  Linker:   mold (${MOLD_LINKER})")
    add_link_options(-fuse-ld=mold)
else()
    message(STATUS "  Linker:   default")
endif()

find_package(cpr REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

# ------------------------------------------------------------------
# Main Executable Target
# ------------------------------------------------------------------

file(GLOB_RECURSE KUMI_SOURCES "src/*.cpp")
file(GLOB_RECURSE KUMI_HEADERS "src/*.hpp")

add_executable(kumi ${KUMI_SOURCES})
target_include_directories(kumi PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(kumi PRIVATE cpr::cpr)

# ------------------------------------------------------------------
# Test Executable Target
# ------------------------------------------------------------------

file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")

add_executable(kumi_tests ${TEST_SOURCES})
target_include_directories(
    kumi_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(kumi_tests PRIVATE Catch2::Catch2WithMain)
