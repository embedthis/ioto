# Ioto Device Agent

Complete IoT solution combining multiple embedded C libraries into a unified agent for local and cloud-based device management.

## Target Audience

Experienced embedded developers who:
- Embed this software in device firmware or other projects
- Are responsible for securing the broader system and validating all inputs
- Use AI/LLM tools to build embedded applications

## Directory Structure

```
src/               # Agent source code
├── agent.c        # Main agent integration
├── config.c       # Configuration management
├── database.c     # Database service
├── webserver.c    # Web server service
└── cmds/          # Command-line utilities

lib/               # Amalgamated module libraries
├── rLib.c         # Safe Runtime
├── jsonLib.c      # JSON parser
├── dbLib.c        # Database
├── webLib.c       # Web server
├── urlLib.c       # HTTP client
├── mqttLib.c      # MQTT client
├── cryptLib.c     # Cryptography
└── ...            # Other modules

include/           # Public API headers
apps/              # Application templates
├── ai/            # AI-enabled application
├── blank/         # Minimal template
├── http/          # Web server app
└── unit/          # Test harness

certs/             # TLS certificates (test only)
samples/           # Working code examples
├── agent/         # C code samples (url-get, web-auth, mqtt, own-main, etc.)
├── api/           # API shell script samples
└── README.md      # Sample index

AI/                # AI/LLM documentation
├── context/       # Per-module API guides (r.md, db.md, web.md, ...)
│   └── api-guide.md  # Cross-cutting quick reference
├── designs/       # Architecture docs (DESIGN.md)
│   └── modules/   # Per-module architecture from upstream
├── references/    # External references (REFERENCES.md)
│   └── modules/   # Per-module references from upstream
└── README.md      # AI directory guide

.claude/
└── skills/        # Claude Code guided workflows

build/             # Build outputs
test/              # Unit tests
```

## Modules

| Module | Header | Description |
|--------|--------|-------------|
| agent | `ioto.h` | Main agent: IoT cloud management, HTTP server, MQTT client, database |
| r | `r.h` | Safe Runtime: memory management, strings, fibers, events (foundation) |
| json | `json.h` | JSON5/JSON6 parsing and manipulation |
| db | `db.h` | Embedded database with cloud sync |
| web | `web.h` | Fast, secure embedded web server |
| url | `url.h` | HTTP client library with SSE and WebSocket |
| mqtt | `mqtt.h` | MQTT 3.1.1 client protocol |
| crypt | `crypt.h` | Cryptographic functions and TLS support |
| websock | `websock.h` | WebSocket protocol support |
| openai | `openai.h` | OpenAI API integration |
| osdep | `osdep.h` | Operating system abstraction layer |
| uctx | `uctx.h` | User context and fiber threading |

## Building

```bash
make                    # Build libioto.a + all apps
make lib                # Build libioto.a only
make http               # Build a single app by name
make run                # Build and run the default app (http)
make clean              # Clean build artifacts
```

Each app builds as a separate binary (e.g. `ioto-http`, `ioto-ai`) linking against `libioto.a`. Run any app via `ioto-NAME`. Apps run from their own directory (`apps/<APP>/`) with local config and state at `apps/<APP>/state/`.

### Build Flow

1. `lib` - Builds `libioto.a` using `projects/gmake2/`
2. `apps` - For each app, runs `bin/prepare` then builds the app binary using `apps/<APP>/projects/gmake2/`

### Build Configuration

- `OPTIMIZE=debug` or `OPTIMIZE=release` - optimization level
- `PROFILE=dev` or `PROFILE=prod` - development vs production
- `SHOW=1` - display build commands
- `ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0 make` - TLS stack selection

### Prerequisites

- GCC or compatible C compiler
- Make (GNU Make)
- OpenSSL or MbedTLS (OpenSSL preferred)
- TestMe, Bun, bash for unit tests

## App Configuration

Config files are at `apps/<APP>/*.json5` (not in a subdirectory).
At build time, `apps/<APP>/bin/prepare` copies and blends them to `apps/<APP>/state/config/`.
The app runs from `apps/<APP>/` and finds its state at `apps/<APP>/state/`.

| File | Purpose |
|------|---------|
| `ioto.json5` | Services, limits, logging, TLS, profiles (dev/prod) |
| `web.json5` | Listen endpoints, routes, timeouts, upload |
| `device.json5` | Product ID, device name, model |
| `schema.json5` | Database schema with model definitions |

### Key Settings

```json5
{
    limits: {
        fiberStack: '64k'        // Fiber stack size
    },
    services: {
        database: true,
        mqtt: true,
        web: true
    }
}
```

## Architecture Principles

