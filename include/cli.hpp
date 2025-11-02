// -------------------------------------------------------------------------------------
//
// docs: https://github.com/ErenKarakas1/cpputils/blob/main/docs/cli.md
// src: https://github.com/ErenKarakas1/cpputils
// license: MIT
//
// -------------------------------------------------------------------------------------

#ifndef UTILS_CLI_HPP
#define UTILS_CLI_HPP

#include "common.hpp"

#include <charconv>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace utils::cli {

using ValueType = std::variant<std::monostate, char, std::string, bool, i64, u64, f64>;

constexpr std::optional<std::string_view> shift(int& argc, char**& argv) {
    if (argc == 0) [[unlikely]] {
        return std::nullopt;
    }
    --argc;
    return {*argv++};
}

constexpr std::optional<std::string_view> peek(const int argc, char** argv) {
    if (argc == 0) [[unlikely]] {
        return std::nullopt;
    }
    return {*argv};
}

enum class ArgType : u8 {
    Flag,      // boolean flag like -v or --verbose
    Option,    // takes a value like [-f value] or [--file value]
    Positional // positional argument
};

class Arg {
public:
    explicit Arg(const std::string_view name) : name_(name) {}

    static Arg flag(const std::string_view name) {
        Arg arg(name);
        arg.type_ = ArgType::Flag;
        return arg;
    }

    static Arg option(const std::string_view name) {
        Arg arg(name);
        arg.type_ = ArgType::Option;
        arg.value_name_ = "VALUE";
        return arg;
    }

    static Arg positional(const std::string_view name) {
        Arg arg(name);
        arg.type_ = ArgType::Positional;
        return arg;
    }

    Arg& short_alias(const char c) {
        short_ = c;
        return *this;
    }

    Arg& long_alias(const std::string_view name) {
        long_ = name;
        return *this;
    }

    Arg& about(const std::string_view description) {
        description_ = description;
        return *this;
    }

    Arg& value_name(const std::string_view name) {
        value_name_ = name;
        return *this;
    }

    Arg& default_value(const ValueType& value) {
        default_value_ = value;
        return *this;
    }

    Arg& required(const bool req = true) {
        required_ = req;
        return *this;
    }

    Arg& multiple(const bool multiple = true) {
        multiple_ = multiple;
        return *this;
    }

    [[nodiscard]] const std::string& name() const {
        return name_;
    }
    [[nodiscard]] ArgType type() const {
        return type_;
    }
    [[nodiscard]] char short_alias() const {
        return short_;
    }
    [[nodiscard]] const std::string& long_alias() const {
        return long_;
    }
    [[nodiscard]] const std::string& description() const {
        return description_;
    }
    [[nodiscard]] const std::string& value_name() const {
        return value_name_;
    }
    [[nodiscard]] const ValueType& default_value() const {
        return default_value_;
    }
    [[nodiscard]] bool is_required() const {
        return required_;
    }
    [[nodiscard]] bool is_multiple() const {
        return multiple_;
    }

private:
    std::string name_;
    ArgType type_ = ArgType::Flag;
    char short_ = '\0';
    std::string long_;
    std::string description_;
    std::string value_name_;
    ValueType default_value_ = std::monostate{};
    bool required_ = false;
    bool multiple_ = false;
};

