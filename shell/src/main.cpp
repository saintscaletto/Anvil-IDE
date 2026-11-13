#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <jsc/jsc.h>

#include <cairo.h>
#include <fontconfig/fontconfig.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

constexpr const char* kHandlerName = "anvil";
constexpr std::size_t kMaxJsMessageBytes = 32U * 1024U * 1024U;
constexpr std::size_t kMaxPendingJsMessages = 600;

struct ShellOptions {
    bool smoke_test = false;
    bool e2e = false;
    bool dev_mode = false;
    std::string screenshot_path;
    int screenshot_delay_seconds = 4;
    std::string workspace;
    std::string e2e_file;
    int e2e_hover_line = 0;
    int e2e_hover_character = 0;
    std::string ui_dir;
    std::string core_path;
    std::string lang;
};

// The UI resolves "auto" from this value. LC_ALL wins over LC_MESSAGES and
// LANG, matching POSIX precedence. ANVIL_LANG and --lang exist for tests and
// deterministic screenshots.
std::string system_locale(const ShellOptions& options) {
    std::string value = options.lang;
    for (const char* name : {"ANVIL_LANG", "LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (!value.empty()) break;
        const char* env = std::getenv(name);
        if (env != nullptr && *env != '\0') value = env;
    }
    if (value.empty()) return "en";
    std::string normalized;
    normalized.reserve(std::min<std::size_t>(value.size(), 64));
    for (const unsigned char c : value) {
        if (normalized.size() >= 64) break;
        if (c < 0x20 || c == 0x7f) continue;
        normalized.push_back(static_cast<char>(std::tolower(c)));
    }
    return normalized.empty() ? "en" : normalized;
}

std::filesystem::path shell_config_dir() {
    if (const char* env = std::getenv("ANVIL_CONFIG_DIR"); env != nullptr && *env != '\0') {
        return env;
    }
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "anvil";
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "anvil";
    }
    return {};
}

