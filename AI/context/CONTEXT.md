# Module Context Documentation

This directory contains detailed context documentation for each module used by the Ioto Agent. These documents provide comprehensive information about the architecture, usage, and integration of each module.

## Module Documentation Index

### Foundation Modules

- **[r.md](r.md)** - Safe Runtime Foundation
  - Memory management and allocation
  - Safe string operations
  - Fiber-aware I/O primitives
  - Cross-platform types and utilities
  - Core runtime functions

- **[json.md](json.md)** - JSON5/JSON6 Parser
  - JSON parsing and manipulation
  - JSON5 support (comments, trailing commas)
  - Query language for nested data
  - Date/time handling

- **[osdep.md](osdep.md)** - OS Abstraction Layer
  - Platform-specific abstractions
  - Cross-platform types
  - Operating system dependencies
  - Portability layer

- **[uctx.md](uctx.md)** - User Context and Fibers
  - Fiber coroutine system
  - Context switching
  - Stack management
  - Cooperative multitasking

### Cryptography and Security

- **[crypt.md](crypt.md)** - Cryptographic Functions
  - TLS/SSL support
  - Certificate management
  - Encryption and hashing
  - Secure random number generation
  - OpenSSL and MbedTLS integration

### Data Management

- **[db.md](db.md)** - Embedded Database
  - Schema-based data storage
  - Cloud synchronization
  - JSON-based data model
  - Query and update operations

### Communication Protocols

- **[mqtt.md](mqtt.md)** - MQTT Client
  - MQTT protocol implementation
  - AWS IoT Core integration
  - Publish/Subscribe operations
  - QoS support and reconnection

- **[web.md](web.md)** - Embedded Web Server
  - HTTP/HTTPS server
  - RESTful API support
  - Static file serving
  - Session management
  - Signature-based validation

- **[url.md](url.md)** - HTTP Client
  - HTTP/HTTPS client requests
  - Connection management
  - Redirect handling
  - Fiber-aware operations

- **[websock.md](websock.md)** - WebSocket Support
  - WebSocket protocol
  - Bi-directional communication
  - Integration with web server

### AI Integration

- **[openai.md](openai.md)** - OpenAI API Support
  - OpenAI API client
  - Chat completions
  - Streaming responses
  - AI-enabled applications

## Usage Guidelines

### For AI Agents and Developers

When working with specific modules:

1. **Start with the relevant module documentation** in this directory
2. **Refer to the main DESIGN.md** for integration architecture
3. **Check the module's source directory** (`src/` or `paks/`) for implementation details
4. **Review the module's CLAUDE.md** in its source directory for additional context

### Module Dependencies

Understanding the dependency hierarchy:

```
Applications (apps/)
    ↓
Agent Integration (src/agent/)
    ↓
Service Modules (mqtt, db, web, url, websock, openai)
    ↓
Foundation (r, json, crypt, uctx)
    ↓
OS Abstraction (osdep)
```

Modules at lower levels are used by modules at higher levels. When working with a module, ensure you understand its dependencies.

### Common Patterns

All modules follow these patterns:

- **Safe Runtime**: All modules use R runtime functions (r.md)
- **JSON Configuration**: Most modules use JSON5 for configuration (json.md)
- **Fiber-Aware**: All I/O operations are fiber-aware (uctx.md)
- **Platform Abstraction**: All use osdep types and functions (osdep.md)
- **Security**: TLS/SSL through crypt module where applicable (crypt.md)

## Related Documentation

