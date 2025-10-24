# Ioto Agent Design Documentation

**Last Updated:** 2025-10-09

## Contents

- [Architecture Overview](#architecture-overview)
- [Core Components](#core-components)
- [Module Documentation](#module-documentation)
- [Integration Architecture](#integration-architecture)
- [Build System Design](#build-system-design)
- [Application Framework](#application-framework)
- [Fiber Execution Model](#fiber-execution-model)

---

## Architecture Overview

The Ioto Device Agent is a complete IoT solution combining multiple embedded C libraries into a unified agent for both local and cloud-based device management.

### Design Philosophy

1. **Single-Threaded with Fiber Coroutines**: Uses fiber coroutines instead of threads for concurrency
2. **Modular Integration**: Combines independent modules (r, json, crypt, db, mqtt, web, url, websock, openai) into a cohesive agent
3. **Minimal Footprint**: Designed for embedded systems with limited resources
4. **Portability**: Written in ANSI C for maximum cross-platform support
5. **Safe Runtime Foundation**: All components built on the R (Safe Runtime) foundation

### Target Platforms

- **Production**: Linux, macOS, ESP32, FreeRTOS, VxWorks
- **Development**: Windows via WSL, native Windows with Visual Studio
- **Architectures**: x86, x64, ARM, ARM64, RISC-V, RISC-V64

---

## Core Components

### 1. Safe Runtime (R)

**Location**: `src/r/` (via paks)

Foundation layer providing:
- Memory management with allocation failure handling
- Safe string operations (slen, scopy, scmp)
- Cross-platform types and utilities
- Fiber-aware I/O primitives

**Design Principles**:
- All functions are null-tolerant
- No explicit memory allocation failure checks (handled globally)
- Replaces all standard C library functions for safety

### 2. JSON5/JSON6 Parser

**Location**: `paks/json/`

Advanced JSON parsing with:
- JSON5 support (comments, trailing commas, unquoted keys)
- JSON6 extensions for embedded systems
- Query language for nested data
- Zero-copy parsing where possible

**Key Features**:
- Integrated date/time handling via `jsonDate()`
- Path-based access: `jsonGet(config, "database.host")`
- Modification support: `jsonSet()`

### 3. Cryptography Layer

**Location**: `paks/crypt/`

**TLS Stack Options**:
- OpenSSL (default, production recommended)
- MbedTLS (embedded systems, ESP32 default)

**Capabilities**:
- TLS/SSL connections
- Certificate management
- Secure random number generation
- Hashing and encryption

### 4. Embedded Database

**Location**: `paks/db/`

**Features**:
- Schema-based embedded database
- Transparent cloud synchronization
- JSON-based data model
- Optimized for device state management

### 5. MQTT Client

**Location**: `paks/mqtt/`

**Design**:
- Fiber-aware asynchronous operations
- AWS IoT Core integration
- Publish/Subscribe with QoS support
- Automatic reconnection

### 6. Embedded Web Server

**Location**: `paks/web/`

**Features**:
- HTTP/HTTPS server
- RESTful API support
- Static file serving
- Signature-based API validation
- Session management

### 7. HTTP Client

**Location**: `paks/url/`

**Capabilities**:
- HTTP/HTTPS client requests
- Fiber-aware blocking I/O
- Connection pooling
- Redirect handling

### 8. WebSocket Support

**Location**: `paks/websock/`

**Features**:
- WebSocket protocol implementation
- Integration with web server
- Bi-directional communication

### 9. OpenAI Integration

**Location**: `paks/openai/`

**Provides**:
- OpenAI API client
- AI-enabled device applications
- Streaming response support

---

## Module Documentation

Detailed module-specific documentation is available in [../context/](../context/):

### Foundation Modules
- **[r.md](../context/r.md)** - Safe Runtime: Memory management, safe strings, fiber-aware I/O, runtime utilities
- **[json.md](../context/json.md)** - JSON5/JSON6 Parser: Parsing, queries, date handling, embedded-friendly JSON
- **[osdep.md](../context/osdep.md)** - OS Abstraction: Platform types, cross-platform APIs, portability layer
- **[uctx.md](../context/uctx.md)** - User Context/Fibers: Coroutines, context switching, stack management

### Security
- **[crypt.md](../context/crypt.md)** - Cryptography: TLS/SSL, certificates, encryption, hashing, OpenSSL/MbedTLS

### Data Management
- **[db.md](../context/db.md)** - Embedded Database: Schema storage, cloud sync, JSON data model, queries

### Communication
- **[mqtt.md](../context/mqtt.md)** - MQTT Client: MQTT protocol, AWS IoT, pub/sub, QoS, reconnection
- **[web.md](../context/web.md)** - Web Server: HTTP/HTTPS, REST APIs, static files, sessions, signatures
- **[url.md](../context/url.md)** - HTTP Client: HTTP/HTTPS requests, connections, redirects, fiber I/O
- **[websock.md](../context/websock.md)** - WebSockets: WebSocket protocol, bidirectional communication

### AI Integration
- **[openai.md](../context/openai.md)** - OpenAI Support: OpenAI API, chat completions, streaming, AI apps

See [../context/CONTEXT.md](../context/CONTEXT.md) for the complete module documentation index and usage guidelines.

---

## Integration Architecture

### Layer Dependencies

```
┌─────────────────────────────────────┐
│      Application Layer (apps/)       │
│   demo, ai, auth, blank, unit        │
└─────────────────────────────────────┘
           ↓
┌─────────────────────────────────────┐
│    Integration Layer (src/agent/)    │
│   cloud, provision, sync, update     │
└─────────────────────────────────────┘
           ↓
┌─────────────────────────────────────┐
│   Service Modules (paks/)            │
│   web, mqtt, db, url, websock     │
└─────────────────────────────────────┘
           ↓
┌─────────────────────────────────────┐
│   Foundation Layer                   │
│   r (runtime), json, crypt, uctx     │
└─────────────────────────────────────┘
           ↓
┌─────────────────────────────────────┐
│   OS Abstraction (osdep)             │
└─────────────────────────────────────┘
```

### Component Interactions

1. **Application → Agent**: Apps call `ioRun()` to start the agent
2. **Agent → Services**: Conditionally loads services based on `ioto.json5`
3. **Services → Foundation**: All services use R runtime and JSON library
4. **Foundation → OSDEP**: All platform-specific code isolated in osdep

---

## Build System Design

### Multi-Stage Build Process

1. **Configuration Selection**: APP parameter selects application
2. **Config Copy**: App config copied from `apps/APP/config/` to `state/`
3. **Conditional Compilation**: Services compiled based on config settings
4. **Platform Detection**: Automatic OS/architecture detection
5. **Project Generation**: Platform-specific makefiles/projects generated

### Build Profiles

**Development Profile** (`profile: "dev"`):
- Debug symbols enabled
- Local directories for state/config
- Verbose logging
- Stack checking available

**Production Profile** (`profile: "prod"`):
- Optimized code
- System directories
- Minimal logging
- Reduced binary size

### Platform-Specific Builds

**Linux/macOS**: Direct make
**Windows WSL**: make via WSL
**Windows Native**: nmake or Visual Studio
**ESP32**: ESP-IDF integration
**FreeRTOS**: Cross-compilation with custom toolchain

---

## Application Framework

### Application Structure

```
apps/APP/
├── config/
│   ├── ioto.json5      # Main configuration
│   ├── web.json5       # Web server config
│   ├── schema.json5    # Database schema
│   └── device.json5    # Device registration
├── site/               # Web documents
└── src/
    └── app.c           # Application code
```

### Application Lifecycle

1. **Initialization**: `ioStartRuntime()`
2. **Service Loading**: Based on `ioto.json5` settings
3. **App Callback**: `ioStart()` invoked when ready
4. **Event Loop**: `ioRun()` executes fiber scheduler
5. **Shutdown**: `ioStop()` callback, `ioStopRuntime()`

### Configurable Services

Services enabled via `ioto.json5`:

- `ai`: OpenAI integration
- `database`: Embedded database
- `mqtt`: MQTT client
- `provision`: Cloud provisioning
- `shadow`: AWS IoT shadow
- `sync`: Database cloud sync
- `url`: HTTP client
- `web`: Web server

---

## Fiber Execution Model

### Design Rationale

**Why Fibers?**
- Simpler than threads (no locking, race conditions)
- More efficient than callbacks (readable synchronous code)
- Lower memory overhead than threads
- Deterministic scheduling for embedded systems

### Fiber Stack Management

**Configuration**: `limits.stack` in `ioto.json5`

**Recommendations**:
- 32KB minimum for 32-bit systems
- 64KB minimum for 64-bit systems
- Larger for debug builds (debugger overhead)
- Avoid large stack allocations
- Prefer heap for large data structures

### Fiber-Aware I/O

All I/O operations are fiber-aware and yield control:

```c
// Network I/O
rConnect(sock, host, port);     // Yields during connection
rRead(sock, buf, size);          // Yields waiting for data

// File I/O
rReadFile(path, &len);           // Yields during read

// Timer operations
rSleep(milliseconds);            // Yields for duration
```

### Concurrency Model

- **Single-threaded**: No thread synchronization needed
- **Cooperative**: Fibers yield explicitly
- **Event-driven**: I/O operations drive scheduling
- **Stack-per-fiber**: Each fiber has isolated stack

---

## Security Architecture

### Certificate Management

**Development**:
- Test certificates in `state/certs/`
- Self-signed certificates acceptable
- HTTP allowed for testing

**Production**:
- Real certificates stored in `state/` only
- HTTPS required for cloud connections
- Certificate validation enforced
- Never commit certificates to source control

### Trust Model

**Assumptions**:
- Experienced embedded developers as users
- Developers validate all external inputs
- Developers secure device configuration
- File system and DNS integrity maintained by platform
- Build configuration secured by developers

**Security Features**:
- TLS for all cloud communications
- Certificate-based authentication
- Signature validation for REST APIs
- Session management for web server

### Debug vs Release

**Debug Builds**:
- May emit sensitive data via `rDebug()`
- Stack checking available
- Additional logging
- Performance impact acceptable

**Release Builds**:
- Minimal logging
- No sensitive data emission
- Optimized code
- Stack checking disabled

---

## Testing Architecture

### Test Harness Application

**Location**: `apps/unit/`

**Purpose**: Comprehensive unit test execution

**Test Types**:
1. **C Tests** (`.tst.c`): Low-level module testing
2. **Shell Tests** (`.tst.sh`): Integration testing
3. **JavaScript/TypeScript Tests** (`.tst.js`, `.tst.ts`): High-level testing

### Test Execution

```bash
make APP=unit && make run    # Run all tests
cd test && tm                # Direct test execution
cd test && tm mqtt-*         # Pattern-based execution
```

### Test Design Principles

- Tests must run in parallel
- Unique filenames using `getpid()`
- Clean up temporary files
- Isolated test environments
- No inter-test dependencies

---

## Future Design Considerations

### Extensibility

- Plugin architecture for custom services
- Dynamic service loading
- Custom protocol handlers
- Extended authentication mechanisms

### Performance Optimization

- Zero-copy buffer management
- Connection pooling optimization
- Database query optimization
- Reduced memory allocations

### Platform Support

- Additional RTOS targets
- Arduino framework integration
- Zephyr OS support
- Additional CPU architectures

---

## Related Documentation

- **[Module Context Documentation](../context/CONTEXT.md)** - Detailed module-specific documentation
- **[Build Procedures](../procedures/PROCEDURE.md)** - Build and testing procedures
- **[Development Plans](../plans/PLAN.md)** - Current development plans
- **[Change Log](../logs/CHANGELOG.md)** - Project change history
- **[Security Design](SECURITY.md)** - Security architecture and guidelines
