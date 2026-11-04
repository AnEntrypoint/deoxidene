cmake_minimum_required(VERSION 3.25)
project(deoxidene LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(deoxidene INTERFACE)
add_library(deoxidene::deoxidene ALIAS deoxidene)

target_include_directories(deoxidene INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
    $<INSTALL_INTERFACE:include>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(deoxidene INTERFACE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(deoxidene INTERFACE /W4 /permissive-)
endif()

option(DEOXIDENE_NATIVE_TUNE "Tune generated code for the build host CPU (-march=native); binaries are not portable" OFF)
if(DEOXIDENE_NATIVE_TUNE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(deoxidene INTERFACE -march=native)
endif()

option(DEOXIDENE_LTO "Link-time optimization for deoxidene executables when the toolchain supports it" OFF)
function(deoxidene_apply_lto target)
    if(NOT DEOXIDENE_LTO)
        return()
    endif()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT deoxidene_ipo_supported LANGUAGES CXX)
    if(deoxidene_ipo_supported)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endfunction()

option(DEOXIDENE_BUILD_EXAMPLES "Build deoxidene example programs" ON)
if(DEOXIDENE_BUILD_EXAMPLES)
    add_subdirectory(examples/hello)
endif()

option(DEOXIDENE_BUILD_TESTS "Build deoxidene tests (run with ctest)" OFF)
if(DEOXIDENE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(DEOXIDENE_BUILD_BENCH "Build deoxidene benchmark programs" OFF)
if(DEOXIDENE_BUILD_BENCH)
    add_subdirectory(bench)
endif()

include(GNUInstallDirs)
install(DIRECTORY include/deoxidene DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY third_party/tl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS deoxidene EXPORT deoxideneTargets INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT deoxideneTargets FILE deoxideneTargets.cmake NAMESPACE deoxidene:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/deoxidene)
