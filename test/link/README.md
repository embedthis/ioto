# test/link - Ioto Link Tests

Tests for linking and embedding the Ioto agent library into custom applications. Validates API usage, library initialization, and integration patterns.

## Test Coverage

### Main Integration ([main.tst.c](main.tst.c))
- Library initialization and cleanup
- API function calls from embedded context
- Memory management when linking Ioto libraries
- Proper shutdown and resource cleanup
- Integration with custom application code

## Running Tests

Run all link tests:
```bash
cd /Users/mob/c/agent/test
tm link
```

Run individual test:
```bash
tm link/main.tst.c          # Link integration test
```

## Test Environment

- Compiles and links test code with Ioto libraries
- Uses libraries from `../../build/bin/`
- Validates proper library initialization sequences
- Tests API boundaries and error handling

## Use Cases

These tests validate embedding scenarios:
1. Linking Ioto as a library in custom firmware
2. Calling Ioto APIs from application code
3. Proper initialization order for dependent modules
4. Resource management and cleanup

## Prerequisites

- Ioto libraries built with: `make`
- TestMe installed: `tm --version`
- C compiler (gcc, clang, or MSVC)
- All Ioto dependencies (OpenSSL, etc.)

## Compilation

Tests are compiled with standard TestMe C compiler settings:
- Include paths: `../../build/inc/`
- Library paths: `../../build/bin/`
- Linked libraries: r, uctx, json, crypt, db, web, url, websock, mqtt, openai

See `../testme.json5` for complete compiler configuration.

## See Also

- [Parent test documentation](../README.md)
- [Ioto Agent Architecture](../../CLAUDE.md)
- [Safe Runtime Library](../../paks/r/README.md)
- [Embedding Guide](../../README.md#embedding)
