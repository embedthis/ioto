# MQTT Module (MQTT)

MQTT client library implementation for embedded IoT applications.

## Module-Specific Components

- `src/mqtt.h` + `src/mqttLib.c` — Core MQTT 3.1.1 protocol implementation
- `src/pubsub.c` — Example publish/subscribe client application
- `test/` — Unit tests for MQTT functionality

## MQTT API Usage Patterns

### Connection Flow

1. Create MQTT instance with `mqttAlloc()`
2. Configure connection parameters (host, port, credentials)
3. Connect with `mqttConnect()` - **REQUIRED before publish/subscribe**
4. Publish messages with `mqttPublish()`
5. Subscribe to topics with `mqttSubscribe()`
6. Process events in main loop
7. Disconnect with `mqttDisconnect()`

### Key API Functions

- `mqttAlloc()` — Create MQTT instance
- `mqttConnect()` — Establish connection to broker
- `mqttPublish()` — Send message to topic
- `mqttSubscribe()` — Subscribe to topic pattern
- `mqttSetCallback()` — Handle incoming messages
- `mqttProcess()` — Process network events (call regularly)

## Module Dependencies

This module depends on these foundation modules (see ../CLAUDE.md):
- `r/` — Safe runtime (memory, strings, collections)
- `osdep/` — OS abstraction layer
- `uctx/` — Fiber coroutines for async I/O
- `crypt/` — TLS/SSL for secure connections

## MQTT-Specific Testing

```bash
cd test
testme connect          # Test MQTT connection
testme publish          # Test message publishing
testme subscribe        # Test topic subscription
testme parse            # Test MQTT packet parsing
```

## Critical Implementation Notes

- **Connection Required**: All publish/subscribe operations require an active connection via `mqttConnect()`
- **Message Limits**: Default max message size is 64KB (configurable via ME_MQTT_MAX_MESSAGE)
- **Topic Validation**: Topics are validated according to MQTT 3.1.1 specification
- **Async Processing**: Use `mqttProcess()` in main event loop for non-blocking operation
- **TLS Security**: Always use TLS in production (`mqttSetTls(mqtt, 1)`)

## Error Handling

MQTT functions return status codes:
- `0` — Success
- `< 0` — Error (check `mqttGetError()` for details)

Common error scenarios:
- Connection timeout/failure
- Invalid topic names
- Message size exceeded
- Network I/O errors

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

