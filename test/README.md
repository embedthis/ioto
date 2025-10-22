# Ioto Agent Test Suite

Comprehensive test suite for the Ioto Device Agent. Tests are organized by category and use the TestMe framework.

## Test Categories

- [basic/](basic/) - Core functionality tests (database, web server, state management)
- [cloud/](cloud/) - Cloud integration tests (AWS, MQTT, device synchronization)
- [command/](command/) - Command-line interface tests
- [link/](link/) - Ioto linking and embedding tests
- [scale/](scale/) - Performance and scalability tests

## Running Tests

### Run All Tests
```bash
cd /Users/mob/c/agent/test
tm                          # Run entire test suite
```

### Run Category Tests
```bash
cd /Users/mob/c/agent/test
tm basic                    # Run all basic tests
tm cloud/mqtt               # Run MQTT tests
tm scale                    # Run scale tests
```

### Run Specific Tests
```bash
cd /Users/mob/c/agent/test
tm basic/web/get.tst.c      # Run specific C test
tm cloud/mqtt/ping.tst.sh   # Run specific shell test
```

## Test Configuration

Test configuration is defined in [testme.json5](testme.json5):
- Compiler settings for C tests
- Library dependencies
- Environment variables
- Execution parameters (timeout, workers, parallelism)
- Global preparation script

## Test File Naming

- `.tst.c` - C unit tests (compiled and linked with Ioto libraries)
- `.tst.sh` - Shell script tests (bash-based integration tests)

All tests include a `.tst` marker in the filename to identify them as TestMe tests.

## Test Design Guidelines

1. **Portability** - Tests must run on Linux, macOS, and Windows (WSL)
2. **Isolation** - Tests use process ID in filenames to avoid conflicts
3. **Parallel Safe** - Tests must be able to run concurrently
4. **State Management** - Tests create temporary state directories (cleaned up after)
5. **Assertions** - Use TestMe assertion functions (ttrue, tfalse, tassert, etc.)

## Prerequisites

- Built Ioto agent: `make APP=unit`
- TestMe installed: `~/.local/bin/tm`
- Libraries available in `../build/bin/`
- Test preparation script: `./prep.sh`

## Directory Structure

```
test/
├── README.md              # This file
├── testme.json5           # Test configuration
├── prep.sh                # Global test preparation
├── basic/                 # Core functionality tests
├── cloud/                 # Cloud integration tests
├── command/               # CLI tests
├── link/                  # Embedding tests
└── scale/                 # Performance tests
```

## Development Workflow

After making code changes:

1. Build the unit test app:
   ```bash
   make APP=unit
   ```

2. Run affected tests:
   ```bash
   cd test
   tm basic                # For core changes
   tm cloud                # For cloud changes
   ```

3. Verify all tests pass before committing

## See Also

- [TestMe C API](~/.local/share/doc/testme/README-C.md)
- [TestMe JavaScript API](~/.local/share/doc/testme/README-JS.md)
- [TestMe Test Design](~/.local/share/doc/testme/README-TEST.md)
- [Ioto Agent Documentation](../README.md)
