# Ioto Agent Changelog

This changelog tracks significant changes to the Ioto Agent project. For detailed release notes, see [doc/releases/](../../doc/releases/).

## [3.0.0] - 2025-12-18

### v3.0.0 Commit Series

Created 15 discrete commits for the v3.0.0 release:

1. **DEV: Add OS type constants for compile-time detection** (`f63521a`)
   - Added ME_OS_* constants to osdep.h for compile-time OS detection

2. **DEV: Improve FreeRTOS fiber implementation with semaphore synchronization** (`29939bf`)
   - Rewrote FreeRTOS context switching with mutex + condition semaphore pairs
   - Added predicate-based wait functions with spurious wakeup protection

3. **DEV: Add fiber stack limit configuration and runtime improvements** (`fe5f419`)
   - Added fiber stack limit configuration (fiberStack, fiberStackMax, etc.)
   - ESP32 CONFIG_ESP_MAIN_TASK_STACK_SIZE integration

4. **DEV: Enhance web server with authentication and performance features** (`99383b7`)
   - HTTP Basic and Digest authentication
   - Client-side cache control headers
   - Event-driven non-blocking I/O for 10x scalability
   - Pre-compressed content serving (.gz, .br)

5. **DEV: Add URL client authentication and security hardening** (`8b31c14`)
   - urlSetAuth() for HTTP authentication
   - Quote escaping defense against header injection

6. **FIX: Security fixes in JSON and crypto libraries** (`d73f1fd`)
   - Fixed invalid reference in JSON parser (fuzzing)
   - Added HMAC support for Digest authentication

7. **DEV: Update MQTT library for v3.0.0 compatibility** (`9492593`)

8. **DEV: Add FreeRTOS demo and update ESP32 apps** (`908114c`)
   - New apps/demo/freertos/ example
   - Added apps/blink/src/blink.c

9. **DEV: Update app configurations for v3.0.0** (`20625b0`)

10. **DEV: Update agent core source files for v3.0.0** (`35d81d9`)
    - SERVICES_REGISTER support
    - Renamed url --count to --iterations

11. **DEV: Update samples for v3.0.0 API** (`3519108`)
    - Updated rInit() calls to use 2 parameters
    - Simplified build paths

12. **CHORE: Update build system and project files** (`0fa9acc`)
    - Regenerated project files for all platforms
    - Updated ESP32 CMake configuration

13. **TEST: Update tests and configs for v3.0.0 features** (`2c01412`)

14. **DOC: Update documentation for v3.0.0 release** (`7409571`)
    - Updated READMEs with simplified build workflows
    - Added v3.0.0 release notes

15. **DOC: Update AI context and remove obsolete CHANGELOG** (`81eaa0d`)

### Major Features

- **Web Server Authentication**: HTTP Basic and Digest authentication with HMAC-SHA256 nonce generation
- **Client-Side Cache Control**: Route-based Cache-Control, Expires, and Pragma headers
- **Event-Driven I/O**: 10x connection scalability by freeing fibers during keep-alive idle periods
- **Pre-Compressed Content**: Automatic .gz and .br file serving with content negotiation
- **Fiber Exception Blocks**: Optional crash recovery for web request handlers
- **Growable Fiber Stacks**: Guard page auto-growing stacks via ME_FIBER_GROWABLE_STACK
- **FreeRTOS Support**: Semaphore-based fiber synchronization
- **ESP32-C6 Support**: RISC-V based ESP32 device support

### New APIs

- `rSetSocketLinger()`, `rGetSocketLimit()`, `rSetSocketLimit()`
- `rGetHttpDate()`, `rMakeTime()`, `rParseHttpDate()`, `rGetElapsedTime()`
- `sncat()`, `rRun()`, `rMakeArgs()`
- `urlSetAuth()`, `urlSetLinger()`
- `webReadDirect()`, `webWriteResponseString()`, `webSetCacheControlHeaders()`

### Breaking Changes

- `rParseIsoDate()` now returns -1 on error (was 0)
- URL command `--count` renamed to `--iterations`

---

## Previous Releases

See [doc/releases/](../../doc/releases/) for historical release notes.
