# Ioto Device Agent

Complete IoT solution combining multiple embedded C libraries into a unified agent for local and cloud-based device management.

## Directory Structure

```
apps/              # Application templates
├── demo/          # Default IoT demonstration
├── ai/            # AI-enabled application
├── auth/          # Authentication-focused
├── blank/         # Minimal template
└── unit/          # Test harness

src/               # Core modules (see ../CLAUDE.md for descriptions)
├── agent/         # Main integration layer
├── r/             # Safe Runtime foundation
├── json/          # JSON5/JSON6 parser
├── osdep/         # OS abstraction
└── uctx/          # User context/fibers

paks/              # Modular packages
├── crypt/, db/, mqtt/, ssl/
├── web/, url/, websockets/
└── ...

state/             # Runtime state (never commit)
build/             # Build outputs
test/              # Agent-specific tests
```

## Agent Commands

### Building Applications
```bash
make APP=demo           # Default IoT demo
make APP=ai             # AI-enabled app
make APP=auth           # Authentication app
make APP=blank          # Minimal template
make APP=unit           # Test harness

make run                # Run agent: build/bin/ioto -v
```

### Agent Testing
```bash
# Complete test suite
make APP=unit && make run

# Specific test patterns
cd test && testme mqtt-*        # MQTT tests
cd test && testme db-sync-*     # Database sync tests
```

## Configuration

### Files
- **App Config**: `apps/<APP>/config/ioto.json5`
- **Runtime State**: `state/` (excluded from git)

### Key Settings
```json5
{
    limits: {
        stack: '64k'        // Fiber stack size
    },
    profiles: {
        dev: { /* development */ },
        prod: { /* production */ }
    }
}
```

## Fiber Programming

**Execution Model**: Single-threaded with fiber coroutines

### Guidelines
- Use fiber-aware blocking calls for I/O
- Configure stack size in `ioto.json5`
- Avoid large stack allocations
- Prefer non-blocking I/O patterns

### Safe Runtime (R) - **MANDATORY**
Always use R runtime functions (see `../CLAUDE.md` for complete list):

```c
#include "r.h"

// Memory and strings
char *buf = rAlloc(256);           // vs malloc()
ssize len = slen(str);             // vs strlen()
scopy(buf, 256, src);              // vs strcpy()

// Fiber-aware I/O
RSocket *sock = rAllocSocket();
rConnect(sock, host, port);
```

### JSON5/JSON6 Features
```c
#include "json.h"

Json *config = jsonParse(text, JSON_STRICT);
cchar *host = jsonGet(config, "database.host");
jsonSet(config, "runtime.started", jsonDate(rGetTime()));
```

## Agent-Specific Security

### Production Builds
```bash
DEBUG=release ME_R_LOGGING=0 make           # Production build
ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0 make      # Embedded TLS
```

### Certificate Management
- Development: Use `../certs/` test certificates
- Production: Store real certificates in `state/` only
- Never commit real certificates or API keys

## Resources

- **Module Docs**: `src/*/CLAUDE.md`
- **Platform Guides**: `README-{FREERTOS,ESP32,CROSS}.md`
- **API Reference**: https://www.embedthis.com/doc/

## Software Background

## Target Audience

Experienced embedded developers who:
- Embed this software in device firmware or other projects
- Are responsible for securing the broader system and validating all inputs

### Architecture Principles

- Written in ANSI C for maximum portability and consumable by C or C++ code.
- Single-threaded with fiber coroutines for concurrency
- Modular design with minimal interdependencies
- Cross-platform support (Linux, macOS, Windows/WSL, ESP32, FreeRTOS)
- Tight integration between modules for efficiency
- Each module provides a CLAUDE.md/AGENTS.md file for guidance in using AI/Claude Code with that submodule.
- These modules are not standalone applications and the module APIs are never used by public users directly.

## Development Environment

