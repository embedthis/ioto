# Web Server Fuzz Testing

Comprehensive automated fuzz testing for the web server module, designed to discover security vulnerabilities, protocol violations, and edge case handling issues.

## Quick Start

```bash
# Run all fuzz tests (requires web server running)
cd test/fuzz
tm

# Run specific fuzzer
tm proto-http

# Run with verbose output
tm -v

# Run with custom iterations
TESTME_ITERATIONS=50000 tm
```

## Overview

The fuzzing framework provides:

- **Protocol Fuzzing** - HTTP/HTTPS protocol parsing and compliance
- **Input Validation** - Path traversal, injection, encoding attacks
- **Resource Exhaustion** - DoS and resource limit testing
- **API Fuzzing** - Signature validation and type confusion
- **Auth/Session** - Authentication bypass and session attacks
- **TLS/SSL** - Cryptographic protocol fuzzing
- **WebSocket** - WebSocket protocol fuzzing
- **Upload** - File upload attack vectors

## Architecture

```
Fuzz Tests (*.c)
    ↓
Fuzz Library (fuzzLib.h/c)
    ↓
TestMe Orchestration (testme.json5)
    ↓
Web Server (dedicated fuzz ports)
```

### Build Artifacts

All intermediate objects and executables are placed under the `.testme` directory:

```
.testme/
├── libfuzz.a               # Fuzzing library (static)
├── fuzzLib.o               # Compiled fuzz library object
├── proto-http              # HTTP protocol fuzzer executable
├── proto-http.dSYM         # Debug symbols (macOS)
├── input-path              # Path validation fuzzer executable
└── ...                     # Other fuzzer artifacts
```

This keeps the source directory clean and follows the standard TestMe convention. The `prep.sh` script compiles `fuzzLib.c` into a static library (`libfuzz.a`) that is linked via `-lfuzz`.

### Source Files

```
fuzzLib.c, fuzzLib.h              # Fuzzing library
proto-http.tst.c                # HTTP protocol fuzzer
input-path.tst.c                # Path validation fuzzer
setup.sh, cleanup.sh        # Environment management scripts
```

## Fuzzer Categories

### Protocol Fuzzers

Test HTTP protocol parsing and handling:

- `proto-http.tst.c` - HTTP request line fuzzing
- `proto-headers.c` - HTTP header fuzzing
- `proto-chunked.c` - Chunked encoding fuzzing
- `proto-methods.c` - HTTP method fuzzing

**Example**:
```bash
tm proto-http
```

### Input Validation Fuzzers

Test input sanitization and validation:

- `input-path.tst.c` - URL path validation
- `input-query.c` - Query string parsing
- `input-form.c` - Form data parsing
- `input-json.c` - JSON payload validation
- `input-encoding.c` - URL encoding edge cases

**Example**:
```bash
tm input-path
```

### Resource Fuzzers

Test resource limits and DoS prevention:

- `resource-connections.c` - Connection flooding
- `resource-slowloris.c` - Slow request attacks
- `resource-memory.c` - Memory exhaustion
- `resource-sessions.c` - Session exhaustion

### Security Fuzzers

Test authentication and authorization:

- `auth-session.c` - Session management
- `auth-xsrf.c` - XSRF token validation
- `auth-roles.c` - Role-based access control
- `auth-cookies.c` - Cookie security

## Configuration

Fuzzing is configured via `testme.json5`:

```json5
{
    compiler: {
        c: {
            compiler: 'gcc',
            flags: [
                '-fsanitize=address,undefined',  // Enable sanitizers
                '-fno-omit-frame-pointer',
                '-g', '-O1',
                // ... other flags
            ],
        },
    },

    execution: {
        timeout: 300000,        // 5 min per fuzzer
        parallel: true,
        worker: 4,
    },

    environment: {
        ASAN_OPTIONS: 'detect_leaks=1:abort_on_error=1',
        UBSAN_OPTIONS: 'print_stacktrace=1:halt_on_error=1',
    },
}
```

## Environment Variables

