# Ioto Agent Changelog

All notable changes to the Ioto Agent project are documented here.

## [3.0.0] - 2025-12-10

### Added
- **Growable Fiber Stacks** - Auto-growing stacks via guard pages (`ME_FIBER_GROWABLE_STACK`)
  - `rAllocPages()`, `rFreePages()`, `rProtectPages()`, `rGetPageSize()` APIs
  - `rSetFiberStackLimits()`, `rGetFiberStackLimits()` for runtime configuration
  - Configuration: `limits.fiberStack`, `limits.fiberStackMax`, `limits.fiberStackGrow`, `limits.fiberStackReset`
- **Fiber Exception Blocks** - Web handler crash recovery (`ME_WEB_FIBER_BLOCKS`)
  - `WEB_HOOK_EXCEPTION` hook event for exception notification
  - `rStartFiberBlock()`, `rEndFiberBlock()` APIs
  - Configuration: `web.fiberBlocks`
- **HTTP Authentication** - Basic and Digest authentication for web server
  - `urlSetAuth()` for URL client authentication
  - Password hash generation command
- **Zero-Copy I/O** - `webReadDirect()` for zero-copy body reading
- **Sendfile Support** - `ME_HTTP_SENDFILE` for zero-copy file transfers
- **Event-Driven Non-Blocking I/O** - 10x connection scalability
- **Simplified SSE/WebSocket APIs** - `urlSseRun()` and `webSocketRun()`
- **Buffer Management** - `rGrowBufSize()` for growing buffers to specific size
- **Socket Limits** - `rGetSocketLimit()`, `rSetSocketLimit()` APIs
- **Wait Handler Flags** - `R_WAIT_MAIN_FIBER` for main fiber execution
- **Time APIs** - `rGetHttpDate()`, `rMakeTime()`, `rParseHttpDate()`, `rGetElapsedTime()`
- **String Functions** - `sncat()` for safe string concatenation

### Changed
- `rSetWaitHandler()` signature: added `flags` parameter
- `rSetFiberLimits()` signature: added `poolMin`, `poolMax` parameters
- `webSendFile()` signature: added `offset`, `len` parameters
- `rParseIsoDate()` now returns -1 on error (was 0)
- URL command `--count` renamed to `--iterations`
- `limits.stack` deprecated, use `limits.fiberStack` instead

### Removed
- `urlSseAsync()`, `urlSseWait()`, `urlWait()` - replaced by `urlSseRun()`
- `webSocketAsync()`, `webSocketWait()` - replaced by `webSocketRun()`
- `urlWebSocketAsync()` - use `webSocketRun()` directly

### Fixed
- Windows pollFds clearing in `rFreeWait()`
- Dynamic poll table growth for Windows/WSAPoll
- Upload forms multipart handling
- Keep-alive timeout behavior
- `webSendFile` hang with large files
- PUT file size limit checking during upload
- IPv4/IPv6 dual-stack networking
- macOS dual-stack localhost issues
- Socket connection verification with `getpeername()`
- JSON parser boundary conditions (fuzzing fixes)

### Security
- HMAC-SHA256 nonce generation for Digest authentication
- Server-wide replay protection with nonce count validation
- Constant-time comparison for timing-attack resistance
- Quote escaping defense against header injection
- Parameter length limits (8KB max) to prevent memory exhaustion

## [2.9.0] - 2025-11-05

### Added
- Socket linger control APIs
- Fiber block management API

### Fixed
- Critical security fixes from fuzzing (JSON parser, HTTP method parsing)
- Resource leaks (file descriptors, memory)
- `webSendFile` hang with large files

---

For detailed release notes, see [doc/releases/](../../doc/releases/).
