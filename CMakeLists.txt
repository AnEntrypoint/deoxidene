cmake_minimum_required(VERSION 3.25)
project(deoxidene LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(Boost CONFIG COMPONENTS container QUIET)
find_package(tl-expected CONFIG QUIET)

add_library(deoxidene INTERFACE)
add_library(deoxidene::deoxidene ALIAS deoxidene)

target_include_directories(deoxidene INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(deoxidene INTERFACE Microsoft.GSL::GSL)
if(Boost_FOUND)
    target_link_libraries(deoxidene INTERFACE Boost::container)
endif()
if(tl-expected_FOUND)
    target_link_libraries(deoxidene INTERFACE tl::expected)
    target_compile_definitions(deoxidene INTERFACE DEOXIDENE_HAS_TL_EXPECTED=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(deoxidene INTERFACE -Wall -Wextra -Wpedantic)
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

option(DEOXIDENE_BUILD_BENCH "Build deoxidene benchmark programs" OFF)
if(DEOXIDENE_BUILD_BENCH)
    add_subdirectory(bench)
endif()

include(GNUInstallDirs)
install(DIRECTORY include/deoxidene DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS deoxidene EXPORT deoxideneTargets)
install(EXPORT deoxideneTargets FILE deoxideneTargets.cmake NAMESPACE deoxidene:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/deoxidene)
