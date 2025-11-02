#include "common.hpp"
#include "cli.hpp"

#include "ssm.hpp"

#include <print>

using utils::cli::Command;
using utils::cli::arg;

int main(int argc, char* argv[]) {
    const Command app = Command("ssm", "Simple Snippet Manager")
        .subcommand(Command("new", "Create a new snippet")
            .arg(arg("<NAME>")
                .about("Name of the snippet")))
        .subcommand(Command("ls", "List all snippets"))
        .subcommand(Command("rm", "Remove a snippet")
            .arg(arg("<NAME>")
                .about("Name of the snippet to remove")))
        .subcommand(Command("get", "Get a snippet's content")
            .arg(arg("<NAME>")
                .about("Name of the snippet")))
        .subcommand(Command("edit", "Edit a snippet")
            .arg(arg("<NAME>")
                .about("Name of the snippet to edit")))
        .arg(arg("-h --help")
            .about("Show this help message"));

    const auto [matches, err] = app.get_matches(argc, argv);

    if (err.has_error()) {
        std::println(stderr, "Error parsing arguments: {}", err.message);
        app.print_help();
        return 1;
    }

    if (matches.get_flag("help")) {
        app.print_help();
        return 0;
    }

    if (matches.subcommand().has_value()) {
        const auto [subcmd_name, subcmd_matches] = *matches.subcommand();

        if (subcmd_name == "new") {
            const std::string name = *subcmd_matches->get_one("NAME");
            return ssm::create_snippet(name) ? 0 : 1;
        }
        if (subcmd_name == "ls") {
            ssm::list_snippets();
            return 0;
        }
        if (subcmd_name == "rm") {
            const std::string name = *subcmd_matches->get_one("NAME");
            return ssm::remove_snippet(name) ? 0 : 1;
        }
        if (subcmd_name == "get") {
            const std::string name = *subcmd_matches->get_one("NAME");
            return ssm::get_snippet(name) ? 0 : 1;
        }
        if (subcmd_name == "edit") {
            const std::string name = *subcmd_matches->get_one("NAME");
            return ssm::edit_snippet(name) ? 0 : 1;
        }

        std::println(stderr, "Unknown subcommand: {}", subcmd_name);
        app.print_help();
        return 1;
    }

    app.print_help();
    return 0;
}
