# WebSock (WEBSOCKETS)

RFC 6455 compliant WebSocket implementation for embedded IoT applications. Provides both client and server WebSocket functionality with TLS support.

## Module-Specific Architecture

### Core Files

- `src/websock.h` - Public API with WebSocket structures, events, and function declarations
- `src/websockLib.c` - Complete WebSocket protocol implementation

### WebSocket Event System

Event-driven callback model with these events:
- `WS_EVENT_OPEN` - Connection established
- `WS_EVENT_MESSAGE` - Complete message received
- `WS_EVENT_PARTIAL_MESSAGE` - Partial message fragment received
- `WS_EVENT_ERROR` - Error condition detected
- `WS_EVENT_CLOSE` - Connection closed

### Key Structures

- `WebSocket` - Main WebSocket connection object
- `WebSocketEvent` - Event data passed to callbacks
- `WebSocketCallback` - Function pointer type for event handlers

### Dependencies (pak.json)
- `r` - Safe runtime foundation
- `json` - JSON5 parsing and manipulation
- `crypt` - Cryptographic functions and TLS
- `osdep` - Cross-platform OS abstraction
- `certs` - Certificate management

## WebSocket-Specific Usage

### Client Connections

```c
WebSocket *ws = wsOpen(url, protocols, origin, callback, userdata);
```

### Server Integration

WebSockets can be integrated with the `web` and `url` modules for server-side and client-side handling.

### Message Handling

- Text and binary message support
- Automatic frame assembly for fragmented messages
- Configurable message size limits

## Testing Configuration

Unit testing here is minimal. The major unit testing is done in the `url` and `web` modules.

The unit testing here is basic API invocation and not operational testing.

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

## Important Notes
