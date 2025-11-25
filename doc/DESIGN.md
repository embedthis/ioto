# Ioto Device Agent Design Document

## Purpose

The EmbedThis Ioto Device Agent is a complete IoT solution that enables cloud-based and local device management for embedded systems. It provides an integrated suite of capabilities including HTTP web server, HTTP client, MQTT client, embedded database, and JSON data management with AWS IoT cloud integration.

**Target Audience**: Experienced embedded developers who embed this software in device firmware and are responsible for system security and input validation.

**Key Differentiators**:
- Tight integration between components for maximum efficiency
- Transparent cloud database synchronization
- Single-threaded fiber coroutine architecture for minimal resource usage
- Modular design allowing selective feature inclusion
- Cross-platform support (Linux, macOS, Windows/WSL, ESP32, FreeRTOS)

## Architecture

### High-Level Architecture

Ioto uses a layered modular architecture where upper modules consume lower modules:

```
┌─────────────────────────────────────────────────────────┐
│                   Ioto Device Agent                     │
│    (Cloud Sync, Provisioning, OTA Updates, Metrics)    │
└─────────────────────────────────────────────────────────┘
                           │
    ┌──────────────────────┼──────────────────────┐
    │                      │                      │
┌───▼────┐  ┌──────▼──────┐  ┌────▼─────┐  ┌────▼─────┐
│  Web   │  │    MQTT     │  │   DB     │  │  OpenAI  │
│ Server │  │   Client    │  │Embedded  │  │   API    │
└───┬────┘  └──────┬──────┘  └────┬─────┘  └────┬─────┘
    │              │              │              │
┌───▼────┐  ┌──────▼──────┐  ┌────▼─────┐  ┌────▼─────┐
│  URL   │  │WebSockets   │  │  JSON5   │  │  Crypt   │
│ Client │  │             │  │  Parser  │  │TLS Stack │
└───┬────┘  └──────┬──────┘  └────┬─────┘  └────┬─────┘
    │              │              │              │
    └──────────────┴──────────────┴──────────────┘
                           │
         ┌─────────────────┴─────────────────┐
         │       Safe Runtime (R)            │
         │  Memory, Strings, Fibers, I/O     │
         └─────────────────┬─────────────────┘
                           │
         ┌─────────────────▼─────────────────┐
         │   OS Abstraction (osdep)          │
         │  Platform-specific implementations │
         └───────────────────────────────────┘
```

### Core Modules

**Safe Runtime (R)** - Foundation layer providing:
- Memory management with centralized allocation failure handling
- Safe string operations (slen, scopy, scmp replacing strlen, strcpy, strcmp)
- Fiber coroutine implementation for concurrency
- Event loop and I/O multiplexing
- File and socket abstractions with fiber-aware blocking
- Logging, timing, and hash/list/tree data structures

**JSON5/JSON6** - Advanced JSON parsing:
- Extended JSON5 syntax support (comments, trailing commas, unquoted keys)
- JSON6 with date primitives and binary data
- Query language for deep property access
- Schema validation

**Cryptography (crypt)** - Security layer:
- TLS/SSL support via OpenSSL or MbedTLS
- Certificate management
- Secure communications for cloud connectivity

**Embedded Database (db)**:
- Lightweight key-value and document storage
- Transparent cloud synchronization
- Local caching with conflict resolution
- Schema-based data management

**MQTT Client**:
- MQTT protocol implementation for pub/sub messaging
- AWS IoT integration
- Automatic reconnection and QoS handling

**Web Server**:
- Embedded HTTP/HTTPS server
- REST API support with signature validation
- Authentication and authorization
- Static file serving and dynamic content

**URL Client**:
- HTTP/HTTPS client for outbound requests
- AWS API integration
- Connection pooling and retry logic

**WebSockets**:
- Full-duplex communication protocol
- Real-time bidirectional messaging

**OpenAI Integration**:
- API client for OpenAI services
- AI-enabled device capabilities

### Directory Structure