// The UI renders before the core has answered system/info. Reading only the
// persisted theme id here lets the first frame use the user's theme instead of
// flashing the default. The value is an allowlisted enum; the core remains the
// owner of the setting and re-validates every write.
std::string initial_ui_theme() {
    const std::filesystem::path path = shell_config_dir() / "settings.json";
    std::ifstream input(path, std::ios::binary);
    if (!input) return "auto";
    std::string content((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    if (content.size() > 256U * 1024U) return "auto";
    const Json parsed = Json::parse(content, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return "auto";
    const auto item = parsed.find("ui.theme");
    if (item == parsed.end() || !item->is_string()) return "auto";
    const std::string theme = item->get<std::string>();
    if (theme == "auto" || theme == "dark" || theme == "light" || theme == "high-contrast") {
        return theme;
    }
    return "auto";
}

// Enumerates installed font families through fontconfig. This is read-only and
// bounded: at most 300 unique families are returned, sorted by name. The UI
// treats the result as suggestions; free-text family names are still allowed.
Json list_font_families() {
    Json families = Json::array();
    if (FcInit() == FcFalse) {
        return Json{{"families", families}, {"truncated", false}};
    }
    FcPattern* pattern = FcPatternCreate();
    FcObjectSet* object_set =
        FcObjectSetBuild(FC_FAMILY, FC_SPACING, static_cast<char*>(nullptr));
    if (pattern == nullptr || object_set == nullptr) {
        if (object_set != nullptr) FcObjectSetDestroy(object_set);
        if (pattern != nullptr) FcPatternDestroy(pattern);
        return Json{{"families", families}, {"truncated", false}};
    }
    std::set<std::string> seen;
    std::vector<std::pair<std::string, bool>> entries;
    bool truncated = false;
    if (FcFontSet* fonts = FcFontList(nullptr, pattern, object_set); fonts != nullptr) {
        for (int index = 0; index < fonts->nfont; ++index) {
            FcChar8* family = nullptr;
            if (FcPatternGetString(fonts->fonts[index], FC_FAMILY, 0, &family) != FcResultMatch ||
                family == nullptr) {
                continue;
            }
            const std::string name(reinterpret_cast<const char*>(family));
            if (name.empty() || name.size() > 128) continue;
            if (!seen.insert(name).second) continue;
            if (entries.size() >= 300) {
                truncated = true;
                break;
            }
            int spacing = FC_PROPORTIONAL;
            const bool monospace =
                FcPatternGetInteger(fonts->fonts[index], FC_SPACING, 0, &spacing) ==
                    FcResultMatch &&
                (spacing == FC_MONO || spacing == FC_DUAL || spacing == FC_CHARCELL);
            entries.emplace_back(name, monospace);
        }
        FcFontSetDestroy(fonts);
    }
    FcObjectSetDestroy(object_set);
    FcPatternDestroy(pattern);
    std::sort(entries.begin(), entries.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });
    for (const auto& [family, monospace] : entries) {
        families.push_back(Json{{"family", family}, {"monospace", monospace}});
    }
    return Json{{"families", families}, {"truncated", truncated}};
}

struct WriteTask {
    std::string data;
};

struct ShellApp {
    ShellOptions options;
    GtkApplication* application = nullptr;
    GtkWindow* window = nullptr;
    WebKitWebView* web_view = nullptr;
    WebKitUserContentManager* content_manager = nullptr;
    GSubprocess* core = nullptr;
    GOutputStream* core_stdin = nullptr;
    GDataInputStream* core_stdout = nullptr;
    GDataInputStream* core_stderr = nullptr;
    GCancellable* cancellable = nullptr;

    bool page_ready = false;
    bool js_ready = false;
    bool shutting_down = false;
    bool allow_close = false;
    bool ready_received = false;
    bool screenshot_taken = false;
    int exit_code = 0;

    std::deque<std::string> js_queue;
    std::deque<std::string> write_queue;
    bool writing = false;

    std::string initial_workspace;
    int core_generation = 0;
    std::set<std::string> allowed_workspaces;

    void allow_workspace(const std::string& path) {
        std::error_code ec;
        const std::filesystem::path canonical =
            std::filesystem::canonical(path, ec);
        if (!ec && std::filesystem::is_directory(canonical)) {
            allowed_workspaces.insert(canonical.string());
        }
    }

    bool workspace_allowed(const std::string& path, std::string* reason) {
        std::error_code ec;
        const std::filesystem::path canonical =
            std::filesystem::canonical(path, ec);
        if (ec || !std::filesystem::is_directory(canonical)) {
            if (reason != nullptr) *reason = "Workspace is not an accessible directory";
            return false;
        }
        if (allowed_workspaces.count(canonical.string()) != 0) return true;

        // A user-approved recent project is also allowed. The recent file is
        // read-only user configuration, bounded here, and every path is
        // canonicalised and checked to be a directory before use.
        const std::filesystem::path recent_path = shell_config_dir() / "recent.json";
        std::ifstream input(recent_path, std::ios::binary);
        if (input) {
            std::string content((std::istreambuf_iterator<char>(input)),
                                std::istreambuf_iterator<char>());
            if (content.size() <= 256U * 1024U) {
                const Json recent = Json::parse(content, nullptr, false);
                if (recent.is_array()) {
                    for (const auto& item : recent) {
                        if (!item.is_object() || !item.contains("path") ||
                            !item["path"].is_string()) {
                            continue;
                        }
                        std::error_code item_ec;
                        const auto item_path = std::filesystem::canonical(
                            item["path"].get<std::string>(), item_ec);
                        if (!item_ec && std::filesystem::is_directory(item_path)) {
                            allowed_workspaces.insert(item_path.string());
                        }
                    }
                }
            }
        }
        if (allowed_workspaces.count(canonical.string()) != 0) return true;
        if (reason != nullptr) {
            *reason = "Workspace must be chosen through the native folder dialog, a recent project, or the command line";
        }
        return false;
    }
    std::string base_uri;
    guint timeout_id = 0;
    guint screenshot_timer_id = 0;
};

std::string json_dump(const Json& value) { return value.dump(); }

void evaluate_js(ShellApp* app, const std::string& script);

void send_js(ShellApp* app, const Json& message) {
    std::string script = "window.__anvilReceive(" + json_dump(message) + ");";
    if (!app->page_ready) {
        if (app->js_queue.size() < kMaxPendingJsMessages) {
            app->js_queue.push_back(std::move(script));
        }
        return;
    }
    evaluate_js(app, script);
}

void send_js_response(ShellApp* app, const Json& id, Json result) {
    send_js(app, Json{{"type", "response"}, {"id", id}, {"result", std::move(result)}});
}

void send_js_error(ShellApp* app, const Json& id, int code, const std::string& message) {
    send_js(app, Json{{"type", "response"},
                      {"id", id},
                      {"error", {{"code", code}, {"message", message}}}});
}

void send_js_event(ShellApp* app, const std::string& method, Json params) {
    send_js(app, Json{{"type", "event"}, {"method", method}, {"params", std::move(params)}});
}

void evaluate_js(ShellApp* app, const std::string& script) {
    if (app->web_view == nullptr) return;
    webkit_web_view_evaluate_javascript(app->web_view, script.c_str(),
                                        static_cast<gssize>(script.size()), nullptr, nullptr,
                                        nullptr, nullptr, nullptr);
}

void flush_js_queue(ShellApp* app) {
    while (!app->js_queue.empty()) {
        std::string script = std::move(app->js_queue.front());
        app->js_queue.pop_front();
        evaluate_js(app, script);
    }
}

std::string detect_ui_dir(const ShellOptions& options) {
    if (!options.ui_dir.empty()) return options.ui_dir;
    if (const char* env = std::getenv("ANVIL_UI_DIR"); env != nullptr && *env != '\0') {
        return env;
    }
    const fs::path installed = "/usr/share/anvil/ui";
    if (fs::exists(installed / "index.html")) return installed.string();

    std::error_code ec;
    const fs::path executable = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        const fs::path directory = executable.parent_path();
        for (const fs::path& candidate : {directory / "ui", directory / "../share/anvil/ui",
                                          directory / "../../apps/desktop/dist"}) {
            if (fs::exists(candidate / "index.html")) {
                return fs::weakly_canonical(candidate, ec).string();
            }
        }
    }
    return installed.string();
}

std::string detect_core_path(const ShellOptions& options) {
    if (!options.core_path.empty()) return options.core_path;
    if (const char* env = std::getenv("ANVIL_CORE_PATH"); env != nullptr && *env != '\0') {
        return env;
    }
    std::error_code ec;
    const fs::path executable = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        const fs::path sibling = executable.parent_path() / "anvil-core";
        if (fs::exists(sibling)) return sibling.string();
    }
    const fs::path installed = "/usr/lib/anvil/anvil-core";
    if (fs::exists(installed)) return installed.string();
    if (gchar* found = g_find_program_in_path("anvil-core"); found != nullptr) {
        std::string result = found;
        g_free(found);
        return result;
    }
    return {};
}

