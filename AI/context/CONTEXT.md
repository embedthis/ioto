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

### Version 3.0.0 Release (December 10, 2025)

Major release with performance optimizations, growable fiber stacks, and API enhancements.

**Major Features**:
- **Growable Fiber Stacks** - Auto-growing stacks via guard pages (`ME_FIBER_GROWABLE_STACK`)
  - Reserves large virtual address space, commits memory on demand
  - Configurable via `limits.fiberStack`, `limits.fiberStackMax`, `limits.fiberStackGrow`
  - New APIs: `rAllocPages()`, `rFreePages()`, `rProtectPages()`, `rGetPageSize()`
  - New APIs: `rSetFiberStackLimits()`, `rGetFiberStackLimits()`
- **Fiber Exception Blocks** - Crash recovery for web handlers (`ME_WEB_FIBER_BLOCKS`)
  - Enable via `web.fiberBlocks` configuration
  - New `WEB_HOOK_EXCEPTION` hook for exception notification
- **Zero-Copy I/O** - `webReadDirect()` and sendfile support
- **HTTP Authentication** - Basic and Digest authentication for web server
- **Event-Driven I/O** - 10x connection scalability via non-blocking keep-alive
- **Simplified SSE/WebSocket APIs** - `urlSseRun()` and `webSocketRun()` replace async/wait pairs

**API Changes**:
- `rSetWaitHandler()` signature changed: added `flags` parameter
- `rSetFiberLimits()` signature changed: added `poolMin`, `poolMax` parameters
- `webSendFile()` signature changed: added `offset`, `len` parameters
- Removed `urlSseAsync()`, `urlSseWait()`, `urlWait()` - use `urlSseRun()` instead
- Removed `webSocketAsync()`, `webSocketWait()` - use `webSocketRun()` instead

**Documentation Updated**:
- [../logs/CHANGELOG.md](../logs/CHANGELOG.md) - Changelog entry for v3.0.0
- [../../doc/releases/3.0.0.md](../../doc/releases/3.0.0.md) - Full release notes

**Status**: Version 3.0.0 committed and ready for release.

---

**Last Updated:** 2025-12-10
