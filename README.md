# Simple Snippet Manager

## Usage

```bash
$ ssm --help
Simple Snippet Manager
Usage: ssm <COMMAND> [OPTIONS]

Commands:
    init    Initialize ssm directory and database
    new     Create a new snippet
    ls      List all snippets
    rm      Remove a snippet
    get     Get a snippet's content
    edit    Edit a snippet

Options:
    -h, --help    Show this help message
```

`ssm init` initializes the snippet directory and database, so it should be run first.

---

Subcommands also have help messages.

```bash
$ ssm new --help
Create a new snippet
Usage: ssm new <NAME> [OPTIONS]

Options:
    -h, --help    Show this help message
```
---

Snippets are stored in `~/.local/share/snippets/` by default.

`ssm` expects `HOME` environment variable to be set.

`ssm edit` invokes the editor defined with the `EDITOR` environment variable. It falls back to `VISUAL`, and then to `nano`.

## Installation

```bash
make all      # Compile the program
make install  # Install the binary to ~/.local/bin
```