void write_next(ShellApp* app);

struct WriteContext {
    ShellApp* app;
    WriteTask* task;
};

void write_finished(GObject* source, GAsyncResult* result, gpointer user_data) {
    auto* context = static_cast<WriteContext*>(user_data);
    ShellApp* app = context->app;
    delete context->task;
    delete context;
    GError* error = nullptr;
    gsize written = 0;
    const gboolean ok = g_output_stream_write_all_finish(G_OUTPUT_STREAM(source), result,
                                                         &written, &error);
    (void)written;
    app->writing = false;
    if (!ok) {
        if (error != nullptr) {
            if (!app->shutting_down) {
                g_printerr("anvil: core stdin write failed: %s\n", error->message);
                send_js_event(app, "core:error",
                              Json{{"message", "lost the core connection"}});
            }
            g_error_free(error);
        }
        app->write_queue.clear();
        return;
    }
    write_next(app);
}

void write_next(ShellApp* app) {
    if (app->writing || app->shutting_down || app->core_stdin == nullptr) return;
    if (app->write_queue.empty()) return;
    app->writing = true;
    auto* task = new WriteTask{std::move(app->write_queue.front())};
    app->write_queue.pop_front();
    auto* context = new WriteContext{app, task};
    g_output_stream_write_all_async(app->core_stdin, task->data.data(), task->data.size(),
                                    G_PRIORITY_DEFAULT, app->cancellable, write_finished,
                                    context);
}

void enqueue_core_line(ShellApp* app, const std::string& line) {
    if (app->core_stdin == nullptr) return;
    if (app->write_queue.size() > 512) {
        send_js_event(app, "core:overloaded", Json::object());
        return;
    }
    app->write_queue.push_back(line + "\n");
    write_next(app);
}

void handle_core_line(ShellApp* app, const std::string& line, bool is_stderr) {
    if (is_stderr) {
        std::string safe = line;
        if (safe.size() > 8192) safe.resize(8192);
        send_js_event(app, "core:log", Json{{"line", safe}});
        return;
    }
    if (line.empty()) return;
    const Json message = Json::parse(line, nullptr, false);
    if (message.is_discarded() || !message.is_object()) {
        send_js_event(app, "core:protocolError", Json{{"message", "invalid core message"}});
        return;
    }
    if (message.contains("method") && message["method"].is_string()) {
        send_js_event(app, message["method"].get<std::string>(),
                      message.value("params", Json::object()));
        return;
    }
    if (!message.contains("id")) return;
    const Json id = message["id"];
    if (message.contains("error")) {
        send_js(app, Json{{"type", "response"}, {"id", id}, {"error", message["error"]}});
    } else {
        send_js_response(app, id, message.value("result", Json()));
    }
}

void read_core_line(ShellApp* app, GDataInputStream* stream, bool is_stderr, int generation);

struct ReadContext {
    ShellApp* app;
    bool is_stderr;
    int generation;
};

void core_line_ready(GObject* source, GAsyncResult* result, gpointer user_data) {
    auto* context = static_cast<ReadContext*>(user_data);
    ShellApp* app = context->app;
    const bool is_stderr = context->is_stderr;
    const int generation = context->generation;
    delete context;

    GError* error = nullptr;
    gsize length = 0;
    gchar* line = g_data_input_stream_read_line_finish(
        G_DATA_INPUT_STREAM(source), result, &length, &error);
    if (line == nullptr) {
        if (error != nullptr) {
            if (!app->shutting_down && error->code != G_IO_ERROR_CANCELLED) {
                g_printerr("anvil: core stream ended: %s\n", error->message);
            }
            g_error_free(error);
        }
        return;
    }
    if (generation == app->core_generation && !app->shutting_down) {
        handle_core_line(app, std::string(line, length), is_stderr);
    }
    g_free(line);
    if (generation == app->core_generation && !app->shutting_down) {
        read_core_line(app, G_DATA_INPUT_STREAM(source), is_stderr, generation);
    }
}

