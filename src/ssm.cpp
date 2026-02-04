#include "ssm.hpp"

#include "sqlite3.hpp"

#define UTILS_PROCESS_IMPLEMENTATION
#include "process.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <print>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::optional<fs::path> get_home() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return {fs::path(home)};
    }
    return std::nullopt;
}

std::string get_editor() {
    const char* editor = std::getenv("EDITOR");
    if (editor != nullptr && editor[0] != '\0') {
        return {editor};
    }
    const char* visual = std::getenv("VISUAL");
    if (visual != nullptr && visual[0] != '\0') {
        return {visual};
    }
    return "nano";
}

std::optional<fs::path> get_snippet_dir() {
    const auto home = get_home();
    if (!home.has_value()) {
        std::println(stderr, "Could not determine home directory");
        return std::nullopt;
    }
    return {*home / ssm::SNIPPETS_DIRNAME};
}

std::optional<fs::path> ensure_snippet_dir() {
    auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return std::nullopt;
    fs::path& dir = *dir_opt;

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        std::println(stderr, "Snippets directory does not exist, did you run `ssm init`?");
        return std::nullopt;
    }

    return dir;
}

std::vector<std::string> snippet_names() {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return {};

    const ssm_sqlite3::database sqlite(*dir_opt / ssm::DB_FILENAME);
    if (!sqlite.ok()) {
        std::println(stderr, "Failed to open database");
        return {};
    }

    std::vector<std::string> names;

    constexpr auto select_sql = "SELECT name FROM file ORDER BY id ASC;";
    const ssm_sqlite3::stmt_handle stmt = sqlite.prepare(select_sql);
    if (!stmt) {
        std::println(stderr, "Failed to prepare statement: {}", sqlite.errmsg());
        return {};
    }

    for (int ret = sqlite3_step(stmt.get()); ret == SQLITE_ROW; ret = sqlite3_step(stmt.get())) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        if (text != nullptr) names.emplace_back(text);
    }

    return names;
}

bool get_snippet_impl(const fs::path& file) {
    if (!fs::exists(file)) {
        std::println(stderr, "Snippet '{}' does not exist", file.filename().string());
        return false;
    }

    std::ifstream ifs(file);
    if (!ifs) {
        std::println(stderr, "Failed to open snippet '{}'", file.filename().string());
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::print("{}", content);
    return true;
}

bool edit_snippet_impl(const fs::path& file) {
    if (!fs::exists(file)) {
        std::println(stderr, "Snippet '{}' does not exist", file.filename().string());
        return false;
    }

    const auto result = utils::process::run_sync({get_editor(), file.string()});
    if (!result.has_value()) {
        std::println(stderr, "Failed to launch editor for snippet '{}'", file.filename().string());
        return false;
    }
    return true;
}

} // namespace

namespace ssm {

bool ssm_init() {
    const auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return false;
    const fs::path& dir = *dir_opt;

    if (fs::exists(dir)) {
        std::println(stderr, "Snippets directory already exists at '{}'", dir.string());
        return false;
    }

    fs::create_directories(dir);
    std::println("Initialized snippets directory at '{}'", dir.string());

    constexpr auto create_tables = R"(
    CREATE TABLE IF NOT EXISTS file (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        path TEXT NOT NULL
    );

    CREATE UNIQUE INDEX IF NOT EXISTS uidx_file_name ON file (name);
    CREATE UNIQUE INDEX IF NOT EXISTS uidx_file_path ON file (path);
    )";

    const fs::path db_path = dir / DB_FILENAME;
    const ssm_sqlite3::database sqlite(db_path);

    if (!sqlite.ok()) {
        std::println(stderr, "Failed to create or open database at '{}'", db_path.string());
        return false;
    }

    if (!sqlite.exec(create_tables)) {
        std::println(stderr, "Failed to create database tables with error: {}", sqlite.errmsg());
        return false;
    }

    return true;
}

