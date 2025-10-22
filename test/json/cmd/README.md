# Command-Line Tests for json Utility

This directory contains shell script tests for the `json` command-line utility.

## Test Files

- **basic.tst.sh** - Basic functionality tests (version, help, check, stdin)
- **output.tst.sh** - Output options tests (--out, --overwrite, mutual exclusivity)
- **convert.tst.sh** - Format conversion tests (JSON, JSON5, JS formats)
- **query.tst.sh** - Query and modification tests (query, set, remove, bump)

## Running Tests

From the test directory:

```bash
# Run all tests including cmd tests
tm

# Run only cmd tests
tm cmd/

# Run a specific test
tm cmd/output.tst.sh
```

Or run directly:

```bash
cd test/cmd
./basic.tst.sh
./output.tst.sh
./convert.tst.sh
./query.tst.sh
```

## Test Coverage

### Basic Functionality (basic.tst.sh)
- `--version` flag
- Usage message display
- `--check` syntax validation (valid and invalid JSON)
- `--stdin` input reading
- Empty input handling

### Output Options (output.tst.sh)
- `--out` writes to specified output file
- `--out` with format conversion
- `--out` preserves input file
- `--out` and `--overwrite` are mutually exclusive
- `--overwrite` modifies files in-place
- `--out` with assignment operations

### Format Conversion (convert.tst.sh)
- JSON5 to JSON conversion
- JSON to JSON5 conversion
- `--compact` output formatting
- `--js` JavaScript module format
- `--one` single-line output
- `--double` and `--single` quote styles
- `--strict` parsing mode

### Query and Modify (query.tst.sh)
- Simple property queries
- Nested property queries with dot notation
- Array element queries
- Query with `--default` value
- Setting properties
- Setting nested properties
- Removing properties with `--remove`
- Version bumping with `--bump` (semver and numeric)
- `--keys` to list property names
- `--env` environment variable format
- `--header` C header define format

## Notes

- Tests use unique PIDs in temporary file names to allow parallel execution
- All tests clean up their temporary files after execution
- Tests verify both success cases and error conditions
- Exit code 0 indicates all tests passed, non-zero indicates failures

## Cleanup

Tests clean up their temporary files automatically. However, if tests are interrupted or killed, you can manually clean up stray files:

```bash
./cleanup.sh
```

This removes all `/tmp/json-test-*` files created by the tests.
