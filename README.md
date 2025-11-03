# Simple Snippet Manager

## Usage

```bash
Usage: ssm <COMMAND> [OPTIONS]
Simple Snippet Manager

Commands:
    new     Create a new snippet
    ls      List all snippets
    rm      Remove a snippet
    get     Get a snippet's content
    edit    Edit a snippet

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
