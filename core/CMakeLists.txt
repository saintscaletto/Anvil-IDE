find_package(nlohmann_json 3.11 REQUIRED)
find_package(spdlog 1.12 REQUIRED)
find_package(Threads REQUIRED)

add_library(anvil_core_lib STATIC
    src/git_branch.cpp
    src/recent.cpp
    src/settings.cpp
    src/ipc/cancel.cpp
    src/ipc/jsonrpc.cpp
    src/ipc/server.cpp
    src/ipc/validate.cpp
    src/lsp/client.cpp
    src/lsp/manager.cpp
    src/lsp/transport.cpp
    src/lsp/uri.cpp
    src/security/sandbox.cpp
    src/security/seccomp.cpp
    src/util/fs_util.cpp
    src/util/log.cpp
    src/util/process.cpp
    src/util/strings.cpp
    src/util/thread_pool.cpp
    src/workspace/watcher.cpp
    src/workspace/workspace.cpp
)
target_include_directories(anvil_core_lib PUBLIC include)
target_link_libraries(anvil_core_lib PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
)
target_compile_features(anvil_core_lib PUBLIC cxx_std_20)
target_link_libraries(anvil_core_lib PUBLIC anvil_hardening)

if(ANVIL_LTO)
    set_property(TARGET anvil_core_lib PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

add_executable(anvil-core src/main.cpp)
target_link_libraries(anvil-core PRIVATE anvil_core_lib)
target_link_options(anvil-core PRIVATE -Wl,--gc-sections)
target_compile_options(anvil-core PRIVATE -ffunction-sections -fdata-sections)
if(ANVIL_LTO)
    set_property(TARGET anvil-core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
