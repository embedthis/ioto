# CLAUDE.md - URL Module

HTTP client library for embedded IoT applications with WebSocket and SSE support.

## Module Overview

**Purpose**: Lightweight, secure HTTP client with fiber-based concurrency for constrained devices.

**Key Differentiator**: Pragmatic HTTP client - not fully HTTP/1.1 compliant but optimized for embedded use with essential features like keep-alive and transfer-chunking.

**Critical Dependencies**: ALL code MUST use R runtime functions (`rAlloc`, `scopy`, `slen`) instead of standard C equivalents.

See the [DESIGN.md](../doc/DESIGN.md) file for more details.

## Module-Specific Build Commands

```bash
make run                # Run test executable
```

## Test Infrastructure

**Test Server**: Unit tests run an embedded web server (`web`) that acts as the HTTP client target:
- Built from `src/web` directory (separate embedded web server module)
- Serves content from `test/site` directory
- Configured via `test/web.json5`
- Supports HTTP/HTTPS, WebSockets, and SSE for comprehensive testing
- **Important**: The web server code is NOT part of the URL module - it's test infrastructure only

**Test Files**:
- `parse.c.tst` - URL parsing and validation
- `fetch.c.tst` - Basic HTTP request/response
- `post.c.tst` - POST requests with data
- `websockets.c.tst` - WebSocket client functionality
- `sse.c.tst` - Server-Sent Events support

## Core Files

- **src/url.h** — Public API definitions and `Url` structure
- **src/urlLib.c** — HTTP client implementation with fiber-aware I/O
- **src/mains/url.c** — Command-line utility for testing

## Key API Functions

```c
// Lifecycle
Url *urlAlloc(int flags);              // Create client with flags
void urlFree(Url *up);                 // Free resources
int urlClose(Url *up);                 // Close connection

// HTTP requests
int urlFetch(Url *up, cchar *method, cchar *url, cvoid *data, ssize size, cchar *headers, ...);
char *urlGet(cchar *url, cchar *headers, ...);        // Simple GET
char *urlPost(cchar *url, cvoid *data, ssize size, cchar *headers, ...);  // Simple POST

// JSON integration (requires json module)
Json *urlGetJson(cchar *url, cchar *headers, ...);
Json *urlPostJson(cchar *url, cvoid *data, ssize len, cchar *headers, ...);

// Response handling
cchar *urlGetResponse(Url *up);        // Get response body
int urlGetStatus(Url *up);             // Get HTTP status code
cchar *urlGetHeader(Url *up, cchar *header);  // Get response header

// Streaming
ssize urlRead(Url *up, char *buf, ssize bufsize);  // Read response data
int urlWrite(Url *up, cvoid *buf, ssize size);     // Write request data
```

## Module Features

**Protocols:**
- HTTP/HTTPS with keep-alive and chunked encoding
- WebSocket client (requires websockets module)
- Server-Sent Events (SSE) with callbacks

**Authentication:**
- Basic, Digest, OAuth support
- Cookie management and sessions

**Configuration:**
- Default timeout: `ME_URL_TIMEOUT` (0 = no timeout)
- Max response: `URL_MAX_RESPONSE` (1MB default)
- Buffer size: `URL_BUFSIZE` (4KB)
- TLS certificates: `../certs/` directory

## Usage Examples

```c
// Simple GET
char *response = urlGet("https://api.example.com/data", "Authorization: Bearer %s", token);

// JSON API
Json *json = urlGetJson("https://api.example.com/status", NULL);
char *jsonData = jsonToString(data);
Json *result = urlPostJson("https://api.example.com/update", jsonData, slen(jsonData),
                          "Content-Type: application/json");

// Streaming
Url *up = urlAlloc(0);
urlFetch(up, "GET", "https://example.com/file", NULL, 0, NULL);
while ((len = urlRead(up, buffer, sizeof(buffer))) > 0) {
    processData(buffer, len);
}
urlFree(up);
```

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
- **API Documentation**: `doc/index.html` (generated via `make doc`)

## Thread Safety

**IMPORTANT**: This module is **NOT thread-safe**. Following the parent architecture principles:
- The URL module uses single-threaded execution with fiber coroutines for concurrency
- `Url` objects must NOT be accessed from multiple OS threads simultaneously
- All URL operations should be performed within the same fiber context
- The deferred free mechanism (`up->inCallback`, `up->needFree`) is designed for single-threaded fiber cooperation, not multi-threaded synchronization

If multi-threaded access is required, the embedding application must provide external synchronization (mutexes, locks, etc.).

## Important Notes
