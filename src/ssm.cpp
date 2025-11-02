#include "ssm.hpp"

#define UTILS_PROCESS_IMPLEMENTATION
#include "process.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <print>

#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

std::optional<fs::path> get_home() {
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
        return {fs::path(home)};
    }

    const passwd* pw = getpwuid(getuid());
    if (pw != nullptr && pw->pw_dir != nullptr && pw->pw_dir[0] != '\0') {
        return {fs::path(pw->pw_dir)};
    }

    return std::nullopt;
}

std::optional<fs::path> get_snippet_dir() {
    const auto home = get_home();
    if (!home.has_value()) {
        std::println("Could not determine home directory");
        return std::nullopt;
    }
    return {*home / ssm::SNIPPETS_DIRNAME};
}

} // namespace

namespace ssm {

bool create_snippet(std::string_view name) {
    const auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return false;
    const fs::path& dir = *dir_opt;

    fs::create_directories(dir);

    const fs::path file = dir / name;
    if (fs::exists(file)) {
        std::println("Snippet '{}' already exists", name);
        return false;
    }

    if (const std::ofstream ofs(file); !ofs) {
        std::println("Failed to create snippet '{}'", name);
        return false;
    }

    return edit_snippet(name);
}

void list_snippets() {
    const auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return;
    const fs::path& dir = *dir_opt;

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        std::println("Snippets directory does not exist");
        return;
    }

    std::println("Available snippets:");
    int index = 1;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            std::println("{} - {}", index, entry.path().filename().string());
            ++index;
        }
    }
}

bool remove_snippet(std::string_view name) {
    auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const fs::path file = *dir_opt / name;

    if (!fs::exists(file)) {
        std::println("Snippet '{}' does not exist", name);
        return false;
    }

    fs::remove(file);
    std::println("Snippet '{}' removed successfully", name);
    return true;
}

bool get_snippet(std::string_view name) {
    const auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const fs::path file = *dir_opt / name;

    if (!fs::exists(file)) {
        std::println("Snippet '{}' does not exist", name);
        return false;
    }

    std::ifstream ifs(file);
    if (!ifs) {
        std::println("Failed to open snippet '{}'", name);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::println("'{}':\n{}", name, content);
    return true;
}

bool edit_snippet(std::string_view name) {
    const auto dir_opt = get_snippet_dir();
    if (!dir_opt.has_value()) return false;

    const fs::path file = *dir_opt / name;

    if (!fs::exists(file)) {
        std::println("Snippet '{}' does not exist.", name);
        return false;
    }

    if (const auto result = utils::process::run_sync({"micro", file.string()}); !result.has_value()) {
        std::println("Failed to launch editor for snippet '{}'", name);
        return false;
    }

    return true;
}

} // namespace ssm