constexpr Arg arg(std::string_view spec) {
    // Required positional argument
    if (spec.starts_with('<') && spec.ends_with('>')) {
        const auto name = spec.substr(1, spec.length() - 2);
        return Arg::positional(name).required(true);
    }

    // Optional positional argument
    if (spec.starts_with('[') && spec.ends_with(']')) {
        const auto name = spec.substr(1, spec.length() - 2);
        return Arg::positional(name).required(false);
    }

    if (spec.starts_with('-')) {
        bool is_option = false;
        std::string_view flag_part = spec;
        std::string_view value_part;

        // Look for <VALUE> pattern at the end
        const std::size_t angle_start = spec.find('<');
        if (angle_start != std::string_view::npos) {
            const std::size_t angle_end = spec.find('>', angle_start);
            if (angle_end != std::string_view::npos && angle_end == spec.length() - 1) {
                is_option = true;
                flag_part = spec.substr(0, angle_start);
                flag_part = flag_part.substr(0, flag_part.find_last_not_of(' ') + 1);
                value_part = spec.substr(angle_start + 1, angle_end - angle_start - 1);
            }
        }

        Arg result_arg = is_option ? Arg::option("") : Arg::flag("");

        const std::size_t space_pos = flag_part.find(' ');
        if (space_pos != std::string_view::npos) {
            // Has both short and long forms
            const auto short_part = flag_part.substr(0, space_pos);
            const auto long_part = flag_part.substr(space_pos + 1);

            char short_alias = '\0';
            if (short_part.starts_with('-') && short_part.length() == 2) {
                short_alias = short_part[1];
                result_arg.short_alias(short_alias);
            }

            if (long_part.starts_with("--") && long_part.length() > 2) {
                const auto long_name = long_part.substr(2);
                result_arg = is_option ? Arg::option(long_name) : Arg::flag(long_name);
                result_arg.short_alias(short_alias).long_alias(long_name);
            }

            if (is_option && !value_part.empty()) result_arg.value_name(value_part);
            return result_arg;
        }

        if (flag_part.starts_with("--") && flag_part.length() > 2) {
            const std::string_view long_name = flag_part.substr(2);
            result_arg = is_option ? Arg::option(long_name) : Arg::flag(long_name);
            result_arg.long_alias(long_name);
        } else if (flag_part.starts_with('-') && flag_part.length() == 2) {
            const char short_char = flag_part[1];
            const std::string short_str(1, short_char);
            result_arg = is_option ? Arg::option(short_str) : Arg::flag(short_str);
            result_arg.short_alias(short_char);
        }

        if (is_option && !value_part.empty()) result_arg.value_name(value_part);
        return result_arg;
    }

    // Default to flag
    return Arg::flag(spec);
}

class ArgMatches {
public:
    ArgMatches() = default;

    ArgMatches(const ArgMatches& other)
        : flags_(other.flags_), values_(other.values_), subcommand_name_(other.subcommand_name_) {
        if (other.subcommand_matches_) {
            subcommand_matches_ = std::make_unique<ArgMatches>(*other.subcommand_matches_);
        }
    }

    ArgMatches(ArgMatches&&) = default;

    ArgMatches& operator=(const ArgMatches& other) {
        if (this != &other) {
            flags_ = other.flags_;
            values_ = other.values_;
            subcommand_name_ = other.subcommand_name_;
            if (other.subcommand_matches_) {
                subcommand_matches_ = std::make_unique<ArgMatches>(*other.subcommand_matches_);
            } else {
                subcommand_matches_.reset();
            }
        }
        return *this;
    }

    ArgMatches& operator=(ArgMatches&&) = default;

    bool get_flag(const std::string& name) const {
        const auto it = flags_.find(name);
        return it != flags_.end() && it->second;
    }

    template <typename T = std::string>
    [[nodiscard]] std::optional<T> get_one(const std::string& name) const {
        const auto it = values_.find(name);
        if (it == values_.end() || it->second.empty()) {
            return std::nullopt;
        }
        return parse_value<T>(it->second.front());
    }

    [[nodiscard]] std::vector<std::string> get_many(const std::string& name) const {
        const auto it = values_.find(name);
        if (it == values_.end() || it->second.empty()) {
            return {};
        }
        return it->second;
    }

    [[nodiscard]] std::optional<std::pair<std::string, std::unique_ptr<ArgMatches>>> subcommand() const {
        if (subcommand_name_.empty()) {
            return std::nullopt;
        }
        return std::make_pair(subcommand_name_, std::make_unique<ArgMatches>(*subcommand_matches_));
    }

