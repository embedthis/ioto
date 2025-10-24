# OpenAI API Support (OPENAI)

This is the OpenAI library module from the EmbedThis Ioto suite - a C library for integrating with OpenAI's APIs. It provides support for:

- OpenAI Chat Completion API (`gpt-4o-mini` default model)
- OpenAI Responses API with agent callbacks
- OpenAI Real-Time API with WebSocket connections
- Streaming responses with Server-Sent Events (SSE)
- Model listing and management

The library is built on the EmbedThis Safe Runtime (R) foundation and integrates with the modular Ioto ecosystem including JSON parsing, URL handling, cryptographic functions, and WebSocket support.

## Architecture

### Core Components

- **src/openai.h** — Main header file with API definitions and types
- **src/openaiLib.c** — Core implementation of OpenAI API client
- **src/*** — Embedded dependencies (r, json, url, crypt, uctx, websockets, ssl, osdep)

### Dependencies

The project uses a modular package system via `paks/` directory:
- **r** — Safe Runtime foundation (memory management, strings, fibers)
- **json** — JSON5/JSON6 parsing and manipulation
- **url** — HTTP client library for API requests
- **crypt** — Cryptographic functions and TLS support
- **websockets** — WebSocket protocol for Real-Time API
- **uctx** — User context switching for fiber coroutines
- **ssl** — SSL/TLS certificate management
- **osdep** — Operating system abstraction layer

When code is duplicated between `paks/` and `src/`, the `src/` version takes precedence.

## Common Build Commands

### Building
```bash
make                    # Build with default configuration
make clean              # Clean build artifacts
make help               # Show configuration options
make install            # Install to system directories
make run                # Run example/test program
make package            # Create distribution packages
```

### Build Configuration

**TLS Stack Selection**:
```bash
# Use OpenSSL (default for OpenAI library)
ME_COM_OPENSSL=1 ME_COM_MBEDTLS=0 make

# Use MbedTLS (embedded/constrained environments)
ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0 make
```

**Build Profiles**:
```bash
# Debug build (symbols, logging enabled)
DEBUG=debug make

# Release build (optimized, minimal logging)
DEBUG=release make

# Show build commands
SHOW=1 make
```

**Feature Control**:
```bash
# Enable/disable runtime logging
ME_R_LOGGING=1 make     # Enable logging
ME_R_LOGGING=0 make     # Disable logging

# Enable debug tracing
ME_R_TRACING=1 make
```

### Code Formatting
```bash
make format             # Format all source code using uncrustify
```

## Testing

Tests are located in the `test/` directory:
- Test files use `.c.tst` extension
- Run all tests: `make test` or `cd test && testme`
- Run specific test: `cd test && testme TEST_NAME`

The `testme.h` is defined in the parent/include directory.

## API Usage

### Key Data Types
```c
typedef struct OpenAI {
    char *endpoint;                     // OpenAI endpoint URL
    char *realTimeEndpoint;             // Real-time WebSocket endpoint
    char *headers;                      // Authorization headers
    int flags;                          // Configuration flags
} OpenAI;

// Agent callback for processing responses
typedef char*(*OpenAIAgent)(cchar *name, Json *request, Json *response, void *arg);
```

### Core API Functions
```c
// Initialize OpenAI client
int openaiInit(cchar *endpoint, cchar *key, Json *config, int flags);

// Chat completion (synchronous)
Json *openaiChatCompletion(Json *props);

// Responses API with agent callbacks
Json *openaiResponses(Json *props, OpenAIAgent agent, void *arg);

// Streaming responses
Url *openaiStream(Json *props, UrlSseProc callback, void *arg);

// Real-time WebSocket connection
Url *openaiRealTimeConnect(Json *props);

// List available models
Json *openaiListModels(void);

// Cleanup
void openaiTerm(void);
```

### Usage Pattern
```c
// Initialize with API key
openaiInit("https://api.openai.com/v1", apiKey, NULL, 0);

// Create request
Json *request = jsonAlloc(0);
jsonSetString(request, "model", "gpt-4o-mini");
jsonSetString(request, "prompt", "Hello, world!");

// Make request
Json *response = openaiChatCompletion(request);

// Process response
char *content = jsonGetString(response, "choices.0.message.content");

// Cleanup
jsonFree(request);
jsonFree(response);
openaiTerm();
```

## Code Style

- **Language**: ANSI C (embeddable in C/C++ applications)
- **Indentation**: 4 spaces, max line length 120 characters
- **Naming**: camelCase for functions and variables
- **Memory Management**: Use R runtime functions (`rAlloc`, `rFree`, `sclone`, etc.)
- **String Operations**: Use safe R runtime functions (`slen`, `scopy`, `scmp`)
- **Error Handling**: Functions return `NULL` or negative values on error

## Security Considerations

- Always validate API keys before storage
- Use TLS for all API communications (enforced by default)
- Never log sensitive data in production builds
- Use `DEBUG=release` and `ME_R_LOGGING=0` for production
- API responses may contain sensitive information - handle appropriately

## Development Notes

### Build System
- Uses **MakeMe** build tool for advanced configuration
- Platform-specific makefiles generated in `projects/` directory
- Supports cross-compilation for embedded targets
- Default TLS stack is OpenSSL (required for OpenAI API compatibility)

### Platform Support
- Linux, macOS, Windows (via WSL)
- Embedded platforms with sufficient resources
- Single-threaded with fiber coroutines for concurrency
- Cross-platform via OS abstraction layer

### Advanced Configuration
Configure via environment variables or `main.me`:
- `ME_COM_OPENAI=1` — Enable OpenAI module
- `ME_COM_WEBSOCK=1` — Enable WebSocket support
- `ME_COM_URL=1` — Enable URL client
- Static linking enabled by default for distribution

## Project Documentation

The OpenAI project maintains structured documentation in the `AI` directory to assist with development and AI-assisted coding:

- **[AI/designs/DESIGN.md](AI/designs/DESIGN.md)** — Detailed architectural design and implementation
- **[AI/plans/PLAN.md](AI/plans/PLAN.md)** — Project roadmap, goals, and current status
- **[AI/procedures/PROCEDURE.md](AI/procedures/PROCEDURE.md)** — Development procedures and workflows
- **[AI/logs/CHANGELOG.md](AI/logs/CHANGELOG.md)** — Reverse chronological change log
- **[AI/references/REFERENCES.md](AI/references/REFERENCES.md)** — External documentation and resources
- **[AI/context/](AI/context/)** — Saved context for in-progress tasks

These documents provide comprehensive guidance for understanding the codebase, development practices, and project evolution.