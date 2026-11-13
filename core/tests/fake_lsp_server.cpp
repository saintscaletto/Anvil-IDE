#include <unistd.h>

#include <string>

#include "anvil/ipc/jsonrpc.hpp"
#include "anvil/lsp/transport.hpp"

int main() {
    anvil::lsp::Transport transport(STDIN_FILENO, STDOUT_FILENO);
    while (true) {
        anvil::Json message;
        std::string error;
        if (!transport.read_message(&message, &error)) break;
        const std::string method = message.value("method", "");
        const anvil::Json id = message.contains("id") ? message["id"] : anvil::Json();

        if (method == "initialize") {
            transport.write_message(anvil::ipc::make_result(
                id, anvil::Json{{"capabilities",
                                 {{"hoverProvider", true}, {"textDocumentSync", 2}}},
                                {"serverInfo", {{"name", "fake-lsp"}, {"version", "1.0"}}}}));
        } else if (method == "shutdown") {
            transport.write_message(anvil::ipc::make_result(id, nullptr));
        } else if (method == "exit") {
            break;
        } else if (method == "textDocument/hover") {
            transport.write_message(anvil::ipc::make_result(
                id, anvil::Json{{"contents",
                                 {{"kind", "markdown"}, {"value", "**fake hover**"}}}}));
        } else if (method == "textDocument/didOpen") {
            const anvil::Json params = message.value("params", anvil::Json::object());
            const std::string uri = params.value("textDocument", anvil::Json::object())
                                        .value("uri", std::string("file:///tmp/fake.cpp"));
            transport.write_message(anvil::ipc::make_notification(
                "textDocument/publishDiagnostics",
                anvil::Json{{"uri", uri},
                            {"diagnostics",
                             anvil::Json::array(
                                 {{{"range",
                                    {{"start", {{"line", 0}, {"character", 0}}},
                                     {"end", {{"line", 0}, {"character", 5}}}}},
                                   {"severity", 1},
                                   {"source", "fake"},
                                   {"message", "fake diagnostic"}}})}}));
        } else if (method == "initialized" || method == "$/cancelRequest" ||
                   method.empty()) {
            continue;
        } else if (message.contains("id")) {
            transport.write_message(anvil::ipc::make_error(
                id, anvil::ErrorCode::MethodNotFound, "fake server does not implement " + method));
        }
    }
    return 0;
}