void read_core_line(ShellApp* app, GDataInputStream* stream, bool is_stderr, int generation) {
    if (app->shutting_down || stream == nullptr) return;
    auto* context = new ReadContext{app, is_stderr, generation};
    g_data_input_stream_read_line_async(stream, G_PRIORITY_DEFAULT, app->cancellable,
                                        core_line_ready, context);
}

void core_wait_finished(GObject* source, GAsyncResult* result, gpointer user_data) {
    auto* app = static_cast<ShellApp*>(user_data);
    if (app->core == nullptr || G_SUBPROCESS(source) != app->core) return;
    GError* error = nullptr;
    if (g_subprocess_wait_finish(G_SUBPROCESS(source), result, &error)) {
        if (g_subprocess_get_if_exited(G_SUBPROCESS(source))) {
            const int status = g_subprocess_get_exit_status(G_SUBPROCESS(source));
            if (!app->shutting_down && status != 0) {
                send_js_event(app, "core:exited", Json{{"code", status}});
            }
        } else if (g_subprocess_get_if_signaled(G_SUBPROCESS(source))) {
            const int signal = g_subprocess_get_term_sig(G_SUBPROCESS(source));
            if (!app->shutting_down) {
                send_js_event(app, "core:exited", Json{{"signal", signal}});
            }
        }
    } else {
        if (error != nullptr) g_error_free(error);
    }
}

void start_core(ShellApp* app) {
    ++app->core_generation;
    const int generation = app->core_generation;
    const std::string core_path = detect_core_path(app->options);
    if (core_path.empty() || !fs::exists(core_path)) {
        send_js_event(app, "core:error",
                      Json{{"message", "anvil-core was not found; set ANVIL_CORE_PATH"}});
        return;
    }
    std::vector<std::string> arguments{core_path, "--stdio"};
    if (app->options.dev_mode) arguments.push_back("--dev");
    std::vector<const gchar*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);

    GError* error = nullptr;
    app->core = g_subprocess_newv(
        argv.data(),
        static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_PIPE),
        &error);
    if (app->core == nullptr) {
        const std::string message = error != nullptr ? error->message : "unknown error";
        if (error != nullptr) g_error_free(error);
        send_js_event(app, "core:error", Json{{"message", "cannot start anvil-core: " + message}});
        return;
    }
    app->core_stdin = g_subprocess_get_stdin_pipe(app->core);
    GInputStream* stdout_pipe = g_subprocess_get_stdout_pipe(app->core);
    GInputStream* stderr_pipe = g_subprocess_get_stderr_pipe(app->core);
    app->core_stdout = g_data_input_stream_new(stdout_pipe);
    app->core_stderr = g_data_input_stream_new(stderr_pipe);
    g_object_unref(stdout_pipe);
    g_object_unref(stderr_pipe);
    g_data_input_stream_set_newline_type(app->core_stdout, G_DATA_STREAM_NEWLINE_TYPE_LF);
    g_data_input_stream_set_newline_type(app->core_stderr, G_DATA_STREAM_NEWLINE_TYPE_LF);

    read_core_line(app, app->core_stdout, false, generation);
    read_core_line(app, app->core_stderr, true, generation);
    g_subprocess_wait_async(app->core, app->cancellable, core_wait_finished, app);
    send_js_event(app, "core:started", Json{{"path", core_path}});
}

void stop_core(ShellApp* app) {
    ++app->core_generation;
    if (app->core == nullptr) return;
    if (app->cancellable != nullptr) g_cancellable_cancel(app->cancellable);
    if (app->core_stdin != nullptr) {
        g_output_stream_close(app->core_stdin, nullptr, nullptr);
        app->core_stdin = nullptr;
    }
    g_subprocess_force_exit(app->core);
    // Objects are intentionally released when the process exits. The window
    // close path must not block on a child that may be wedged; the core's
    // sandboxed children die through PDEATHSIG / bwrap --die-with-parent.
    app->core = nullptr;
    app->core_stdout = nullptr;
    app->core_stderr = nullptr;
    app->writing = false;
    app->write_queue.clear();
    app->cancellable = g_cancellable_new();
}

std::string choose_folder(ShellApp* app, const char* title) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title, app->window, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Cancel",
        GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, nullptr);
    std::string result;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename != nullptr) {
            result = filename;
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
    return result;
}

std::string choose_file(ShellApp* app, const char* title) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        title, app->window, GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT, nullptr);
    std::string result;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename != nullptr) {
            result = filename;
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
    return result;
}

void capture_screenshot(ShellApp* app);

