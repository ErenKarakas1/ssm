// -------------------------------------------------------------------------------------
//
// docs: https://github.com/ErenKarakas1/cpputils/blob/main/docs/process.md
// src: https://github.com/ErenKarakas1/cpputils
// license: MIT
//
// -------------------------------------------------------------------------------------

#ifndef UTILS_PROCESS_HPP
#define UTILS_PROCESS_HPP

#include <expected>
#include <string>
#include <vector>

#ifdef _WIN32
#include <bit>
#include <cstdint>
#include <string_view>
using Proc = void*; // HANDLE
using Fd   = void*;
inline const Proc INVALID_PROC = std::bit_cast<Proc>(static_cast<std::intptr_t>(-1)); // INVALID_HANDLE_VALUE
inline const Fd   INVALID_FD   = std::bit_cast<Fd>(static_cast<std::intptr_t>(-1));
#else
#include <sys/types.h>
using Proc = pid_t;
using Fd   = int;
inline constexpr Proc INVALID_PROC = -1;
inline constexpr Fd   INVALID_FD   = -1;
#endif // _WIN32

namespace utils::process {

struct Redirect {
    Fd fd_in  = INVALID_FD;
    Fd fd_out = INVALID_FD;
    Fd fd_err = INVALID_FD;
};

std::expected<Proc, std::string> run_async(const std::vector<std::string>& args, Redirect& redirect, bool reset_fds = true);

inline std::expected<Proc, std::string> run_async(const std::vector<std::string>& args) {
    Redirect redirect;
    return run_async(args, redirect);
}

std::expected<void, std::string> run_sync(const std::vector<std::string>& args, Redirect& redirect, bool reset_fds = true);

inline std::expected<void, std::string> run_sync(const std::vector<std::string>& args) {
    Redirect redirect;
    return run_sync(args, redirect);
}

std::expected<void, std::string> wait_proc(Proc proc);
std::expected<void, std::string> wait_procs(const std::vector<Proc>& procs);

std::expected<Fd, std::string> open_fd_for_read(const std::string& filename);
std::expected<Fd, std::string> open_fd_for_write(const std::string& filename);
void close_fd(Fd fd) noexcept;
void reset_fd(Fd& fd) noexcept;
void reset_redirect(Redirect& redirect) noexcept;

std::expected<void, std::string> create_pipe(Fd& read_end, Fd& write_end);

#ifdef _WIN32
std::string win32_error_to_string(unsigned long error_code) noexcept;
#else
std::string posix_error_to_string(int error_code) noexcept;
#endif // _WIN32

namespace detail {
#ifdef _WIN32
void argv_quote(std::string& result, const std::string_view argument);
void cmd_escape(std::string& cmd_line, const std::size_t start_pos);
std::string build_cmdline(const std::vector<std::string>& args);
#else
std::vector<const char*> build_cmdline(const std::vector<std::string>& args);
#endif // _WIN32
} // namespace detail

} // namespace utils::process

#endif // UTILS_PROCESS_HPP

// ————————————————————————————————————————————————————————————————————————————————
// Implementation section
// ————————————————————————————————————————————————————————————————————————————————

#ifdef UTILS_PROCESS_IMPLEMENTATION

#include <array>

#ifdef _WIN32
#include <cctype>
#include <type_traits>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif // _WIN32

namespace utils::process {

namespace detail {
#ifdef _WIN32
static_assert(std::is_same_v<HANDLE, void*>, "HANDLE must be a typedef to void* on Windows");
static_assert(std::is_same_v<DWORD, unsigned long>, "DWORD must be a typedef to unsigned long on Windows");

// The following three functions are used to escape arguments before passing them to cmd.exe. Code is based on:
// https://learn.microsoft.com/en-us/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way
void argv_quote(std::string& result, const std::string_view argument) {
    constexpr std::string_view whitespace_chars = " \t\n\v\"";
    if (!argument.empty() && argument.find_first_of(whitespace_chars) == std::string::npos) {
        result.append(argument);
        return;
    }

    result.push_back('"');
    for (auto it = argument.begin();; ++it) {
        unsigned int num_backslashes = 0;
        while (it != argument.end() && *it == '\\') {
            ++num_backslashes;
            ++it;
        }
        if (it == argument.end()) {
            result.append(num_backslashes * 2, '\\');
            break;
        }
        if (*it == '"') {
            result.append(num_backslashes * 2 + 1, '\\');
            result.push_back(*it);
        } else {
            result.append(num_backslashes, '\\');
            result.push_back(*it);
        }
    }
    result.push_back('"');
}

void cmd_escape(std::string& cmd_line, const std::size_t start_pos) {
    constexpr std::string_view meta_chars = "()%!^\"<>&|";
    for (std::size_t i = start_pos; i < cmd_line.size(); ++i) {
        if (meta_chars.find(cmd_line[i]) != std::string_view::npos) {
            cmd_line.insert(i, 1, '^');
            ++i; // Skip the newly inserted caret
        }
    }
}

std::string build_cmdline(const std::vector<std::string>& args) {
    std::string cmd_line;
    cmd_line.reserve(128);
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd_line.push_back(' ');
        const std::size_t arg_start = cmd_line.size();
        argv_quote(cmd_line, args[i]);
        cmd_escape(cmd_line, arg_start);
    }
    return cmd_line;
}
#else
std::vector<const char*> build_cmdline(const std::vector<std::string>& args) {
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1); // do NOT inline this, we do not want default initialization
    for (const std::string& s : args) {
        argv.push_back(s.c_str());
    }
    argv.push_back(nullptr); // null-terminate the array
    return argv;
}
#endif // _WIN32
} // namespace detail