- Written in ANSI C for maximum portability
- Single-threaded with fiber coroutines for concurrency
- Modular design with minimal interdependencies
- Cross-platform: Linux, macOS, Windows/WSL, ESP32, FreeRTOS
- Most functions are null-tolerant
- Use `osdep.h` for OS abstraction; prefer `ssize` over standard C types (`ssize` is always 64-bit)

## Code Conventions

- 4-space indentation
- 120-character line limit
- CamelCase for functions and variables
- One line between functions and between code blocks
- Single-line comments use `//`
- Multi-line comments use `/* */` (not `//` on each line, no `*` prefix per line)
- Descriptive symbol names
- Declare variables at top of functions
- Use single quotes in JSON5 files
- Put temporary test files in `.test` directory

## Safe Runtime (R) - MANDATORY

Always use R runtime functions instead of standard C:

| Standard C | Safe Runtime | Notes |
|-----------|-------------|-------|
| `malloc(n)` | `rAlloc(n)` | |
| `free(p)` | `rFree(p)` | Null-tolerant |
| `realloc(p,n)` | `rRealloc(p,n)` | |
| `strlen(s)` | `slen(s)` | Null-tolerant |
| `strcpy(d,s)` | `scopy(d,max,s)` | Buffer-safe |
| `strcmp(a,b)` | `scmp(a,b)` | Null-tolerant |
| `strdup(s)` | `sclone(s)` | Returns "" for NULL |
| `sprintf(...)` | `sfmt(fmt,...)` | Allocates result |

```c
#include "r.h"

char *buf = rAlloc(256);           // vs malloc()
ssize len = slen(str);             // vs strlen()
scopy(buf, 256, src);              // vs strcpy()
rFree(buf);                        // vs free()
```

Memory rules: use `rAlloc`/`rFree` exclusively. Never mix with `malloc`/`free`.

## Fiber Programming

**Execution Model**: Single-threaded with fiber coroutines.

- Use fiber-aware blocking calls for I/O (`rReadSocket`, `rSleep`)
- Configure stack size in `ioto.json5` `limits.fiberStack` (default 64K)
- Avoid large stack allocations
- If an app is running unreliably or crashing, check the stack depth and consider increasing the fiber stack size

## Testing

```bash
make test                           # Complete test suite

cd test && testme mqtt-*            # MQTT tests
cd test && testme db-sync-*         # Database sync tests
```

- Don't use `printf` in C unit tests; use `tinfo` for trace
- Unit tests must have unique names to avoid conflicts

## Security Considerations

- Configure TLS properly before cloud connections
- Never commit real certificates or API keys
- Use test certificates from `certs/` for development only
- Store production certificates in `apps/<APP>/state/` only
- Code does NOT explicitly check memory allocations for failure (uses global handler)
- Inputs to local APIs are deemed validated by developers
- `rDebug` may emit sensitive data in debug builds (intentional)

## Resources

- **API Quick Reference**: [AI/context/api-guide.md](AI/context/api-guide.md)
- **Module API Context**: `AI/context/modules/*.md` — detailed API guides per module
- **Architecture Design**: [AI/designs/DESIGN.md](AI/designs/DESIGN.md)
- **Per-Module Designs**: `AI/designs/modules/` — architecture docs from each module
- **External References**: `AI/references/` — external documentation links
- **Guided Workflows**: `.claude/skills/` — task-oriented Claude Code skills
- **Code Samples**: `samples/agent/` — working C examples, `samples/api/` — API scripts
- **Platform Guides**: `README-FREERTOS.md`, `README-ESP32.md`, `README-CROSS.md`
- **API Documentation**: https://www.embedthis.com/doc/

## Available Skills

| Skill | Purpose |
|-------|---------|
| `getting-started` | App scaffolding, entry points, fiber model, build/run |
| `runtime` | Safe Runtime patterns, fibers, scheduling, memory |
| `rest` | REST endpoints, web action routines, authentication, WebSocket |
| `http` | Outbound HTTP client requests (url module) |
| `database` | Schema design, CRUD operations, expiry, pagination |
| `mqtt` | MQTT publish/subscribe |
| `migrate-appweb` | Migrate Appweb applications to Ioto — API conversion, config migration, verification |
| `migrate-goahead` | Migrate GoAhead applications to Ioto — API conversion, config migration, verification |

Cloud skills (require `pak install cloud`): `cloud-connect`, `cloud-database`, `cloud-provision`, `cloud-ota`, `cloud-ai`.

## Important Notes

- `ssize` is always 64 bits on all systems
- After modifying code, run unit tests
- Always declare variables at top of functions
- In documentation, do not use `@return Void` or `@defgroup`