```
agent/
├── apps/              # Application templates
│   ├── demo/          # Default IoT cloud demonstration
│   ├── ai/            # AI-enabled application
│   ├── auth/          # Authentication-focused
│   ├── blank/         # Minimal template
│   └── unit/          # Test harness
├── lib/               # Amalgamated library source files
│   ├── rLib.c         # Safe Runtime
│   ├── jsonLib.c      # JSON parser
│   ├── dbLib.c        # Database
│   ├── mqttLib.c      # MQTT client
│   ├── webLib.c       # Web server
│   ├── urlLib.c       # URL client
│   ├── websockLib.c   # WebSockets
│   ├── cryptLib.c     # Cryptography
│   ├── openaiLib.c    # OpenAI
│   └── uctxLib.c      # User context/fibers
├── paks/              # Modular packages (source)
├── state/             # Runtime state (never committed)
│   ├── certs/         # TLS certificates
│   ├── config/        # Configuration files
│   └── db/            # Database files
├── build/             # Build outputs
├── test/              # Agent test suite
├── scripts/           # Device management scripts (OTA)
└── projects/          # Generated IDE projects
```

## Design

### Concurrency Model: Fiber Coroutines

Ioto uses a **single-threaded** execution model with **fiber coroutines** instead of traditional threads or callbacks. This provides:

**Benefits**:
- Minimal memory overhead (configurable stack per fiber, typically 32-64KB)
- Simplified synchronization (no mutexes or locks needed)
- Deterministic behavior
- Ideal for resource-constrained embedded systems

**Implementation**:
- Fibers are user-space cooperative threads
- Fiber context switching via uctx module (user context)
- Event loop processes I/O events and schedules ready fibers
- Blocking operations (I/O, sleep) yield to other fibers

**Programming Model**:
```c
// Fiber-aware blocking call
RSocket *sock = rAllocSocket();
rConnect(sock, host, port);  // Yields while connecting
rRead(sock, buf, size);      // Yields while reading
```

### Memory Management

**Centralized Allocation**:
- All allocations via `rAlloc()` family (rAlloc, rAllocZeroed, rRealloc)
- Global memory handler detects allocation failures
- No need to check for NULL returns individually
- Memory debugging tracks allocations when ME_FIBER_CHECK_STACK enabled

**Safety Features**:
- Null-tolerant functions (`rFree(NULL)` is safe)
- `sclone(NULL)` returns empty string, never NULL
- Bounds-checked string operations (scopy, sncopy)
- Buffer overflow protection via dynamic RBuf type

### Configuration System

**JSON5-based Configuration**:
- Primary config: `ioto.json5`
- Device registration: `device.json5`
- Web server: `web.json5`
- Database schema: `schema.json5`
- Display UI: `display.json5`

**Profile System**:
- `dev` profile: Local directories, debug symbols, verbose logging
- `prod` profile: System directories, optimized builds, minimal logging
- Conditional properties applied based on active profile

**Build-time Configuration**:
- Services enabled/disabled via ioto.json5
- Selective compilation reduces binary size
- Makefile variables override settings (OPTIMIZE, PROFILE)

### Cloud Integration

**AWS IoT Platform**:
- Device provisioning via AWS IoT Core
- Dynamic certificate generation on claim
- MQTT connectivity to AWS endpoints
- Shadow state synchronization
- CloudWatch metrics and logs

**Registration and Provisioning**:
1. Device generates unique ID on first boot (or uses pre-configured ID)
2. Device registers with Builder service using product token
3. User claims device via Device App using claim ID
4. Device receives AWS certificates and endpoint configuration
5. Device connects to AWS IoT using MQTT over TLS

**Database Synchronization**:
- Local embedded database transparently syncs to cloud
- Bidirectional updates with conflict resolution
- Schema-driven data validation
- Efficient delta synchronization

### Security Architecture

**TLS Stack Selection**:
- OpenSSL (default, faster, larger)
- MbedTLS (compact, embedded-optimized)
- Switchable at build time

**Certificate Management**:
- Development: Test certificates in `paks/certs/`
- Production: Dynamic provisioning stores certs in `state/`
- Never commit real certificates or API keys to repository

**Secure Communications**:
- All cloud communication over TLS
- Certificate-based device authentication
- AWS IAM key integration for dedicated clouds
- Signature validation for REST APIs

**Security Assumptions**:
- Developers responsible for input validation
- File system integrity assumed secure
- DNS integrity out of scope
- Debug logging may expose sensitive data (production builds disable)

## Implementation

### Build System

**Make-based Build**:
- Top-level Makefile detects OS and CPU architecture
- Invokes platform-specific project Makefiles
- Copies selected app configuration to `state/`
- Supports cross-compilation

**Build Commands**:
```bash
make APP=demo                              # Build demo app
make OPTIMIZE=release PROFILE=prod         # Production build
make ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0    # Use MbedTLS
make SHOW=1                                # Verbose output
make clean build                           # Clean rebuild
```

