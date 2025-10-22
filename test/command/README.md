# test/command - Ioto Command Line Tests

Tests for Ioto agent command-line interface including argument parsing, configuration loading, and runtime behavior.

## Test Coverage

### Command Generation ([gen.tst.sh](gen.tst.sh))
- Command-line argument parsing
- Configuration file generation
- Help and usage output
- Version information
- Error handling for invalid arguments

### State Management ([state/](state/))
- Runtime state directory creation and management
- Configuration file loading from command-line paths

## Running Tests

Run all command tests:
```bash
cd /Users/mob/c/agent/test
tm command
```

Run individual test:
```bash
tm command/gen.tst.sh       # Command generation test
```

## Test Environment

- Invokes `ioto` binary from `../../build/bin/`
- Creates temporary state directories for isolated testing
- Tests various command-line flags and options
- Validates configuration file parsing and error handling

## Command-Line Interface

The Ioto agent supports various command-line options tested here:
- `-v` - Verbose output
- `-d` - Debug mode
- `--help` - Display usage information
- `--version` - Display version information
- Configuration file paths and loading

## Prerequisites

- Ioto agent built with: `make APP=unit`
- TestMe installed: `tm --version`
- Bash shell (available on all platforms including Windows/WSL)

## See Also

- [Parent test documentation](../README.md)
- [Ioto Agent Documentation](../../README.md)
- [Ioto Configuration](../../apps/unit/config/ioto.json5)
