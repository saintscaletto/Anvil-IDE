cmake_minimum_required(VERSION 3.20)

project(anvil VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Never bake local sysroot or build-tree paths into shipped binaries.
set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "")

option(ANVIL_BUILD_SHELL "Build the native WebKitGTK shell" ON)
option(ANVIL_BUILD_TESTS "Build the test suite" ON)
option(ANVIL_DEV_MODE "Allow explicit unsafe development fallbacks" OFF)
option(ANVIL_WERROR "Treat warnings as errors" ON)
option(ANVIL_LTO "Enable link-time optimisation in release builds" ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(anvil_hardening INTERFACE)
target_compile_options(anvil_hardening INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
    -fstack-protector-strong
    $<$<CONFIG:Release,RelWithDebInfo>:-D_FORTIFY_SOURCE=3>
)
if(ANVIL_WERROR)
    target_compile_options(anvil_hardening INTERFACE -Werror)
endif()
target_compile_options(anvil_hardening INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-fPIE>)
target_link_options(anvil_hardening INTERFACE
    -pie
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,-z,noexecstack
)

if(ANVIL_DEV_MODE)
    add_compile_definitions(ANVIL_DEV_MODE=1)
else()
    add_compile_definitions(ANVIL_DEV_MODE=0)
endif()

add_subdirectory(core)

if(ANVIL_BUILD_SHELL)
    add_subdirectory(shell)
endif()

if(ANVIL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(core/tests)
endif()
