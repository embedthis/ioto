# test/basic - Core Functionality Tests

Basic unit tests for core Ioto agent features including database operations, web server functionality, and state management.

## Test Coverage

### Database Tests ([db/](db/))
- `update.tst.sh` - Database update and persistence operations

### Web Server Tests ([web/](web/))
- `get.tst.c` - HTTP GET request handling and response validation

### State Management ([state/](state/))
- Runtime state directory tests (created dynamically during test execution)

## Running Tests

Run all basic tests:
```bash
cd /Users/mob/c/agent/test
tm basic
```

Run specific test category:
```bash
tm basic/web                # Web server tests only
tm basic/db                 # Database tests only
```

Run individual test:
```bash
tm basic/web/get.tst.c      # Specific web test
```

## Test Environment

- Uses local web server on ephemeral ports
- Creates temporary database files with PID-based naming
- Cleans up state directories after execution
- Requires built Ioto libraries in `../../build/bin/`

## Prerequisites

- Ioto agent built with: `make APP=unit`
- TestMe installed: `tm --version`
- OpenSSL libraries available (for TLS tests)

## See Also

- [Parent test documentation](../README.md)
- [Ioto Web Server](../../paks/web/README.md)
- [Ioto Database](../../paks/db/README.md)