    void set_flag(const std::string& name, const bool value) {
        flags_[name] = value;
    }

    void add_value(const std::string& name, const std::string& value) {
        values_[name].push_back(value);
    }

    void set_subcommand(const std::string& name, std::unique_ptr<ArgMatches> matches) {
        subcommand_name_ = name;
        subcommand_matches_ = MOVE(matches);
    }

private:
    std::unordered_map<std::string, bool> flags_;
    std::unordered_map<std::string, std::vector<std::string>> values_;
    std::string subcommand_name_;
    std::unique_ptr<ArgMatches> subcommand_matches_;

    template <typename T>
    static constexpr std::optional<T> parse_value(std::string_view str) {
        if constexpr (std::is_same_v<T, std::string>) {
            return {std::string(str)};
        } else if constexpr (std::is_arithmetic_v<T>) {
            T result;
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
            if (ec == std::errc{} && ptr == str.data() + str.size()) return {result};
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, bool>) {
            if (str == "true"  || str == "1" || str == "yes" || str == "on") return {true};
            if (str == "false" || str == "0" || str == "no"  || str == "off") return {false};
            return std::nullopt;
        }
        return std::nullopt;
    }
};

struct ParseError {

    enum class ParseErrorType : u8 {
        None,
        MissingRequiredArgument,
        MissingRequiredSubcommand,
    };

    ParseErrorType type;
    std::string message;

    [[nodiscard]] constexpr bool has_error() const {
        return type != ParseErrorType::None;
    }

    static ParseError None() {
        return {.type = ParseErrorType::None, .message = ""};
    }

    static ParseError MissingRequiredArgument(const std::string& cmd_name, const std::string& arg_name) {
        return {.type = ParseErrorType::MissingRequiredArgument, .message = std::format("Missing required argument '{}' for command '{}'", arg_name, cmd_name)};
    }

    static ParseError MissingRequiredSubcommand(const std::string& cmd_name) {
        return {.type = ParseErrorType::MissingRequiredSubcommand, .message = std::format("Missing required subcommand for command '{}'", cmd_name)};
    }
};

struct ParseResult {
    ArgMatches matches;
    ParseError error;
};

class Command {
public:
    explicit Command(const std::string_view name, const std::string_view description = "")
        : name_(name), description_(description) {
        ASSERT(!name_.empty(), "Command name cannot be empty");
    }

    Command& subcommand(const Command& cmd) {
        if (cmd.name().size() > max_cmd_len) max_cmd_len = cmd.name().size();
        subcommands_.push_back(cmd);
        return *this;
    }

    Command& subcommand_required(bool required = true) {
        subcommand_required_ = required;
        return *this;
    }

    Command& about(std::string_view description) {
        description_ = description;
        return *this;
    }

    Command& arg(const Arg& arg) {
        // Calculate max option length for help formatting
        std::size_t curr_opt_len = 0;
        if (arg.short_alias() != '\0') {
            curr_opt_len += 2; // "-x"
        }
        if (!arg.long_alias().empty()) {
            if (curr_opt_len > 0) {
                curr_opt_len += 2; // ", "
            }
            curr_opt_len += arg.long_alias().size() + 2; // "--name"
        } else if (!arg.name().empty() && arg.type() != ArgType::Positional) {
            if (curr_opt_len > 0) {
                curr_opt_len += 2; // ", "
            }
            curr_opt_len += arg.name().size() + 2; // "--name"
        }
        if (!arg.value_name().empty() && arg.type() == ArgType::Option) {
            curr_opt_len += arg.value_name().size() + 3; // " <value>"
        }
        if (curr_opt_len > max_opt_len) max_opt_len = curr_opt_len;

        args_.push_back(arg);
        return *this;
    }

    ParseResult get_matches(const int argc, char** argv) const {
        ArgMatches matches;
        int current_argc = argc;
        char** current_argv = argv;
        shift(current_argc, current_argv); // Skip program name
        return parse_args(matches, current_argc, current_argv);
    }