std::expected<Proc, std::string> run_async(const std::vector<std::string>& args, Redirect& redirect,
                                           const bool reset_fds) {
    if (args.empty()) return std::unexpected("No command specified");

#ifdef _WIN32
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    if (redirect.fd_in != INVALID_FD || redirect.fd_out != INVALID_FD || redirect.fd_err != INVALID_FD) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput  = redirect.fd_in  == INVALID_FD ? GetStdHandle(STD_INPUT_HANDLE)  : redirect.fd_in;
        si.hStdOutput = redirect.fd_out == INVALID_FD ? GetStdHandle(STD_OUTPUT_HANDLE) : redirect.fd_out;
        si.hStdError  = redirect.fd_err == INVALID_FD ? GetStdHandle(STD_ERROR_HANDLE)  : redirect.fd_err;
    }

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Build command string
    std::string command_line = detail::build_cmdline(args);
    if (command_line.empty()) {
        return std::unexpected("Command line is empty");
    }

    std::vector<char> cmd_buf(command_line.begin(), command_line.end());
    cmd_buf.push_back('\0');
    if (!CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        if (reset_fds) reset_redirect(redirect);
        return std::unexpected("CreateProcessA failed: " + win32_error_to_string(GetLastError()));
    }

    // Close the thread handle but keep the process handle
    CloseHandle(pi.hThread);
    Proc proc = pi.hProcess;
#else
    posix_spawn_file_actions_t fa;
    if (posix_spawn_file_actions_init(&fa) != 0) {
        return std::unexpected("Could not init posix_spawn_file_actions: " + posix_error_to_string(errno));
    }

    auto add_redirect = [&](const Fd src, const int dest) -> bool {
        if (src == INVALID_FD) return true;
        if (posix_spawn_file_actions_adddup2(&fa, src, dest) != 0) {
            posix_spawn_file_actions_destroy(&fa);
            return false;
        }
        if (posix_spawn_file_actions_addclose(&fa, src) != 0) {
            posix_spawn_file_actions_destroy(&fa);
            return false;
        }
        return true;
    };

    if (!add_redirect(redirect.fd_in, STDIN_FILENO)) {
        return std::unexpected("Could not add stdin redirect: " + posix_error_to_string(errno));
    }
    if (!add_redirect(redirect.fd_out, STDOUT_FILENO)) {
        return std::unexpected("Could not add stdout redirect: " + posix_error_to_string(errno));
    }
    if (!add_redirect(redirect.fd_err, STDERR_FILENO)) {
        return std::unexpected("Could not add stderr redirect: " + posix_error_to_string(errno));
    }

    const std::vector<const char*> argv = detail::build_cmdline(args);
    if (argv[0] == nullptr) {
        posix_spawn_file_actions_destroy(&fa);
        return std::unexpected("Command line is empty");
    }

    pid_t child_pid;
    const int ec = posix_spawnp(&child_pid, argv[0], &fa, nullptr, const_cast<char* const*>(argv.data()), environ);
    posix_spawn_file_actions_destroy(&fa);

    if (ec != 0) {
        if (reset_fds) reset_redirect(redirect);
        return std::unexpected("Could not spawn '" + std::string(argv[0]) + "': " + posix_error_to_string(ec));
    }

    Proc proc = child_pid;
#endif // _WIN32
    if (reset_fds) reset_redirect(redirect);
    return {proc};
}

std::expected<void, std::string> run_sync(const std::vector<std::string>& args, Redirect& redirect,
                                          const bool reset_fds) {
    const auto result = run_async(args, redirect, reset_fds);
    if (!result) return std::unexpected(result.error());
    return wait_proc(*result);
}

std::expected<void, std::string> wait_proc(Proc proc) {
    if (proc == INVALID_PROC) return std::unexpected("Invalid process handle");

#ifdef _WIN32
    DWORD result = WaitForSingleObject(proc, INFINITE);
    if (result == WAIT_FAILED) {
        return std::unexpected("Could not wait on child process: " + win32_error_to_string(GetLastError()));
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(proc, &exit_status)) {
        return std::unexpected("Could not get exit code: " + win32_error_to_string(GetLastError()));
    }
    if (exit_status != 0) {
        return std::unexpected("Child process exited with error code: " + std::to_string(exit_status));
    }

    CloseHandle(proc);
    return {};
#else
    for (;;) {
        int wstatus = 0;
        if (waitpid(proc, &wstatus, 0) < 0) {
            return std::unexpected("Could not wait on child process: " + posix_error_to_string(errno));
        }

        if (WIFEXITED(wstatus)) {
            if (const int exit_status = WEXITSTATUS(wstatus); exit_status != 0) {
                return std::unexpected("Child process exited with error code: " + std::to_string(exit_status));
            }
            break;
        }
        if (WIFSIGNALED(wstatus)) {
            return std::unexpected("Child process terminated by signal: " + std::to_string(WTERMSIG(wstatus)));
        }
    }

    return {};
#endif // _WIN32
}

