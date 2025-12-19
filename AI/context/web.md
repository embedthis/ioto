# Web Server Module (Web)

The **Web Server Module** — a fast, secure, tiny web server for embedded applications with HTTP, HTTPS, WebSocket support, and comprehensive security features.

## Web Server Features

- HTTP/1.0 and HTTP/1.1 support
- TLS/SSL with OpenSSL or MbedTLS
- WebSocket support
- SSE support
- File upload/download
- Session management with XSRF protection
- Input validation and sanitization
- Configurable request/response limits
- Flexible routing system
- Invocation of C functions bound to URL routes
- API signatures
- JSON configuration file

## Web Module Testing

### Test Ports and Configuration
- HTTP test server: port 4100
- HTTPS test server: port 4443
- Test configuration: `./test/web.json5`
- Test certificates: `../certs/` directory
- When making changes to tests, you can verify they compile by running `tm -s NAME`.

The unit tests use the `url` HTTP client library to exercise the web server. The `testme` tool launches the web server automatically during test runs.

### Benchmark Testing
- Benchmark suite: `test/bench/`
- Run benchmarks: `cd test/bench && tm bench`
- Run with duration: `tm --duration 30 bench`

### Benchmark Results Location
Benchmark results are saved to platform-specific directories:
- `doc/benchmarks/macosx/` - macOS results
- `doc/benchmarks/linux/` - Linux results
- `doc/benchmarks/windows/` - Windows results

Within each platform directory:
- `latest.json5` / `latest.md` - Most recent benchmark run
- `vX.Y.Z.json5` / `vX.Y.Z.md` - Version-specific baselines

To save a release baseline:
```bash
cp doc/benchmarks/macosx/latest.json5 doc/benchmarks/macosx/v1.2.0.json5
cp doc/benchmarks/macosx/latest.md doc/benchmarks/macosx/v1.2.0.md
```

## Web Server Configuration

### Configuration File Format
- Configuration uses JSON5 format with comments
- Test config: `./test/web.json5`
- Key sections: `log`, `tls`, `web`, routing, sessions, uploads, websock

### TLS Certificates
- Test certificates: `../certs/` directory
- CA: `../certs/ca.crt`
- Certificate: `../certs/test.crt`
- Private key: `../certs/test.key`

## Platform-Specific Buffer Sizing

The web module uses adaptive buffer sizes based on platform memory constraints:

### Buffer Boost Macros
- `WEB_BUF_BOOST_2X` — 2x ME_BUFSIZE on desktop, ME_BUFSIZE on constrained platforms
- `WEB_BUF_BOOST_4X` — 4x ME_BUFSIZE on desktop, ME_BUFSIZE on constrained platforms
- `WEB_BUF_BOOST_16X` — 16x ME_BUFSIZE on desktop, ME_BUFSIZE on constrained platforms

### Platform Detection
- **Constrained platforms** (ESP32, FreeRTOS, VxWorks): Use base `ME_BUFSIZE` for all buffer operations
- **Desktop/server platforms**: Use boosted sizes (2x, 4x, 16x) for performance

This ensures efficient memory usage on memory-constrained embedded devices while maintaining high performance on desktop/server platforms.

## Web Module Architecture

### Core Implementation Files
- `src/web.h` - Public API definitions
- `src/http.c` - HTTP protocol implementation
- `src/io.c` - I/O and connection handling
- `src/host.c` - Virtual host management
- `src/session.c` - Session management
- `src/upload.c` - File upload handling
- `src/validate.c` - Input validation and security
- `src/auth.c` - Authentication and authorization

### Web Module Dependencies
- `r/` - Safe runtime and memory management
- `uctx/` - User context and fiber coroutines
- `json/` - JSON5 configuration parsing
- `crypt/` - Cryptographic functions
- `url/` - URL handling utilities
- `websock/` - WebSocket protocol support

## Web Module Development

### Web-Specific Security Notes
- All user input processed through validation pipeline in `src/validate.c`
- XSRF protection enabled by default in tests
- Content security headers configured in host configuration
- Input sanitization for file uploads and form data

### Web Server Debugging
- Enable detailed logging via `web.json5` log configuration
- Log levels: error, info, trace
- Request/response tracing available
- Valgrind suppression: `.valgrind.supp`

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

- **Project Documentation**: See `AI/` directory for designs, plans, procedures, and context

- **Parent Project**: See `../CLAUDE.md` for general build commands, testing procedures, and overall EmbedThis architecture
- **Sub-Projects**: See `src/*/CLAUDE.md` for specific instructions related to sub-projects
- **API Documentation**: Generated via `make doc` → `doc/index.html`

## Notes
- Use `tm --iterations` rather than `TESTME_ITERATIONS=number tm` environment variable
- Always use /* */ for multiline comments
- Always use the best TestMe comparison function instead of relying on the generic `ttrue` or `tassert`. More specific TestMe comparison functions can report expected vs actual.
- Use tm -s --duration 0 to do compile tests. A duration of zero will not run any tests.