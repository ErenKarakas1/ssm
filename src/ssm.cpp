#include "ssm.hpp"

#define UTILS_PROCESS_IMPLEMENTATION
#include "process.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <print>
#include <ranges>
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
        std::println(stderr, "Snippets directory does not exist");
        return std::nullopt;
    }

    return dir;
}

std::vector<std::string> snippet_names(const fs::path& dir) {
    std::vector<std::string> snippet_names =
        fs::directory_iterator(dir)
        | std::views::filter([](const fs::directory_entry& entry) { return entry.is_regular_file(); })
        | std::views::transform([](const fs::directory_entry& entry) { return entry.path().filename().string(); })
        | std::ranges::to<std::vector<std::string>>();

    std::ranges::sort(snippet_names);
    return snippet_names;
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
    std::println("{}\n-------------------------------------------------------------", file.filename().string());
    std::println("{}", content);
    return true;
}

bool edit_snippet_impl(const fs::path& file) {
    if (!fs::exists(file)) {
        std::println(stderr, "Snippet '{}' does not exist", file.filename().string());
        return false;
    }

    if (const auto result = utils::process::run_sync({get_editor(), file.string()}); !result.has_value()) {
        std::println(stderr, "Failed to launch editor for snippet '{}'", file.filename().string());
        return false;
    }
    return true;
}

} // namespace

namespace ssm {

bool create_snippet(const std::string_view name) {
    const auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return false;
    const fs::path& dir = *dir_opt;

    fs::create_directories(dir);

    const fs::path file = dir / name;
    if (fs::exists(file)) {
        std::println(stderr, "Snippet '{}' already exists, you can 'edit' or 'rm'", name);
        return false;
    }

    if (const std::ofstream ofs(file); !ofs) {
        std::println(stderr, "Failed to create snippet '{}'", name);
        return false;
    }

    return edit_snippet(name);
}

void list_snippets() {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return;

    const std::vector<std::string> names = snippet_names(*dir_opt);

    int index = 1;
    std::println("Available snippets:\n");
    for (const std::string& name : names) {
        std::println("  {}. {}", index, name);
        ++index;
    }
}

bool remove_snippet(const std::string_view name) {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const fs::path file = *dir_opt / name;

    if (!fs::exists(file)) {
        std::println(stderr, "Snippet '{}' does not exist", name);
        return false;
    }

    fs::remove(file);
    std::println("Snippet '{}' removed successfully", name);
    return true;
}

bool get_snippet(const std::string_view name) {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;
    return get_snippet_impl(*dir_opt / name);
}

bool get_snippet(const int number) {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const std::vector<std::string> names = snippet_names(*dir_opt);

    if (number < 1 || static_cast<std::size_t>(number) > names.size()) {
        std::println(stderr, "Snippet number {} is out of range", number);
        return false;
    }

    return get_snippet_impl(*dir_opt / names[number - 1]);
}

bool edit_snippet(const std::string_view name) {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;
    return edit_snippet_impl(*dir_opt / name);
}

bool edit_snippet(const int number) {
    const auto dir_opt = ensure_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const std::vector<std::string> names = snippet_names(*dir_opt);

    if (number < 1 || static_cast<std::size_t>(number) > names.size()) {
        std::println(stderr, "Snippet number {} is out of range", number);
        return false;
    }

    return edit_snippet_impl(*dir_opt / names[number - 1]);
}

} // namespace ssm