    [[nodiscard]] const std::string& name() const {
        return name_;
    }

    [[nodiscard]] const std::string& description() const {
        return description_;
    }

    [[nodiscard]] const std::vector<Command>& subcommands() const {
        return subcommands_;
    }

    [[nodiscard]] const std::vector<Arg>& args() const {
        return args_;
    }

    void clear() {
        subcommands_.clear();
        args_.clear();
        max_opt_len = 0;
        max_cmd_len = 0;
    }

    void print_help() const {
        std::print("Usage: {}", name_);

        for (const Arg& arg : args_) {
            if (arg.type() == ArgType::Positional) {
                if (arg.is_required()) {
                    std::print(" <{}>", arg.name());
                } else {
                    std::print(" [{}]", arg.name());
                }
            }
        }

        if (!subcommands_.empty()) {
            std::print(" <COMMAND>");
        }

        // Check if we have any non-positional args for [OPTIONS]
        bool has_options = false;
        for (const auto& arg : args_) {
            if (arg.type() != ArgType::Positional) {
                has_options = true;
                break;
            }
        }
        if (has_options) {
            std::print(" [OPTIONS]");
        }

        std::println();
        if (!description_.empty()) {
            std::print("{}", description_);
        }
        std::println();

        if (!subcommands_.empty()) {
            std::println("\nCommands:");
            for (const Command& cmd : subcommands_) {
                std::print("    {}{:{}}", cmd.name(), "", max_cmd_len - cmd.name().size());
                if (!cmd.description().empty()) {
                    std::print("    {}", cmd.description());
                }
                std::println();
            }
        }

        if (has_options) {
            std::println("\nOptions:");

            for (const Arg& arg : args_) {
                if (arg.type() == ArgType::Positional) continue;

                std::print("    ");
                std::size_t written = 0;

                if (arg.short_alias() != '\0') {
                    std::print("-{}", arg.short_alias());
                    written += 2;
                }

                const std::string& long_name = arg.long_alias().empty() ? arg.name() : arg.long_alias();
                if (!long_name.empty()) {
                    if (written > 0) {
                        std::print(", ");
                        written += 2;
                    }
                    std::print("--{}", long_name);
                    written += long_name.size() + 2;
                }

                if (!arg.value_name().empty() && arg.type() == ArgType::Option) {
                    std::print(" <{}>", arg.value_name());
                    written += arg.value_name().size() + 3;
                }

                if (written < max_opt_len) {
                    std::print("{:{}}", "", max_opt_len - written);
                }

                if (std::holds_alternative<std::monostate>(arg.default_value())) {
                    std::println("    {}", arg.description());
                } else {
                    std::println("    {} (default: {})", arg.description(), to_formatted(arg.default_value()));
                }
            }
        }
    }

private:
    std::string name_;
    std::string description_;

    std::vector<Command> subcommands_;
    std::vector<Arg> args_;

    std::size_t max_opt_len = 0;
    std::size_t max_cmd_len = 0;
    bool subcommand_required_ = false;

    static constexpr std::string to_formatted(const ValueType& var) {
        static_assert(std::variant_size_v<ValueType> == 7, "ValueType variant size changed, update to_formatted accordingly");
        switch (var.index()) {
        case 0: return "";
        case 1: return std::format("'{}'", *std::get_if<char>(&var));
        case 2: return std::format("\"{}\"", *std::get_if<std::string>(&var));
        case 3: return std::format("{}", *std::get_if<bool>(&var) ? "true" : "false");
        case 4: return std::format("{}", *std::get_if<i64>(&var));
        case 5: return std::format("{}", *std::get_if<u64>(&var));
        case 6: return std::format("{}", *std::get_if<f64>(&var));
        default: UNREACHABLE();
        }
    }