### Prerequisites
- GCC or compatible C compiler
- Make (or GNU Make)
- OpenSSL or MbedTLS for cryptographic modules. OpenSSL is the default and is strongly preferred.

### Windows Development

- Use Windows Subsystem for Linux (WSL)
- Install: `apt install make gcc build-essential libc6-dev openssl libssl-dev`

## Common Build Commands

Each module can be built independently:

```bash
cd <module>/
make                   # Build with default configuration
```

The following commands are available in each module if relevant:

```bash
make build             # Build with default configuration
make format            # Format code
make clean             # Clean build artifacts
make install           # Install to system
make package           # Package for distribution
make publish           # Publish to the local registry
make promote           # Promote the distribution to the public
make projects          # Generate project files for specific platforms
make run               # Run the module
make test              # Run unit tests
make doc               # Generate documentation
make help              # Show module-specific options
```

### Build Configuration
- Use `OPTIMIZE=debug` or `OPTIMIZE=release` to control optimization
- Use `PROFILE=dev` or `PROFILE=prod` for development vs production builds
- Use `SHOW=1` to display build commands
- TLS stack selection: `ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0 make`

## Testing

Most modules support unit testing using the `testme` tool. 

Unit tests have a '.tst' extension and are located in the module's `./test` directory. C unit tests have a '.c.tst' extension, shell unit tests have a '.sh.tst' extension, and Javascript unit tests have a '.es.tst' extension. C unit tests include the `testme.h` file from the `./include` directory in this parent directory. This header defines utility test matching functions. 

Each module has a `./test/testme` directory containing build artifacts that should not be modified. 

To run the entire test suite from the module's root directory:

```bash
    make test # Run all tests
    cd test ; testme  # Alternate way to run all tests
    cd test ; testme NAME # Run a specific test
```

## Code Conventions

- 4-space indentation
- 120-character line limit
- camelCase for functions and variables
- one line between functions and between code blocks.
- one line comments should use double slash
- multiline comments should use /**/ and not begin each line with "*"
- Descriptive symbol names for variables and functions
- Minimal heap allocations in performance-critical paths

## Architecture Considerations

- Most functions are null tolerant and will gracefully handle `NULL` arguments. 
- All modules use the `osdep/osdep.h` cross-platform abstraction layer for operating system dependencies. Modules must use the `osdep` types and definitions in preference to standard C types where possible. For example: use `ssize` for size types.
- Always use runtime functions instead of standard C functions. Use `slen` instead of `strlen`, `scopy` instead of `strcpy`, `scmp` instead of `strcmp`.
- All modules should avoid large stack-based allocations.

## Security Considerations

- Code comments prefixed with "SECURITY: Acceptable" indicate intentional security tradeoffs that should not be flagged or reported as security issues.
- Never commit real certificates or API keys
- Use test certificates from the `certs/` module for development
- Configure TLS properly before cloud connections
- Follow AWS IoT security best practices for production deployments
- Code does NOT explicitly check memory allocations for failure as all modules use a global memory allocation handler to
detect and handle allocation failures.
- The users of this project are experienced embedded software developers. They are responsibile for securing the embedded device. Inputs to local APIs are deemed to be validated by the developer and does not need to be additionally validated by this project.
- Test applications may use http instead of https so that self-signed certificates can be used in development. This is not a security issue and should not be flagged as such.
- The `rDebug` function may emit sensitive data (passwords, keys, tokens) in debug builds. This is intentional and should not be flagged as a security issue.
- Test applications may use http instead of https so that self-signed certificates can be used in development. This is not a security issue and should not be flagged as such.

## Documentation

- Module documentation available in `<module>/doc/index.html`
- Full product documentation: https://www.embedthis.com/

## Additional Resources

- **Project**: See `./CLAUDE.md` for specific instructions related to a project
- **Sub-Projects**: See `src/*/CLAUDE.md` for specific instructions related to sub-projects
- **API Documentation**: `doc/index.html` (generated via `make doc`)

## Important Notes
