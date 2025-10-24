# Reference Documentation

This document provides links to reference materials, external documentation, and module-specific guides for the Ioto Agent project.

## Internal Documentation

### Module Context Documentation

Detailed documentation for each module is available in [../context/](../context/):

#### Foundation Modules
- **[r.md](../context/r.md)** - Safe Runtime foundation
- **[json.md](../context/json.md)** - JSON5/JSON6 parser
- **[osdep.md](../context/osdep.md)** - OS abstraction layer
- **[uctx.md](../context/uctx.md)** - User context and fibers

#### Security
- **[crypt.md](../context/crypt.md)** - Cryptographic functions and TLS

#### Data Management
- **[db.md](../context/db.md)** - Embedded database

#### Communication
- **[mqtt.md](../context/mqtt.md)** - MQTT client
- **[web.md](../context/web.md)** - Embedded web server
- **[url.md](../context/url.md)** - HTTP client
- **[websock.md](../context/websock.md)** - WebSocket support

#### AI Integration
- **[openai.md](../context/openai.md)** - OpenAI API support

### Design Documentation
- **[../designs/DESIGN.md](../designs/DESIGN.md)** - Overall architecture and design
- **[../designs/SECURITY.md](../designs/SECURITY.md)** - Security architecture

### Planning Documentation
- **[../plans/PLAN.md](../plans/PLAN.md)** - Current development plan
- **[../plans/ROADMAP.md](../plans/ROADMAP.md)** - Product roadmap
- **[../plans/BUSINESS-PLAN.md](../plans/BUSINESS-PLAN.md)** - Business planning
- **[../plans/MIGRATION.md](../plans/MIGRATION.md)** - Migration guides

### Procedures
- **[../procedures/PROCEDURE.md](../procedures/PROCEDURE.md)** - Build and development procedures

### Project Documentation
- **[../../CLAUDE.md](../../CLAUDE.md)** - Project-level agent instructions
- **[../../README.md](../../README.md)** - Project overview

## External Documentation

### Official Documentation
- **Ioto Documentation**: https://www.embedthis.com/doc/
- **Ioto API Reference**: https://www.embedthis.com/doc/api/
- **Ioto Quick Start**: https://www.embedthis.com/doc/start/

### Platform Documentation

#### ESP32
- **ESP-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/
- **ESP32 Technical Reference**: https://www.espressif.com/en/support/documents/technical-documents

#### FreeRTOS
- **FreeRTOS Documentation**: https://www.freertos.org/Documentation/
- **FreeRTOS API Reference**: https://www.freertos.org/a00106.html

### Protocol Documentation

#### MQTT
- **MQTT 3.1.1 Specification**: http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/
- **AWS IoT MQTT**: https://docs.aws.amazon.com/iot/latest/developerguide/mqtt.html

#### HTTP/WebSocket
- **HTTP/1.1 RFC 7230**: https://tools.ietf.org/html/rfc7230
- **WebSocket RFC 6455**: https://tools.ietf.org/html/rfc6455

#### JSON
- **JSON5 Specification**: https://json5.org/
- **JSON RFC 8259**: https://tools.ietf.org/html/rfc8259

### Cryptography

#### OpenSSL
- **OpenSSL Documentation**: https://www.openssl.org/docs/
- **OpenSSL Wiki**: https://wiki.openssl.org/

#### MbedTLS
- **MbedTLS Documentation**: https://tls.mbed.org/
- **MbedTLS API Reference**: https://tls.mbed.org/api/

### AI Integration

#### OpenAI
- **OpenAI API Documentation**: https://platform.openai.com/docs/
- **OpenAI API Reference**: https://platform.openai.com/docs/api-reference

## Build Tools

### MakeMe Build System
- **MakeMe Documentation**: https://www.embedthis.com/makeme/
- **MakeMe Repository**: https://github.com/embedthis/makeme

### TestMe Testing Framework
- **TestMe Documentation**: https://www.embedthis.com/testme/
- **TestMe Repository**: https://github.com/embedthis/testme

## Development Tools

### Compilers
- **GCC Documentation**: https://gcc.gnu.org/onlinedocs/
- **Clang Documentation**: https://clang.llvm.org/docs/
- **Visual Studio**: https://docs.microsoft.com/en-us/cpp/

### Debugging
- **GDB Documentation**: https://sourceware.org/gdb/documentation/
- **Valgrind Manual**: https://valgrind.org/docs/manual/manual.html

## Standards and Best Practices

### C Programming
- **CERT C Coding Standard**: https://wiki.sei.cmu.edu/confluence/display/c/
- **MISRA C**: https://www.misra.org.uk/

### Embedded Systems
- **Embedded C Coding Standard**: https://barrgroup.com/embedded-systems/books/embedded-c-coding-standard

### Security
- **OWASP IoT Security**: https://owasp.org/www-project-internet-of-things/
- **CWE List**: https://cwe.mitre.org/

## Cloud Services

### AWS IoT
- **AWS IoT Core**: https://docs.aws.amazon.com/iot/
- **AWS IoT Device SDK**: https://docs.aws.amazon.com/iot/latest/developerguide/iot-sdks.html
- **AWS IoT Security**: https://docs.aws.amazon.com/iot/latest/developerguide/security.html

## Source Code Repositories

### Project Repositories
- **Ioto Agent**: (Internal repository location)
- **Component Modules**: Located in `paks/` and `src/`

### Related Projects
- **Appweb**: https://github.com/embedthis/appweb
- **GoAhead**: https://github.com/embedthis/goahead

## Community and Support

### Forums and Discussion
- **GitHub Issues**: (Project-specific issue tracker)
- **Community Forums**: https://www.embedthis.com/support/

### Commercial Support
- **Embedthis Support**: https://www.embedthis.com/support/

---

**Last Updated:** 2025-10-24

## Maintenance Notes

When updating this document:
- Add new external references as they become relevant
- Update URLs if documentation moves
- Add new module documentation references when modules are added
- Keep links to official documentation current
- Archive deprecated references