- **[../designs/DESIGN.md](../designs/DESIGN.md)** - Overall architecture and design
- **[../plans/PLAN.md](../plans/PLAN.md)** - Current development plans
- **[../../CLAUDE.md](../../CLAUDE.md)** - Project-level documentation
- **[../../paks/*/CLAUDE.md](../../paks/)** - Individual module source documentation

## Maintenance

This documentation should be updated when:

- New modules are added to the agent
- Module interfaces change significantly
- Integration patterns are modified
- New dependencies are introduced

## Recent Activity

### Safe Runtime and Web Module Updates (December 19, 2025)

Updates to Safe runtime API documentation and web module platform support.

**Safe Runtime (R) Changes**:
- `rRunEvent` now documented as thread-safe
- Removed Red/Black tree from r module (moved to db module)
- Removed `R_USE_RB` compile-time flag

**Database Module Changes**:
- Red/Black tree implementation moved from r module to db module
- Added RbTree, RbNode types and associated functions
- Includes `rbAlloc`, `rbFree`, `rbInsert`, `rbRemove`, `rbLookup`, etc.

**Web Module Changes**:
- Added platform-specific buffer sizing macros for embedded platforms
- New macros: `WEB_BUF_BOOST_2X`, `WEB_BUF_BOOST_4X`, `WEB_BUF_BOOST_16X`
- ESP32/FreeRTOS/VxWorks use base `ME_BUFSIZE` for memory constraints
- Desktop/server platforms use boosted sizes (2x, 4x, 16x) for performance
- Updated `webValidateSignature` documentation for clarity

**URL Module Changes**:
- Added new test for connection reuse (`test/url/reuse.tst.c`)

**Files Modified**:
- `include/r.h`, `lib/rLib.c` - Removed Red/Black tree
- `include/db.h`, `lib/dbLib.c` - Added Red/Black tree
- `include/web.h`, `lib/webLib.c` - Buffer sizing macros
- `paks/r/`, `paks/db/`, `paks/web/`, `paks/url/` - Updated pak distributions

**Commit**: `a269d9bd` - DEV: update Safe runtime rWakeup API and web webValidateSignature

---

### README Documentation Improvements (December 18, 2025)

Comprehensive cleanup of README.md for improved readability and correctness.

**Line Wrapping**:
- Wrapped all lines to 120 character limit for better readability
- Maintains consistent formatting throughout the document

**Spelling Fixes**:
- "extentension" → "extension"
- "developement" → "development"

**Grammar Corrections**:
- "a HTTP" → "an HTTP" (vowel sound)
- "a OPTIMIZE" → "an OPTIMIZE" (vowel sound)
- "What is unique about Ioto, is that" → removed errant comma
- "Demonstrate using" → "Demonstrates using" (parallel structure)
- "little or not effort" → "little or no effort"
- "first time build" → "initial build"
- "on windows the command line" → "on Windows from the command line"
- "It will define use local directories" → "It will use local directories"

**Consistency Improvements**:
- "FREERTOS" → "FreeRTOS" (link text)
- "SubSystem" → "Subsystem" (standard capitalization)
- "cloud based" → "cloud-based" (hyphenated for consistency)
- "lower performing" → "lower-performing" (compound adjective)

**Sentence Completions**:
- Fixed incomplete sentence: "To link, reference the Ioto library"
- Fixed incomplete phrase: "unless the built app requires."
- Fixed awkward colon usage: "skip these steps: If" → "skip these steps. If"

**Files Modified**:
- `README.md`

---

### FreeRTOS Fiber Support and OS Abstraction (December 17, 2025)

Major improvements to FreeRTOS support and cross-platform OS detection.

**Uctx FreeRTOS Implementation**:
- Rewrote FreeRTOS context switching with semaphore-based synchronization
- Each fiber context has a mutex and condition semaphore for cooperative scheduling
- Implemented predicate-based wait functions (`resumed_or_done`, `resumed`)
- Tasks block on condition semaphore until explicitly resumed via swapcontext
- Proper lifecycle: main fiber uses current task, child fibers spawn new tasks

**OS Type Constants**:
- Added `ME_OS_*` constants to `osdep.h` for compile-time OS detection
- ME_OS_MACOSX, ME_OS_LINUX, ME_OS_FREEBSD, ME_OS_WINDOWS, etc.
- Enables cleaner platform-specific code with numeric comparisons

**Fiber Stack Configuration**:
- `ME_FIBER_DEFAULT_STACK` now integrates with ESP32's `CONFIG_ESP_MAIN_TASK_STACK_SIZE`
- Platform-specific defaults: 64KB (64-bit), 32KB (32-bit)
- Moved stack defines earlier in r.h for broader visibility

**New Demo App**:
- Added `apps/demo/freertos/` with FreeRTOSConfig.h, Makefile, main.c
- Complete example for FreeRTOS POSIX/GCC integration

**Files Modified**:
- `lib/uctxLib.c`, `paks/uctx/dist/uctxLib.c` - FreeRTOS implementation
- `include/osdep.h`, `paks/osdep/dist/osdep.h` - OS constants
- `include/r.h` - Stack configuration
- `README-FREERTOS.md`, `README-ESP32.md` - Simplified docs

**Commit**: `77d01e39` - DEV: improve FreeRTOS fiber support and OS abstraction

---

### FreeRTOS and ESP32 Documentation Update (December 11, 2025)

Improved step-by-step documentation for building Ioto on FreeRTOS and ESP32 platforms.

**README-FREERTOS.md Changes**:
- Added "Getting FreeRTOS" section with git clone instructions
- Added "Project Structure" diagram showing expected directory layout
- Added "Finding Your App Makefile" section with platform-specific paths
- Fixed Makefile example (`//` comments → `#` comments)
- Fixed function name typos (`iotStart` → `ioStart`, `iotStop` → `ioStop`)
- Fixed typo "performace" → "performance"

**README-ESP32.md Changes**:
- Added "Getting ESP-IDF" section with install steps for Ubuntu/Debian and macOS
- Added "Project Structure" diagram showing myproject layout
- Added WiFi configuration code example showing `#define` statements
- Converted build instructions to numbered workflow steps
- Streamlined redundant ESP-IDF sourcing instructions

**Files Modified**:
- `README-FREERTOS.md`
- `README-ESP32.md`

---

### Builder Configuration Update (December 10, 2025)

Enhanced builder configuration availability for provision-only builds.

**Changes**:
- Enable `cmdBuilder` configuration when `SERVICES_REGISTER` is defined (not just `SERVICES_CLOUD`)
- Moved `cmdBuilder` member outside `#if SERVICES_CLOUD` block in `ioto.h`
- Changed `#if SERVICES_CLOUD` to `#if SERVICES_REGISTER` in `setup.c` for builder initialization
- Enable `SERVICES_REGISTER` when either `SERVICES_PROVISION` or `SERVICES_CLOUD` is set

**Documentation**:
- Added README section for running standalone web server (`web` program)
- Useful for local-only management without cloud or embedded database

**Commit**: `f178a94d` - DEV: enable builder config for register service and document standalone web server

---

### Version 3.0.0 Release (December 9, 2025)

Major release with performance optimizations and API enhancements.

**Key Changes**:
- Socket accept optimization with `R_WAIT_MAIN_FIBER` flag
- New `rGrowBufSize()` for growing buffers to specific sizes
- New `rGetSocketLimit()` / `rSetSocketLimit()` for runtime socket limits
- New `webReadDirect()` for zero-copy body reading
- New `webWriteResponseString()` for efficient static responses
- Fixed Windows pollFds clearing in `rFreeWait()`
- Dynamic poll table growth for Windows/WSAPoll

**API Changes**:
- `rSetWaitHandler()` signature changed: added `flags` parameter
- Added `flags` field to `RWait` structure
- New constant `R_WAIT_MAIN_FIBER` for main fiber execution

**Documentation Updated**:
- [../logs/CHANGELOG.md](../logs/CHANGELOG.md) - Changelog entry for v3.0.0

**Status**: Version 3.0.0 released.

---

**Last Updated:** 2025-12-19
