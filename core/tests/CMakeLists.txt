find_package(GTest 1.14 REQUIRED)
include(GoogleTest)

add_executable(anvil_fake_lsp fake_lsp_server.cpp)
target_link_libraries(anvil_fake_lsp PRIVATE anvil_core_lib)

add_executable(anvil_core_tests
    test_fs_util.cpp
    test_jsonrpc.cpp
    test_line_reader.cpp
    test_lsp_client.cpp
    test_lsp_transport.cpp
    test_recent.cpp
    test_redact.cpp
    test_sandbox.cpp
    test_seccomp.cpp
    test_server.cpp
    test_strings.cpp
    test_validate.cpp
    test_workspace.cpp
    test_watcher.cpp
)
target_link_libraries(anvil_core_tests PRIVATE anvil_core_lib GTest::gtest_main)
target_compile_definitions(anvil_core_tests PRIVATE
    ANVIL_FAKE_LSP_PATH="$<TARGET_FILE:anvil_fake_lsp>"
    ANVIL_CORE_PATH="$<TARGET_FILE:anvil-core>"
)
add_dependencies(anvil_core_tests anvil_fake_lsp anvil-core)
gtest_discover_tests(anvil_core_tests DISCOVERY_TIMEOUT 30)
