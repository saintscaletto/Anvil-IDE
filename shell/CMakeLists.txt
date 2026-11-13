find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKIT2 REQUIRED IMPORTED_TARGET webkit2gtk-4.1)

add_executable(anvil src/main.cpp)
target_link_libraries(anvil PRIVATE PkgConfig::WEBKIT2 anvil_hardening)
target_compile_definitions(anvil PRIVATE ANVIL_VERSION="0.0.1")
target_compile_options(anvil PRIVATE -ffunction-sections -fdata-sections)
target_link_options(anvil PRIVATE -Wl,--gc-sections)
target_include_directories(anvil PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(ANVIL_LTO)
    set_property(TARGET anvil PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