- `TESTME_ITERATIONS` - Number of iterations per fuzzer (default: 1000, set by TestMe)
- `TESTME_VERBOSE` - Enable verbose output (set by TestMe via `tm -v`)
- `ASAN_OPTIONS` - Address Sanitizer options
- `UBSAN_OPTIONS` - Undefined Behavior Sanitizer options

Server endpoints are configured in `web.json5`:
- HTTP: http://localhost:4200
- HTTPS: https://localhost:4243

## Corpus Management

### Seed Corpus

Initial test inputs are stored in `corpus/`:

```
corpus/
├── http-requests.txt      # Valid HTTP requests
├── http-headers.txt       # Common headers
├── paths.txt              # URL paths
├── json-payloads.txt      # JSON examples
├── attack-patterns.txt    # Known attacks
└── websocket-frames.bin   # WebSocket frames
```

### Crash Corpus

Crash-inducing inputs are saved to `crashes/`:

```
crashes/
├── protocol-http/
│   ├── crash-DEADBEEF.txt
│   └── crash-CAFEBABE.txt
├── input-path/
│   └── crash-12345678.txt
└── crashes-archive/
    └── crashes-20250104-143022.tar.gz
```

Each crash file includes:
- Input that caused the crash
- Signal/error information
- Stack trace (if available)
- Timestamp

## Writing New Fuzzers

### 1. Create Fuzzer File

```c
// resource-newtest.tst.c
#include "../test.h"
#include "fuzzLib.h"

static bool testOracle(cchar *input, ssize len) {
    // Test the input, return true if passed
    // Return false if vulnerability found
    return true;
}

static char *mutateInput(cchar *input, ssize *len) {
    // Apply mutations to input
    return fuzzBitFlip(input, len);
}

int main(int argc, char **argv) {
    FuzzRunner *runner;
    FuzzConfig config = {
        .iterations = 10000,
        .timeout = 5000,
        .crashDir = "./crashes/resource-newtest",
    };

    if (!setup(&HTTP, NULL)) return 1;

    runner = fuzzInit(&config);
    fuzzSetOracle(runner, testOracle);
    fuzzSetMutator(runner, mutateInput);

    // Add corpus
    fuzzAddCorpus(runner, "initial input", 13);

    int crashes = fuzzRun(runner);
    fuzzReport(runner);
    fuzzFree(runner);

    return crashes > 0 ? 1 : 0;
}
```

### 2. Add to TestMe

The fuzzer will be automatically discovered if it matches the naming pattern in testme.json5.

### 3. Run Fuzzer

```bash
cd test/fuzz
tm resource-newtest
```

## Mutation Strategies

The fuzz library provides mutation functions:

```c
// Bit-level mutations
char *fuzzBitFlip(cchar *input, ssize *len);
char *fuzzByteFlip(cchar *input, ssize *len);

// Structural mutations
char *fuzzInsertRandom(cchar *input, ssize *len);
char *fuzzDeleteRandom(cchar *input, ssize *len);
char *fuzzTruncate(cchar *input, ssize *len);
char *fuzzDuplicate(cchar *input, ssize *len);

// Content mutations
char *fuzzInsertSpecial(cchar *input, ssize *len);
char *fuzzReplace(cchar *input, ssize *len, cchar *pattern, cchar *replacement);
char *fuzzOverwriteRandom(cchar *input, ssize *len);

// Generation
char *fuzzRandomString(ssize len);
char *fuzzRandomData(ssize len);
```

## Sanitizers

Sanitizers are configured in `testme.json5`. To temporarily override:

```bash
# Address Sanitizer - detects memory corruption, buffer overflows, use-after-free
CFLAGS="-fsanitize=address" tm

# Undefined Behavior Sanitizer - detects undefined behavior, integer overflow
CFLAGS="-fsanitize=undefined" tm

# Combined (recommended)
CFLAGS="-fsanitize=address,undefined" tm
```

## CI/CD Integration

### GitHub Actions

```yaml
name: Fuzz Testing
on:
  schedule:
    - cron: '0 2 * * *'  # Nightly

jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Run fuzz tests
        run: cd test/fuzz && TESTME_ITERATIONS=50000 tm
      - name: Upload crashes
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: fuzz-crashes
          path: test/fuzz/crashes/
```