void screenshot_ready(GObject* source, GAsyncResult* result, gpointer user_data) {
    auto* app = static_cast<ShellApp*>(user_data);
    GError* error = nullptr;
    cairo_surface_t* surface = webkit_web_view_get_snapshot_finish(WEBKIT_WEB_VIEW(source),
                                                                   result, &error);
    if (surface == nullptr) {
        g_printerr("anvil: screenshot failed: %s\n",
                   error != nullptr ? error->message : "unknown error");
        if (error != nullptr) g_error_free(error);
        app->exit_code = 1;
        g_application_quit(G_APPLICATION(app->application));
        return;
    }
    const cairo_status_t status =
        cairo_surface_write_to_png(surface, app->options.screenshot_path.c_str());
    cairo_surface_destroy(surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        g_printerr("anvil: cannot write screenshot: %s\n",
                   cairo_status_to_string(status));
        app->exit_code = 1;
    } else {
        std::printf("SCREENSHOT %s\n", app->options.screenshot_path.c_str());
        std::fflush(stdout);
    }
    app->screenshot_taken = true;
    g_application_quit(G_APPLICATION(app->application));
}

void capture_screenshot(ShellApp* app) {
    if (app->web_view == nullptr || app->screenshot_taken) return;
    webkit_web_view_get_snapshot(app->web_view, WEBKIT_SNAPSHOT_REGION_VISIBLE,
                                 WEBKIT_SNAPSHOT_OPTIONS_NONE, app->cancellable,
                                 screenshot_ready, app);
}

gboolean screenshot_timer(gpointer user_data) {
    auto* app = static_cast<ShellApp*>(user_data);
    app->screenshot_timer_id = 0;
    if (app->shutting_down) return G_SOURCE_REMOVE;
    capture_screenshot(app);
    return G_SOURCE_REMOVE;
}

void handle_ready(ShellApp* app, const Json& params) {
    if (app->ready_received) return;
    app->ready_received = true;
    // Ready means the startup watchdog must never fire later during normal use.
    if (app->timeout_id != 0) {
        g_source_remove(app->timeout_id);
        app->timeout_id = 0;
    }
    const bool ok = params.value("ok", true);
    if (!ok && app->options.smoke_test) {
        const std::string error = params.value("error", "UI did not become ready");
        std::printf("SMOKE_FAIL %s\n", error.c_str());
        std::fflush(stdout);
        app->exit_code = 1;
        g_application_quit(G_APPLICATION(app->application));
        return;
    }
    app->js_ready = true;
    flush_js_queue(app);
    if (app->options.e2e && app->options.e2e_hover_line > 0) {
        const std::string script =
            "window.__anvilTest && window.__anvilTest.hover(" +
            std::to_string(app->options.e2e_hover_line) + "," +
            std::to_string(app->options.e2e_hover_character) + ");";
        evaluate_js(app, script);
    }
    if (!app->options.screenshot_path.empty()) {
        const int delay = std::max(1, app->options.screenshot_delay_seconds);
        app->screenshot_timer_id =
            g_timeout_add_seconds(static_cast<guint>(delay), screenshot_timer, app);
        return;
    }
    if (app->options.smoke_test) {
        std::printf("SMOKE_OK\n");
        std::fflush(stdout);
        g_application_quit(G_APPLICATION(app->application));
    }
}

void handle_shell_message(ShellApp* app, const Json& message) {
    const std::string type = message.value("type", "");
    const std::string method = message.value("method", "");
    const Json id = message.value("id", Json());
    const Json params = message.value("params", Json::object());

    if (method == "shell/ready" && type == "notification") {
        handle_ready(app, params);
        return;
    }
    if (method == "shell/log" && type == "notification") {
        const std::string line = params.value("line", "");
        g_printerr("anvil-ui: %s\n", line.substr(0, 4096).c_str());
        return;
    }
    if (type != "request") return;

    if (method == "shell/info") {
        send_js_response(app, id,
                         Json{{"version", "0.0.1"},
                              {"native", true},
                              {"platform", "linux"},
                              {"uiDir", app->options.ui_dir},
                              {"core", app->options.core_path},
                              {"devMode", app->options.dev_mode}});
        return;
    }
    if (method == "shell/listFonts") {
        send_js_response(app, id, list_font_families());
        return;
    }
    if (method == "shell/openFolder") {
        const std::string path = choose_folder(app, "Open Folder");
        if (path.empty()) {
            send_js_response(app, id, Json{{"cancelled", true}});
        } else {
            app->allow_workspace(path);
            send_js_response(app, id, Json{{"path", path}});
        }
        return;
    }
    if (method == "shell/openFile") {
        const std::string path = choose_file(app, "Open File");
        if (path.empty()) {
            send_js_response(app, id, Json{{"cancelled", true}});
        } else {
            app->allow_workspace(std::filesystem::path(path).parent_path().string());
            send_js_response(app, id, Json{{"path", path}});
        }
        return;
    }
    if (method == "shell/setTitle") {
        const std::string title = params.value("title", "Anvil IDE");
        gtk_window_set_title(app->window, title.substr(0, 200).c_str());
        send_js_response(app, id, Json{{"ok", true}});
        return;
    }
    if (method == "shell/close") {
        app->allow_close = true;
        send_js_response(app, id, Json{{"ok", true}});
        gtk_window_close(app->window);
        return;
    }
    if (method == "shell/restartCore") {
        app->shutting_down = false;
        stop_core(app);
        start_core(app);
        send_js_response(app, id, Json{{"ok", true}});
        return;
    }
    send_js_error(app, id, -32601, "Unknown shell method: " + method);
}