bool create_snippet(const std::string& name) {
    if (name.empty()) {
        std::println(stderr, "Snippet name cannot be empty");
        return false;
    }

    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const fs::path file = *dir_opt / name;
    if (fs::exists(file)) {
        std::println(stderr, "Snippet '{}' already exists, you can 'edit' or 'rm'", name);
        return false;
    }

    if (const std::ofstream ofs(file); !ofs) {
        std::println(stderr, "Failed to create snippet '{}'", name);
        return false;
    }

    if (!edit_snippet(name)) {
        fs::remove(file);
        return false;
    }

    const ssm_sqlite3::database sqlite(*dir_opt / DB_FILENAME);
    if (!sqlite.ok()) {
        std::println(stderr, "Failed to open database");
        return false;
    }

    constexpr auto insert_sql = "INSERT INTO file (name, path) VALUES (?, ?);";
    const ssm_sqlite3::stmt_handle stmt = sqlite.prepare(insert_sql);
    if (!stmt) {
        std::println(stderr, "Failed to prepare statement: {}", sqlite.errmsg());
        fs::remove(file);
        return false;
    }

    if (!ssm_sqlite3::database::bind_text(stmt.get(), 1, name) || !ssm_sqlite3::database::bind_text(stmt.get(), 2, file.string())) {
        std::println(stderr, "Failed to bind parameters: {}", sqlite.errmsg());
        fs::remove(file);
        return false;
    }

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::println(stderr, "Failed to execute statement: {}", sqlite.errmsg());
        fs::remove(file);
        return false;
    }

    return true;
}

void list_snippets() {
    const std::vector<std::string> names = snippet_names();

    if (names.empty()) {
        std::println("No snippets available");
        return;
    }

    int index = 1;
    std::println("Available snippets:\n");
    for (const std::string& name : names) {
        std::println("{}. {}", index, name);
        ++index;
    }
}

bool remove_snippet(const std::string& name) {
    if (name.empty()) {
        std::println(stderr, "Snippet name cannot be empty");
        return false;
    }

    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const fs::path file_path = *dir_opt / name;
    if (!fs::exists(file_path)) {
        std::println(stderr, "Snippet '{}' does not exist", name);
        return false;
    }

    const ssm_sqlite3::database sqlite(*dir_opt / DB_FILENAME);
    if (!sqlite.ok()) {
        std::println(stderr, "Failed to open database");
        return false;
    }

    constexpr auto delete_sql = "DELETE FROM file WHERE path = ?;";
    const ssm_sqlite3::stmt_handle stmt = sqlite.prepare(delete_sql);
    if (!stmt) {
        std::println(stderr, "Failed to prepare statement: {}", sqlite.errmsg());
        return false;
    }

    if (!ssm_sqlite3::database::bind_text(stmt.get(), 1, file_path.string())) {
        std::println(stderr, "Failed to bind parameters: {}", sqlite.errmsg());
        return false;
    }

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::println(stderr, "Failed to execute statement: {}", sqlite.errmsg());
        return false;
    }

    fs::remove(file_path);

    std::println("Snippet '{}' removed successfully", name);
    return true;
}

bool get_snippet(const std::string_view name) {
    if (name.empty()) {
        std::println(stderr, "Snippet name cannot be empty");
        return false;
    }

    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    return get_snippet_impl(*dir_opt / name);
}

bool get_snippet(const int number) {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const std::vector<std::string> names = snippet_names();

    if (number < 1 || static_cast<std::size_t>(number) > names.size()) {
        std::println(stderr, "Snippet number {} is out of range", number);
        return false;
    }

    return get_snippet_impl(*dir_opt / names[static_cast<std::size_t>(number - 1)]);
}

bool edit_snippet(const std::string_view name) {
    if (name.empty()) {
        std::println(stderr, "Snippet name cannot be empty");
        return false;
    }

    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    return edit_snippet_impl(*dir_opt / name);
}

bool edit_snippet(const int number) {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const std::vector<std::string> names = snippet_names();

    if (number < 1 || static_cast<std::size_t>(number) > names.size()) {
        std::println(stderr, "Snippet number {} is out of range", number);
        return false;
    }

    return edit_snippet_impl(*dir_opt / names[static_cast<std::size_t>(number - 1)]);
}

} // namespace ssm