**Platform Support**:
- Linux (GCC, Clang)
- macOS (Apple Clang)
- Windows (WSL, Visual Studio, nmake)
- ESP32 (ESP-IDF toolchain)
- FreeRTOS (cross-compilation)

### Testing Framework

**TestMe Integration**:
- Unit tests use TestMe framework (`tm` tool)
- C unit tests: `*.tst.c` files including `testme.h`
- Shell tests: `*.tst.sh` scripts
- JavaScript/TypeScript tests: `*.tst.js` / `*.tst.ts`

**Test Organization**:
- Tests in `test/` directory
- Configuration in `test/testme.json5`
- Build artifacts in `test/testme/` (auto-generated)
- Tests must be parallel-safe (use getpid() for unique filenames)

**Running Tests**:
```bash
make test                  # Run all tests
cd test && tm             # Alternate method
cd test && tm mqtt-*      # Pattern matching
make APP=unit && make run # Full integration test
```

### Application Templates

**demo** - Cloud messaging demonstration:
- Sends counter data to cloud via MQTT
- Database synchronization example
- Metrics generation

**ai** - OpenAI integration:
- AI API usage examples
- Natural language processing

**auth** - Authentication showcase:
- User/group authentication
- Session management
- Access control

**blank** - Minimal template:
- Empty slate for custom applications
- Basic initialization only

**unit** - Test harness:
- Comprehensive test suite
- All features enabled

### Embedding Ioto

**Library Integration**:
```c
#include "ioto.h"

int main() {
    ioStartRuntime(1);           // Initialize runtime
    ioRun(ioStart);              // Run until exit
    ioStopRuntime();             // Cleanup
}

int ioStart() {
    // User application code
    return 0;
}

void ioStop() {
    // Cleanup code
}
```

**Linking**:
```bash
gcc -o prog -I build/inc -L build/bin main.c -lioto -lssl -lcrypto
```

### Code Conventions

**Style Guidelines**:
- 4-space indentation (no tabs)
- 120-character line limit
- camelCase for functions and variables
- One blank line between functions
- Single-line comments use `//`
- Multi-line comments without leading `*`

**Mandatory Practices**:
- Use R runtime functions, never standard C (slen vs strlen)
- Use osdep types (ssize, not size_t)
- Declare variables at function top
- NULL-tolerant function design
- Minimal heap allocations in critical paths

**Documentation**:
- API documentation via Doxygen
- No `@return Void` in docs
- No `@defgroup` usage
- Test doc generation with `make doc`

### Deployment Workflow

**Development**:
1. Select application template: `make APP=demo`
2. Configure `ioto.json5` for required services
3. Build: `make`
4. Run locally: `make run`
5. Test: `make test`

**Production**:
1. Set product token in `device.json5`
2. Configure production profile in `ioto.json5`
3. Build optimized: `OPTIMIZE=release PROFILE=prod make`
4. Package: `make package`
5. Deploy to device hardware
6. Device registers and awaits claiming
7. User claims device via Device App
8. Device provisions certificates and connects to cloud

**Over-the-Air Updates**:
- OTA script in `scripts/`
- Update management via Builder
- Versioned firmware distribution
- Rollback support on failure

### Performance Characteristics

**Resource Usage**:
- Binary size: ~200KB-500KB (depending on enabled services)
- RAM: ~1MB minimum (varies with fiber stack configuration)
- Flash: ~500KB (with TLS stack)

**Optimization Strategies**:
- Selective service compilation
- Tight module integration eliminates abstraction overhead
- Single-threaded eliminates thread overhead
- Custom printf uses <1KB stack
- Fiber stacks configurable per application needs

**Benchmarks**:
- HTTP server: Thousands of requests/second on embedded ARM
- MQTT: Sub-millisecond message processing
- Database: Microsecond query times for small datasets
- Fiber context switch: Nanosecond scale

## References

- **API Documentation**: https://www.embedthis.com/doc/
- **Source Repository**: Contact EmbedThis for access
- **Builder Platform**: https://admin.embedthis.com/
- **Support**: support@embedthis.com

## Appendix: Module Dependencies

```
agent → web, mqtt, db, url, openai
web → crypt, json, r
mqtt → crypt, json, r
db → json, r
url → crypt, json, r
websock → crypt, json, r
openai → url, json, r
crypt → r, osdep
json → r
r → osdep, uctx
uctx → osdep
osdep → (platform libraries)
```