void forward_to_core(ShellApp* app, const Json& message) {
    const std::string type = message.value("type", "");
    const std::string method = message.value("method", "");
    const Json params = message.value("params", Json::object());
    if (method.empty() || method.rfind("shell/", 0) == 0) {
        handle_shell_message(app, message);
        return;
    }
    const int api_version = message.value("apiVersion", 0);
    if (api_version != 1) {
        send_js_error(app, message.value("id", Json()), -32600,
                      "apiVersion 1 is required");
        return;
    }
    if (method == "workspace/open") {
        if (!params.is_object() || !params.contains("path") ||
            !params["path"].is_string()) {
            send_js_error(app, message.value("id", Json()), -32602,
                          "workspace/open requires a path");
            return;
        }
        std::string reason;
        if (!app->workspace_allowed(params["path"].get<std::string>(), &reason)) {
            send_js_error(app, message.value("id", Json()), -32002, reason);
            return;
        }
    }
    Json request{{"jsonrpc", "2.0"}, {"method", method}, {"params", params},
                 {"apiVersion", 1}};
    if (type == "request") {
        if (!message.contains("id")) {
            send_js_error(app, Json(), -32600, "request id is required");
            return;
        }
        request["id"] = message["id"];
        if (method == "workspace/open" && params.is_object() && params.contains("path") &&
            params["path"].is_string()) {
            app->initial_workspace = params["path"].get<std::string>();
        }
    } else if (type != "notification") {
        send_js_error(app, message.value("id", Json()), -32600, "unknown message type");
        return;
    }
    enqueue_core_line(app, request.dump());
}

void on_script_message(WebKitUserContentManager* manager, WebKitJavascriptResult* result,
                       gpointer user_data) {
    (void)manager;
    auto* app = static_cast<ShellApp*>(user_data);
    JSCValue* value = webkit_javascript_result_get_js_value(result);
    if (value == nullptr || !jsc_value_is_object(value)) return;
    gchar* json_text = jsc_value_to_json(value, 0);
    if (json_text == nullptr) return;
    const gsize length = std::strlen(json_text);
    if (length > kMaxJsMessageBytes) {
        g_printerr("anvil: oversized UI message ignored\n");
        g_free(json_text);
        return;
    }
    const Json message = Json::parse(json_text, nullptr, false);
    g_free(json_text);
    if (message.is_discarded() || !message.is_object()) {
        send_js_event(app, "core:protocolError", Json{{"message", "invalid UI message"}});
        return;
    }
    forward_to_core(app, message);
}

void on_load_changed(WebKitWebView* view, WebKitLoadEvent event, gpointer user_data) {
    (void)view;
    auto* app = static_cast<ShellApp*>(user_data);
    if (event == WEBKIT_LOAD_FINISHED) {
        app->page_ready = true;
        flush_js_queue(app);
        gtk_widget_show_all(GTK_WIDGET(app->window));
    }
}