    ParseResult parse_args(ArgMatches& matches, const int argc, char** argv) const {
        int current_argc = argc;
        char** current_argv = argv;
        std::size_t positional_index = 0;

        while (current_argc > 0) {
            const std::string_view arg = *current_argv;

            // Check for subcommands first
            for (const auto& subcmd : subcommands_) {
                if (arg != subcmd.name()) continue;
                shift(current_argc, current_argv);

                ArgMatches temp_matches;
                const auto [res, err] = subcmd.parse_args(temp_matches, current_argc, current_argv);
                if (err.has_error()) return {.matches = {}, .error = err};

                matches.set_subcommand(subcmd.name(), std::make_unique<ArgMatches>(MOVE(res)));
                return {.matches = matches, .error = ParseError::None()};
            }

            // Check for --
            if (arg == "--") {
                shift(current_argc, current_argv);
                // Treat all remaining arguments as positional
                while (current_argc > 0) {
                    const Arg* pos_arg = find_positional_arg(positional_index);
                    if (pos_arg != nullptr) {
                        matches.add_value(pos_arg->name(), std::string(*current_argv));
                        if (!pos_arg->is_multiple()) ++positional_index;
                    }
                    shift(current_argc, current_argv);
                }
                break;
            }

            // Handle flags and options
            if (arg.starts_with("--")) {
                const Arg* matching_arg = find_flag_by_long(arg.substr(2));
                add_flag_or_option(matches, matching_arg, current_argc, current_argv);
            } else if (arg.starts_with("-") && arg.length() > 1) {
                const Arg* matching_arg = find_flag_by_short(arg[1]);
                add_flag_or_option(matches, matching_arg, current_argc, current_argv);
            } else {
                const Arg* pos_arg = find_positional_arg(positional_index);
                if (pos_arg != nullptr) {
                    matches.add_value(pos_arg->name(), std::string(arg));
                    if (!pos_arg->is_multiple()) ++positional_index;
                }
            }

            shift(current_argc, current_argv);
        }

        // Validate required arguments
        for (const Arg& arg : args_) {
            if (!arg.is_required()) continue;
            const bool found = arg.type() == ArgType::Flag ? matches.get_flag(arg.name())
                                                           : matches.get_one<std::string>(arg.name()).has_value();
            if (!found) {
                return {.matches = {}, .error = ParseError::MissingRequiredArgument(name_, arg.name())};
            }
        }

        // Validate subcommand requirement
        if (subcommand_required_ && !subcommands_.empty() && !matches.subcommand().has_value()) {
            return {.matches = {}, .error = ParseError::MissingRequiredSubcommand(name_)};
        }

        return {.matches = matches, .error = ParseError::None()};
    }

    [[nodiscard]] const Arg* find_positional_arg(const std::size_t index) const {
        std::size_t pos_count = 0;
        for (const Arg& cmd_arg : args_) {
            if (cmd_arg.type() == ArgType::Positional) {
                if (pos_count == index) return &cmd_arg;
                ++pos_count;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const Arg* find_flag_by_short(const char short_alias) const {
        for (const Arg& cmd_arg : args_) {
            if (cmd_arg.short_alias() == short_alias) return &cmd_arg;
        }
        return nullptr;
    }

    [[nodiscard]] const Arg* find_flag_by_long(const std::string_view long_alias) const {
        for (const auto& cmd_arg : args_) {
            if (cmd_arg.long_alias() == long_alias || cmd_arg.name() == long_alias) return &cmd_arg;
        }
        return nullptr;
    }

    static void add_flag_or_option(ArgMatches& matches, const Arg* matching_arg, int& argc, char**& argv) {
        if (matching_arg == nullptr) return;
        const Arg& arg = *matching_arg;
        if (arg.type() == ArgType::Flag) {
            matches.set_flag(arg.name(), true);
        } else if (arg.type() == ArgType::Option) {
            shift(argc, argv);
            if (argc > 0) {
                matches.add_value(arg.name(), std::string(*argv));
            }
        }
    }
};

} // namespace utils::cli

#endif // UTILS_CLI_HPP