std::expected<void, std::string> wait_procs(const std::vector<Proc>& procs) {
    for (const Proc& proc : procs) {
        const auto result = wait_proc(proc);
        if (!result) return std::unexpected(result.error());
    }
    return {};
}

std::expected<Fd, std::string> open_fd_for_read(const std::string& filename) {
    if (filename.empty()) {
        return std::unexpected("Filename cannot be empty");
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    Fd fd = CreateFileA(filename.c_str(), GENERIC_READ, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
    if (fd == INVALID_FD) {
        return std::unexpected("Could not open file '" + filename +
                               "' for reading: " + win32_error_to_string(GetLastError()));
    }
    return {fd};
#else
    Fd fd = open(filename.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == INVALID_FD) {
        return std::unexpected("Could not open file '" + filename + "' for reading: " + posix_error_to_string(errno));
    }
    return {fd};
#endif // _WIN32
}

std::expected<Fd, std::string> open_fd_for_write(const std::string& filename) {
    if (filename.empty()) {
        return std::unexpected("Filename cannot be empty");
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    Fd fd = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fd == INVALID_FD) {
        return std::unexpected("Could not open file '" + filename +
                               "' for writing: " + win32_error_to_string(GetLastError()));
    }
    return {fd};
#else
    Fd fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd == INVALID_FD) {
        return std::unexpected("Could not open file '" + filename + "' for writing: " + posix_error_to_string(errno));
    }
    return {fd};
#endif // _WIN32
}

void close_fd(const Fd fd) noexcept {
    if (fd == INVALID_FD) return;
#ifdef _WIN32
    CloseHandle(fd);
#else
    close(fd);
#endif // _WIN32
}

void reset_fd(Fd& fd) noexcept {
    if (fd == INVALID_FD) return;
    close_fd(fd);
    fd = INVALID_FD;
}

void reset_redirect(Redirect& redirect) noexcept {
    reset_fd(redirect.fd_in);
    reset_fd(redirect.fd_out);
    reset_fd(redirect.fd_err);
}

std::expected<void, std::string> create_pipe(Fd& read_end, Fd& write_end) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&read_end, &write_end, &sa, 0)) {
        return std::unexpected("Could not create pipe: " + win32_error_to_string(GetLastError()));
    }
#else
    std::array<Fd, 2> fds;
#ifdef __APPLE__
    if (pipe(fds.data()) < 0) {
        return std::unexpected("Could not create pipe: " + posix_error_to_string(errno));
    }
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) < 0) {
        close_fd(fds[0]);
        close_fd(fds[1]);
        return std::unexpected("Could not set FD_CLOEXEC flag on pipe read end: " + posix_error_to_string(errno));
    }
    if (fcntl(fds[1], F_SETFD, FD_CLOEXEC) < 0) {
        close_fd(fds[0]);
        close_fd(fds[1]);
        return std::unexpected("Could not set FD_CLOEXEC flag on pipe write end: " + posix_error_to_string(errno));
    }
#else
    if (pipe2(fds.data(), O_CLOEXEC) < 0) {
        return std::unexpected("Could not create pipe: " + posix_error_to_string(errno));
    }
#endif // __APPLE__
    read_end = fds[0];
    write_end = fds[1];
#endif // _WIN32
    return {};
}

#ifdef _WIN32
std::string win32_error_to_string(unsigned long error_code) noexcept {
    constexpr int WIN32_ERR_MESSAGE_SIZE = 4096;
    std::array<char, WIN32_ERR_MESSAGE_SIZE> error_message = {};

    DWORD error_msg_size =
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code,
                       LANG_USER_DEFAULT, error_message.data(), WIN32_ERR_MESSAGE_SIZE, nullptr);
    if (error_msg_size == 0) {
        if (GetLastError() != ERROR_MR_MID_NOT_FOUND) {
            return "Could not get error message for error code " + std::to_string(error_code);
        }
        return "Unknown error code " + std::to_string(error_code);
    }

    while (error_msg_size > 0 && std::isspace(static_cast<unsigned char>(error_message[error_msg_size - 1]))) {
        error_message[--error_msg_size] = '\0';
    }
    return {error_message.data(), error_msg_size};
}
#else
std::string posix_error_to_string(const int error_code) noexcept {
    std::array<char, 256> error_message = {};
#if defined(__APPLE__) || ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE)
    if (strerror_r(error_code, error_message.data(), error_message.size()) != 0) {
        return "Unknown error code " + std::to_string(error_code);
    }
    return {error_message.data(), std::strlen(error_message.data())};
#else
    return strerror_r(error_code, error_message.data(), error_message.size());
#endif // _POSIX
}
#endif // _WIN32

} // namespace utils::process

#endif // UTILS_PROCESS_IMPLEMENTATION