gboolean on_decide_policy(WebKitWebView* view, WebKitPolicyDecision* decision,
                          WebKitPolicyDecisionType decision_type, gpointer user_data) {
    (void)view;
    auto* app = static_cast<ShellApp*>(user_data);
    if (decision_type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
        decision_type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        return FALSE;
    }
    WebKitNavigationAction* action =
        webkit_navigation_policy_decision_get_navigation_action(
            WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    if (action == nullptr) {
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }
    WebKitURIRequest* request = webkit_navigation_action_get_request(action);
    const char* uri = request != nullptr ? webkit_uri_request_get_uri(request) : nullptr;
    if (uri != nullptr && app->base_uri == uri) return FALSE;
    webkit_policy_decision_ignore(decision);
    return TRUE;
}

GtkWidget* on_create(WebKitWebView* view, WebKitNavigationAction* action, gpointer user_data) {
    (void)view;
    (void)action;
    (void)user_data;
    return nullptr;
}

gboolean on_permission_request(WebKitWebView* view, WebKitPermissionRequest* request,
                               gpointer user_data) {
    (void)view;
    (void)user_data;
    webkit_permission_request_deny(request);
    return TRUE;
}

gboolean on_context_menu(WebKitWebView* view, WebKitContextMenu* menu, GdkEvent* event,
                         WebKitHitTestResult* hit_test, gpointer user_data) {
    (void)view;
    (void)menu;
    (void)event;
    (void)hit_test;
    (void)user_data;
    return TRUE;
}

void on_download_started(WebKitWebContext* context, WebKitDownload* download,
                         gpointer user_data) {
    (void)context;
    (void)user_data;
    webkit_download_cancel(download);
}

void on_web_process_terminated(WebKitWebView* view, WebKitWebProcessTerminationReason reason,
                               gpointer user_data) {
    (void)view;
    auto* app = static_cast<ShellApp*>(user_data);
    const char* text = reason == WEBKIT_WEB_PROCESS_CRASHED ? "web process crashed"
                                                             : "web process terminated";
    send_js_event(app, "ui:processTerminated", Json{{"reason", text}});
}

gboolean on_delete_event(GtkWidget* widget, GdkEvent* event, gpointer user_data) {
    (void)widget;
    (void)event;
    auto* app = static_cast<ShellApp*>(user_data);
    if (app->allow_close || app->shutting_down) return FALSE;
    send_js_event(app, "shell:closeRequested", Json::object());
    return TRUE;
}

void on_window_destroy(GtkWidget* widget, gpointer user_data) {
    (void)widget;
    auto* app = static_cast<ShellApp*>(user_data);
    app->shutting_down = true;
    if (app->timeout_id != 0) {
        g_source_remove(app->timeout_id);
        app->timeout_id = 0;
    }
    app->web_view = nullptr;
    app->window = nullptr;
    if (app->cancellable != nullptr) g_cancellable_cancel(app->cancellable);
    stop_core(app);
    g_application_quit(G_APPLICATION(app->application));
}

gboolean startup_timeout(gpointer user_data) {
    auto* app = static_cast<ShellApp*>(user_data);
    app->timeout_id = 0;
    std::printf("SMOKE_TIMEOUT\n");
    std::fflush(stdout);
    app->exit_code = 1;
    g_application_quit(G_APPLICATION(app->application));
    return G_SOURCE_REMOVE;
}

void on_activate(GtkApplication* application, gpointer user_data) {
    auto* app = static_cast<ShellApp*>(user_data);
    app->application = application;

    app->window = GTK_WINDOW(gtk_application_window_new(application));
    gtk_window_set_title(app->window, "Anvil IDE");
    gtk_window_set_default_size(app->window, 1400, 900);
    gtk_window_set_position(app->window, GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(app->window, "anvil");

    app->content_manager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(app->content_manager,
                                                                kHandlerName);
    g_signal_connect(app->content_manager, "script-message-received::anvil",
                     G_CALLBACK(on_script_message), app);

    Json boot{{"native", true},
              {"version", "0.0.1"},
              {"e2e", app->options.e2e},
              {"devMode", app->options.dev_mode},
              {"initialWorkspace", app->options.workspace},
              {"e2eFile", app->options.e2e_file},
              {"theme", initial_ui_theme()},
              {"locale", system_locale(app->options)}};
    std::string bootstrap =
        "window.__ANVIL_BOOT__ = " + boot.dump() +
        "; window.__ANVIL_NATIVE__ = true;";
    WebKitUserScript* script = webkit_user_script_new(
        bootstrap.c_str(), WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, nullptr, nullptr);
    webkit_user_content_manager_add_script(app->content_manager, script);
    webkit_user_script_unref(script);

    app->web_view = WEBKIT_WEB_VIEW(
        webkit_web_view_new_with_user_content_manager(app->content_manager));
    WebKitSettings* settings = webkit_settings_new();
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_enable_webgl(settings, FALSE);
    webkit_settings_set_enable_media(settings, FALSE);
    webkit_settings_set_enable_webaudio(settings, FALSE);
    webkit_settings_set_enable_page_cache(settings, FALSE);
    webkit_settings_set_enable_html5_local_storage(settings, FALSE);
    webkit_settings_set_javascript_can_access_clipboard(settings, TRUE);
    webkit_settings_set_allow_file_access_from_file_urls(settings, FALSE);
    webkit_settings_set_allow_universal_access_from_file_urls(settings, FALSE);
    webkit_settings_set_enable_developer_extras(settings, app->options.dev_mode);
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, app->options.dev_mode);
    webkit_web_view_set_settings(app->web_view, settings);
    g_object_unref(settings);

    g_signal_connect(app->web_view, "load-changed", G_CALLBACK(on_load_changed), app);
    g_signal_connect(app->web_view, "decide-policy", G_CALLBACK(on_decide_policy), app);
    g_signal_connect(app->web_view, "create", G_CALLBACK(on_create), app);
    g_signal_connect(app->web_view, "permission-request", G_CALLBACK(on_permission_request), app);
    g_signal_connect(app->web_view, "context-menu", G_CALLBACK(on_context_menu), app);
    g_signal_connect(webkit_web_view_get_context(app->web_view), "download-started",
                     G_CALLBACK(on_download_started), app);
    g_signal_connect(app->web_view, "web-process-terminated",
                     G_CALLBACK(on_web_process_terminated), app);
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_window_destroy), app);
    gtk_container_add(GTK_CONTAINER(app->window), GTK_WIDGET(app->web_view));

    app->cancellable = g_cancellable_new();
    if (!app->options.workspace.empty()) app->allow_workspace(app->options.workspace);
    start_core(app);

    const fs::path index = fs::path(app->options.ui_dir) / "index.html";
    GError* error = nullptr;
    gchar* uri = g_filename_to_uri(index.c_str(), nullptr, &error);
    if (uri == nullptr) {
        g_printerr("anvil: cannot build UI URI: %s\n",
                   error != nullptr ? error->message : "unknown error");
        if (error != nullptr) g_error_free(error);
        app->exit_code = 1;
        g_application_quit(G_APPLICATION(application));
        return;
    }
    app->base_uri = uri;
    webkit_web_view_load_uri(app->web_view, uri);
    g_free(uri);
    gtk_widget_show_all(GTK_WIDGET(app->window));
    app->timeout_id = g_timeout_add_seconds(30, startup_timeout, app);
}

}  // namespace