### Jenkins

```groovy
pipeline {
    agent any
    triggers {
        cron('0 2 * * *')
    }
    stages {
        stage('Fuzz') {
            steps {
                sh 'cd test/fuzz && TESTME_ITERATIONS=50000 tm'
            }
        }
    }
    post {
        failure {
            archiveArtifacts 'test/fuzz/crashes/**'
        }
    }
}
```

## Analyzing Crashes

### 1. Reproduce Crash

```bash
# Find crash file
ls crashes/protocol-http/

# Replay crash with TestMe
cd test/fuzz
cat crashes/protocol-http/crash-DEADBEEF.txt | .testme/proto-http
```

### 2. Debug with GDB

```bash
gdb .testme/proto-http
(gdb) run < crashes/protocol-http/crash-DEADBEEF.txt
(gdb) bt
(gdb) info locals
```

### 3. Minimize Input

Use delta debugging to find minimal reproducer:

```bash
# Manual minimization
cat crash.txt | head -n 10 | .testme/fuzzer  # Test smaller input
cat crash.txt | sed 's/AAAAA/A/' | .testme/fuzzer  # Remove characters
```

### 4. Fix and Verify

After fixing the issue:

```bash
# Re-run test with crash input
cd test/fuzz
cat crashes/protocol-http/crash-DEADBEEF.txt | .testme/proto-http

# Should not crash anymore
```

## Performance Tuning

### Parallel Execution

Run multiple fuzzers concurrently:

```json5
{
    execution: {
        parallel: true,
        worker: 8,  // Run 8 fuzzers in parallel
    }
}
```

### Fuzzing Iterations

Adjust based on CI/CD budget:

```bash
# Quick smoke test (CI on every commit)
TESTME_ITERATIONS=1000 tm

# Normal testing (CI nightly)
TESTME_ITERATIONS=50000 tm

# Intensive testing (manual/weekly)
TESTME_ITERATIONS=1000000 tm
```

## Best Practices

1. **Start with Corpus** - Always seed with valid inputs
2. **Enable Sanitizers** - Use ASan/UBSan to catch issues
3. **Set Timeouts** - Prevent infinite loops
4. **Deduplicate Crashes** - Use hashing to find unique issues
5. **Minimize Reproducers** - Reduce crash inputs to minimal size
6. **Continuous Fuzzing** - Run in CI/CD nightly
7. **Triage Quickly** - Fix crashes as they're found
8. **Update Corpus** - Add new test cases from production

## Troubleshooting

### Fuzzer Won't Connect

```bash
# Check if web server is running
curl http://localhost:4200/

# Check fuzz ports
netstat -an | grep 4200

# Restart setup
cd test/fuzz
../setup.sh
```

### No Crashes Found

Good! But verify:

```bash
# Try more iterations
TESTME_ITERATIONS=100000 tm proto-http

# Try different mutation strategies
```

### Too Many Crashes

```bash
# Deduplicate
cd crashes/protocol-http
for f in crash-*; do
    md5sum "$f"
done | sort | uniq -d -w 32

# Analyze most common
ls -lS crashes/protocol-http/ | head -10
```

## References

- [../../doc/PLAN.md](../../doc/PLAN.md) - Complete fuzzing plan, implementation guide, and task tracking
- [../../doc/DESIGN.md](../../doc/DESIGN.md) - Architecture and Safe Runtime patterns
- [../../doc/CHANGELOG.md](../../doc/CHANGELOG.md) - Change history
- [fuzzLib.h](fuzzLib.h) - Fuzzing API documentation
- [OWASP Fuzzing Guide](https://owasp.org/www-community/Fuzzing)
- [libFuzzer Tutorial](https://llvm.org/docs/LibFuzzer.html)
- [AFL++ Documentation](https://aflplus.plus/)

## Support

For issues or questions:
1. Check existing crashes in `crashes/`
2. Review fuzzer logs in `fuzz-log.txt`
3. Enable verbose mode: `tm -v`
4. Consult [../../doc/PLAN.md](../../doc/PLAN.md) for architecture and implementation status
