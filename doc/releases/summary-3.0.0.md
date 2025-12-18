# Ioto Device Agent v3.0.0 Release Notes

Release Date: December 10, 2025

## Recommended Action

- [ ] Optional Upgrade -- Upgrade only if convenient
- [x] **Recommended Upgrade** -- Upgrade recommended but not essential
- [ ] Essential Upgrade -- All users strongly advised to upgrade

## Overview

Version 3.0.0 is a major feature and security release adding HTTP Basic and Digest authentication, client-side cache control, event-driven non-blocking I/O for 10x connection scalability, and pre-compressed content serving. The release includes significant performance optimizations (socket accept, zero-copy body reading, dynamic buffers), security hardening from fuzzing campaigns, and new runtime APIs for fiber and socket control.

## Major Features

### Web Server Authentication
- **HTTP Basic Authentication** - Username/password with configurable TLS enforcement (defaults to required) and SHA-256 password hashing
- **HTTP Digest Authentication** - Challenge-response authentication with MD5/SHA-256 algorithms and replay protection
- **Password Tool** - New `password` command for generating hashed passwords
- **Flexible User Management** - `webAddUser()` allows null password, `webLogin()` auto-creates users for custom auth schemes

### Client-Side Cache Control
- Route-based `Cache-Control`, `Expires`, and `Pragma` headers
- Extension-based filtering and natural time string parsing (e.g., `1week`, `5mins`)

### Event-Driven Non-Blocking I/O
- **10x Connection Scalability** - Frees fibers during keep-alive idle periods
- Saves 64-256KB per idle connection with zero performance impact on active requests

### Pre-Compressed Content Serving
- Automatic `.gz` and `.br` file serving based on `Accept-Encoding`
- Content negotiation with Brotli priority over gzip

### Fibers
- **Fiber Pool** - Pool of pre-allocated fibers for reuse
- **Guard Page Auto-Growing Stacks** - Uses virtual memory guard pages for automatic stack growth
- Configurable initial, maximum, and growth sizes via `limits.fiberStack*` properties
- Async-signal-safe stack growth handler

### Builder Configuration
- **Flexible Builder Endpoint** - `cmdBuilder` available with `SERVICES_REGISTER` (not just `SERVICES_CLOUD`)

### Standalone Web Server
- **Web Server Only Mode** - Run `web` program standalone without full Ioto agent for local-only management

### Web Server Exception Handling
- **Fiber Exception Blocks** - Optional crash recovery for web request handlers
- Enable via `web.fiberBlocks` configuration; catches SIGSEGV, SIGFPE, SIGBUS, SIGILL

### Performance Optimizations
- Optimized socket accept path with `R_WAIT_MAIN_FIBER` flag
- Zero-copy body reading via `webReadDirect()`
- Dynamic buffer growth with `rGrowBufSize()`
- Dynamic poll table growth for Windows/WSAPoll

### New Runtime APIs
- **Socket/Fiber**: `rSetSocketLinger()`, `rStartFiberBlock()`, `rGetSocketLimit()`, `rSetSocketLimit()`
- **Time**: `rGetHttpDate()`, `rMakeTime()`, `rParseHttpDate()`, `rGetElapsedTime()`
- **String/Command**: `sncat()`, `rRun()`, `rMakeArgs()`
- **URL Client**: `urlSetAuth()`, `urlSetLinger()`
- **Web Server**: `webReadDirect()`, `webWriteResponseString()`, `webSetCacheControlHeaders()`

### Comprehensive Test Suites
- **Unit Tests** - Extensive test suite covering all modules
- **Leak Tests** - Memory and resource leak detection suite
- **Benchmark Tests** - Performance benchmarking suite
- **Fuzz Tests** - Security fuzzing test suite

## Bug Fixes

### Critical Fixes
- **Fixed hang in `webSendFile`** - Resolved deadlock when sending large files
- **Fixed upload forms** - Resolved multipart form-data file upload handling
- **Fixed keep-alive timeout** - Corrected HTTP keep-alive connection timeout behavior
- **Fixed file descriptor leak** - Prevents descriptor exhaustion in long-running applications
- **Fixed fiber exhaustion handling** - Improved resource management when fiber pool is depleted
- **Fixed IPv4/IPv6 dual-stack** - Properly supports both protocols simultaneously

### Security Fixes
- Fixed invalid reference in JSON parser (discovered via fuzzing)
- Fixed null dereference in HTTP method parsing (discovered via fuzzing)
- HMAC-SHA256 nonce generation for Digest authentication
- URL client header injection defense

### Platform Fixes
- Fixed macOS socket connection and dual-stack localhost issues
- Fixed Windows pollFds clearing in `rFreeWait`
- Fixed socket connection verification with `getpeername()`

### Samples Fixes
- Fixed sample build configuration to use simplified relative paths
- Updated samples to use current API signatures (`rInit` with 2 parameters)
- Added explicit callback type casts for stricter compilation

## Breaking Changes

- **`rParseIsoDate()` now returns -1 on error** (previously returned 0)
- **URL command `--count` renamed to `--iterations`**

## FreeRTOS and ESP32 Improvements

- **FreeRTOS Fiber Implementation** - Semaphore-based synchronization with mutex + condition semaphore pairs
- **ESP32-C6 Support** - Added support for ESP32-C6 (RISC-V based) devices
- **OS Type Constants** - Added `ME_OS_*` constants for compile-time OS detection
- **FreeRTOS Demo App** - New `apps/demo/freertos/` with complete integration example
- **Improved Build Documentation** - Simplified README-FREERTOS.md and README-ESP32.md

## Platform Support

- **Tier 1**: Linux (x86, x64, ARM, ARM64, RISC-V), macOS (Intel, Apple Silicon), Windows 11 (VS2022, WSL)
- **Tier 2**: FreeRTOS, BSD, ESP32 (ESP-IDF) including ESP32-S3 and ESP32-C6, VxWorks

## Security Considerations

- **Basic Authentication**: Use only with TLS/HTTPS in production
- **Digest Authentication**: More secure challenge-response mechanism suitable for HTTP
- **Fuzzing Fixes**: JSON parser boundary conditions, HTTP method parsing, resource leak prevention
- **Recommendation**: Users with web services exposed to untrusted input should upgrade promptly

## Package Updates

- **r/ v1.2.0** - Growable fiber stacks, socket/fiber control APIs, time APIs, connection fixes
- **json/** - Fuzzing-discovered security fixes
- **crypt/** - HMAC support for Digest authentication
- **url/ v1.3.0** - HTTP Basic/Digest authentication, security hardening
- **web/ v1.3.0** - Event-driven I/O, exception handling, authentication, cache control, pre-compressed content

## Upgrade Instructions

1. **Backup** current configuration and state directory
2. **Review** authentication configuration if adding auth
3. **Update** to v3.0.0 source code
4. **Rebuild** with your application configuration
5. **Note Breaking Changes**: `rParseIsoDate()` return value, URL command `--count` renamed
6. **Test** authentication flows, dual-stack networking, file uploads, long-running applications

## Known Issues

None identified in this release.

## Download

Download from the [Builder Site](https://admin.embedthis.com/product)

## Documentation

Full documentation available at: https://www.embedthis.com/doc/
Full release notes at: github.com/embedthis/ioto/releases/tag/v3.0.0

## Support

- Documentation: https://www.embedthis.com/doc/
- Issues: Contact EmbedThis support

---

**Note**: Major release adding HTTP authentication, cache control, 10x connection scalability, growable fiber stacks, handler crash recovery, and pre-compressed content serving. Includes security fixes from fuzzing and Windows stability improvements. Upgrade recommended.