int main(int argc, char** argv) {
    ShellOptions options;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--version" || argument == "-v") {
            std::printf("anvil 0.0.1\n");
            return 0;
        }
        if (argument == "--help" || argument == "-h") {
            std::printf("Usage: anvil [options] [workspace]\n"
                        "  --smoke-test            load the UI, verify the bridge, exit\n"
                        "  --screenshot PATH       capture the web view after startup\n"
                        "  --screenshot-delay SEC  seconds to wait before capture (default 4)\n"
                        "  --e2e                   enable UI test hooks\n"
                        "  --e2e-file PATH         open a workspace-relative file after startup\n"
                        "  --e2e-hover LINE:COL     show a clangd hover at a 1-based position\n"
                        "  --workspace PATH        initial workspace\n"
                        "  --ui-dir PATH           override the bundled UI directory\n"
                        "  --core PATH             override the anvil-core path\n"
                        "  --lang LOCALE           override the UI locale (also ANVIL_LANG)\n"
                        "  --dev                   enable developer extras and debug logging\n");
            return 0;
        }
        if (argument == "--smoke-test") {
            options.smoke_test = true;
            continue;
        }
        if (argument == "--e2e") {
            options.e2e = true;
            continue;
        }
        if (argument == "--dev") {
            options.dev_mode = true;
            continue;
        }
        if (argument == "--screenshot" && i + 1 < argc) {
            options.screenshot_path = argv[++i];
            continue;
        }
        if (argument == "--screenshot-delay" && i + 1 < argc) {
            options.screenshot_delay_seconds = std::atoi(argv[++i]);
            continue;
        }
        if (argument == "--e2e-file" && i + 1 < argc) {
            options.e2e_file = argv[++i];
            options.e2e = true;
            continue;
        }
        if (argument == "--e2e-hover" && i + 1 < argc) {
            const std::string value = argv[++i];
            const std::size_t colon = value.find(':');
            if (colon != std::string::npos) {
                options.e2e_hover_line = std::atoi(value.substr(0, colon).c_str());
                options.e2e_hover_character =
                    std::atoi(value.substr(colon + 1).c_str());
                options.e2e = true;
            }
            continue;
        }
        if (argument == "--workspace" && i + 1 < argc) {
            options.workspace = argv[++i];
            continue;
        }
        if (argument == "--ui-dir" && i + 1 < argc) {
            options.ui_dir = argv[++i];
            continue;
        }
        if (argument == "--core" && i + 1 < argc) {
            options.core_path = argv[++i];
            continue;
        }
        if (argument == "--lang" && i + 1 < argc) {
            options.lang = argv[++i];
            continue;
        }
        if (argument.rfind("--", 0) == 0) {
            std::fprintf(stderr, "anvil: unknown option: %s\n", argument.c_str());
            return 2;
        }
        positional.push_back(argument);
    }
    if (options.workspace.empty() && !positional.empty()) {
        options.workspace = positional.front();
    }
    if (options.workspace.empty() && options.e2e) {
        options.workspace = fs::current_path().string();
    }
    // Desktop files and file-manager launches pass a file path. Open its
    // parent as the workspace and the file itself as the active editor.
    if (!options.workspace.empty()) {
        std::error_code ec;
        const fs::path absolute = fs::weakly_canonical(options.workspace, ec);
        if (!ec) options.workspace = absolute.string();
        if (fs::is_regular_file(options.workspace, ec)) {
            const fs::path file(options.workspace);
            if (options.e2e_file.empty()) options.e2e_file = file.filename().string();
            options.workspace = file.parent_path().string();
        }
    }
    options.ui_dir = detect_ui_dir(options);
    options.core_path = detect_core_path(options);
    if (options.ui_dir.empty() || !fs::exists(fs::path(options.ui_dir) / "index.html")) {
        std::fprintf(stderr, "anvil: UI bundle not found at %s\n", options.ui_dir.c_str());
        return 1;
    }

    auto* app = new ShellApp();
    app->options = options;
    GtkApplication* application =
        gtk_application_new("dev.anvil.Anvil", G_APPLICATION_NON_UNIQUE);
    app->application = application;
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), app);
    char* gtk_argv[] = {argv[0], nullptr};
    const int status = g_application_run(G_APPLICATION(application), 1, gtk_argv);
    const int app_exit_code = app->exit_code;
    delete app;
    g_object_unref(application);
    if (status != 0) return status;
    return app_exit_code;
}
