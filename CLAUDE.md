# Ioto Device Agent

Complete IoT solution combining multiple embedded C libraries into a unified agent for local and cloud-based device management.

## Directory Structure

```
apps/              # Application templates
├── demo/          # Default IoT demonstration
├── ai/            # AI-enabled application
├── auth/          # Authentication-focused
├── blank/         # Minimal template
└── unit/          # Test harness

src/               # Core modules (see ../CLAUDE.md for descriptions)
├── agent/         # Main integration layer
├── r/             # Safe Runtime foundation
├── json/          # JSON5/JSON6 parser
├── osdep/         # OS abstraction
└── uctx/          # User context/fibers

paks/              # Modular packages
├── crypt/, db/, mqtt/, ssl/
├── web/, url/, websock/
└── ...

state/             # Runtime state (never commit)
build/             # Build outputs
test/              # Agent-specific tests
```

## Agent Commands

### Building Applications
```bash
make APP=demo           # Default IoT demo
make APP=ai             # AI-enabled app
make APP=auth           # Authentication app
make APP=blank          # Minimal template
make APP=unit           # Test harness

make run                # Run agent: build/bin/ioto -v
```

### Agent Testing
```bash
# Complete test suite
make APP=unit && make run

# Specific test patterns
cd test && testme mqtt-*        # MQTT tests
cd test && testme db-sync-*     # Database sync tests
```

## Configuration

### Files
- **App Config**: `apps/<APP>/config/ioto.json5`
- **Runtime State**: `state/` (excluded from git)

### Key Settings
```json5
{
    limits: {
        fiberStack: '64k'        // Fiber stack size
    },
    profiles: {
        dev: { /* development */ },
        prod: { /* production */ }
    }
}
```

## Fiber Programming

**Execution Model**: Single-threaded with fiber coroutines

### Guidelines
- Use fiber-aware blocking calls for I/O
- Configure stack size in `ioto.json5`
- Avoid large stack allocations
- Prefer non-blocking I/O patterns

### Safe Runtime (R) - **MANDATORY**
Always use R runtime functions (see `../CLAUDE.md` for complete list):

```c
#include "r.h"

// Memory and strings
char *buf = rAlloc(256);           // vs malloc()
ssize len = slen(str);             // vs strlen()
scopy(buf, 256, src);              // vs strcpy()

// Fiber-aware I/O
RSocket *sock = rAllocSocket();
rConnect(sock, host, port);
```

### JSON5/JSON6 Features
```c
#include "json.h"

Json *config = jsonParse(text, JSON_STRICT);
cchar *host = jsonGet(config, "database.host");
jsonSet(config, "runtime.started", jsonDate(rGetTime()));
```

## Agent-Specific Security

### Production Builds
```bash
DEBUG=release ME_R_LOGGING=0 make           # Production build
ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0 make      # Embedded TLS
```

### Certificate Management
- Development: Use `../certs/` test certificates
- Production: Store real certificates in `state/` only
- Never commit real certificates or API keys

## Resources

- **Module Docs**: `src/*/CLAUDE.md`
- **Platform Guides**: `README-{FREERTOS,ESP32,CROSS}.md`
- **API Reference**: https://www.embedthis.com/doc/
