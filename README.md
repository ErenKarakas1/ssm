# Simple Snippet Manager

## Usage

```bash
$ ssm --help
Simple Snippet Manager
Usage: ssm <COMMAND> [OPTIONS]

Commands:
    new     Create a new snippet
    ls      List all snippets
    rm      Remove a snippet
    get     Get a snippet's content
    edit    Edit a snippet

Options:
    -h, --help    Show this help message
```

Subcommands also have help messages.

```bash
$ ssm new --help
Create a new snippet
Usage: ssm new <NAME> [OPTIONS]

Options:
    -h, --help    Show this help message
```

Snippets are stored in `~/.snippets/` by default.

`ssm` expects `HOME` environment variable to be set.

`ssm edit` invokes the editor defined with the `EDITOR` environment variable. It falls back to `VISUAL`, and then to `nano`.

## Installation

```bash
make all      # Compile the program
make install  # Install the binary to ~/.local/bin
```
