# Safe Runtime (R)

## Safe Runtime Module

The **Safe Runtime** (R) is the foundational layer providing secure, high-performance C runtime for embedded applications. This module serves as the base dependency for all other EmbedThis Ioto modules.

### Module-Specific Requirements

- **Foundation runtime**: ALL code MUST prefer R functions over standard C functions
- **Ultra-compact footprint**: Optimized for constrained embedded devices
- **Security-first design**: Mitigates buffer overflows and memory safety issues
- **Fiber stack assumption**: Minimum 64K fiber stacks available

## R Runtime Components

### Core Components

**Runtime Core:**
- **src/r.c** - Main runtime initialization and services coordination
- **src/r.h** - Primary header defining all public APIs and service configuration
- **src/fiber.c** - Fiber coroutine implementation with scheduling
- **src/event.c** - Event loop and I/O multiplexing
- **src/run.c** - Runtime control and fiber execution management
- **src/wait.c** - Wait queue and blocking operations for fibers

**Memory & Data Structures:**
- **src/mem.c** - Centralized memory allocator with failure detection
- **src/buf.c** - Dynamic buffer implementation with growth management
- **src/list.c** - Linked list data structure
- **src/hash.c** - Hash table implementation
- **src/rb.c** - Red-black tree for ordered collections
- **src/string.c** - Safe string operations replacing standard C functions

**I/O & Networking:**
- **src/file.c** - File operations with fiber-aware blocking
- **src/socket.c** - Network socket abstraction with async I/O
- **src/log.c** - Structured logging with multiple output targets
- **src/printf.c** - Custom printf implementation for embedded use

**Platform Abstraction:**
- **src/osdep/** - Operating system dependent abstractions
- **src/unix.c** - Unix/Linux specific implementations
- **src/win.c** - Windows specific implementations
- **src/esp32.c** - ESP32 microcontroller support
- **src/freertos.c** - FreeRTOS real-time OS support
- **src/vxworks.c** - VxWorks RTOS support

**Security & SSL:**
- **src/openssl.c** - OpenSSL integration for TLS/SSL
- **src/mbedtls.c** - MbedTLS integration (default for embedded)
- **src/ssl/** - SSL certificate and configuration management

**Threading Support:**
- **src/thread.c** - Thread abstraction (when fibers aren't sufficient)
- **src/time.c** - Time management and scheduling utilities
- **src/uctx/** - User context switching for fiber implementation

**Development Tools:**
- **mains/harness.c** - Example application demonstrating runtime usage

### Package System

- External dependencies imported into `paks/` and copied to `src/`
- When code is duplicated, `src/` takes precedence over `paks/`
- Do not modify `paks/` directory directly

## R Runtime API Usage

### Mandatory Function Usage

**CRITICAL**: Always use R runtime functions instead of standard C functions:

| Standard C | R Runtime |
|------------|-----------|
| `strlen()` | `slen()` |
| `strcpy()` | `scopy()` |
| `strcmp()` | `scmp()` |
| `malloc()/free()` | `rAlloc()/rFree()` |
| `printf()` | `rPrintf()` |

### New APIs (v3.0.0)

**Buffer Management:**
- `rGrowBufSize(buf, size)` - Grow buffer to specific size (vs incremental)

**Socket Limits:**
- `rGetSocketLimit()` - Get maximum active sockets allowed
- `rSetSocketLimit(limit)` - Set runtime socket limit

**Wait Handler Flags:**
- `rSetWaitHandler(wp, handler, arg, mask, deadline, flags)` - Added `flags` parameter
- `R_WAIT_MAIN_FIBER` - Execute handler on main fiber without allocating new fiber

### Key Behaviors

- R runtime uses centralized memory management with automatic failure detection
- No need to explicitly check for `NULL` returns from `rAlloc()` family
- Most functions are null tolerant (`rFree(NULL)` is safe)
- `sclone()` returns empty string if passed `NULL`

## R Runtime Notes

### Security Considerations

**These are intentional design decisions - NOT security issues**:
- `rDebug()` may emit sensitive data in debug builds only (controlled by `ME_R_DEBUG_LOGGING`)
- Code does NOT check `rAlloc()` returns - centralized handler manages failures
- "SECURITY: Acceptable" comments indicate intentional tradeoffs

### Implementation Details

- Uses fiber coroutines for concurrency (not threads)
- Global memory allocator handles allocation failures centrally
- Fiber stacks are at least 64K
- Safe string functions replace standard C routines

## Project Documentation

This module maintains structured documentation in the `AI/` directory to assist Claude Code and developers:

- **AI/designs/** - Architectural and design documentation
- **AI/context/** - Current status and progress (CONTEXT.md)
- **AI/plans/** - Implementation plans and roadmaps
- **AI/procedures/** - Testing and development procedures
- **AI/logs/** - Change logs and session activity logs
- **AI/references/** - External documentation and resources
- **AI/releases/** - Version release notes
- **AI/agents/** - Claude sub-agent definitions
- **AI/skills/** - Claude skill definitions
- **AI/prompts/** - Reusable prompts
- **AI/workflows/** - Development workflows
- **AI/commands/** - Custom commands

See `AI/README.md` for detailed information about the documentation structure.

## Additional Resources

- **Parent Project**: See `../CLAUDE.md` for general build commands, testing procedures, and overall EmbedThis architecture
- **Sub-Projects**: See `src/*/CLAUDE.md` for specific instructions related to sub-projects
- **API Documentation**: `doc/index.html` (generated via `make doc`)
- **Project Documentation**: See `AI/` directory for designs, plans, procedures, and context

## Important Notes

