# Ioto Device Agent Security Measures

**Last Updated:** 2025-10-19
**Version:** 1.0
**Status:** Active

---

## Executive Summary

This document provides an overview of the security measures implemented in the Ioto Device Agent. The agent is designed for embedded IoT applications where security is paramount, combining multiple layers of protection including transport security, authentication, authorization, input validation, secure coding practices, and runtime protections.

### Security Philosophy

The Ioto Device Agent follows a **defense-in-depth** security approach with the following principles:

1. **Secure by Design** - Security is built into the architecture, not bolted on
2. **Least Privilege** - Components operate with minimum necessary permissions
3. **Defense in Depth** - Multiple layers of security controls
4. **Fail Secure** - Security failures result in safe, secure states
5. **Trust but Verify** - Validate all inputs and external data
6. **Transparency** - Clear documentation of security boundaries and assumptions

### Target Threat Model

**In Scope:**
- Network-based attacks (MITM, eavesdropping, replay attacks)
- Malicious cloud services or compromised cloud accounts
- Malformed or malicious data from external sources
- Unauthorized access to device APIs and services
- Code injection and command injection attacks
- Cross-site scripting (XSS) and cross-site request forgery (XSRF)
- Denial of service (resource exhaustion)
- Data tampering and integrity attacks

**Out of Scope:**
- Physical device tampering or hardware attacks
- Side-channel attacks (timing, power analysis)
- Compromised device firmware or bootloader
- Compromised file system or operating system
- DNS spoofing (assumed secure DNS infrastructure)
- Social engineering attacks against developers

### Intended Users

The Ioto Device Agent is designed for **experienced embedded developers** who:
- Embed this software in device firmware or other projects
- Are responsible for securing the broader system and validating all inputs
- Are responsible for secure configuration of the system and software
- Understand embedded security best practices
- Have full control over the device build and deployment environment

---

## Table of Contents

1. [Transport Security](#transport-security)
2. [Authentication & Authorization](#authentication--authorization)
3. [Certificate Management](#certificate-management)
4. [Input Validation & Sanitization](#input-validation--sanitization)
5. [Cryptographic Protections](#cryptographic-protections)
6. [Web Server Security](#web-server-security)
7. [Database Security](#database-security)
8. [MQTT Security](#mqtt-security)
9. [Cloud Integration Security](#cloud-integration-security)
10. [Memory Safety](#memory-safety)
11. [Secure Coding Practices](#secure-coding-practices)
12. [Configuration Security](#configuration-security)
13. [Logging & Monitoring](#logging--monitoring)
14. [Security Testing](#security-testing)
15. [Intentional Security Trade-offs](#intentional-security-trade-offs)
16. [Security Best Practices for Developers](#security-best-practices-for-developers)

---

## 1. Transport Security

### TLS/SSL Encryption

**Implementation:**
- All cloud communications encrypted using TLS 1.2+
- Support for both OpenSSL and MbedTLS cryptographic stacks
- Strong cipher suite configuration
- Certificate-based mutual authentication

**Files:**
- [src/cloud/provision.c](../../src/cloud/provision.c) - Secure provisioning with TLS
- [src/mqtt.c](../../src/mqtt.c) - MQTT over TLS
- [src/webserver.c](../../src/webserver.c) - HTTPS web server
- [paks/crypt/](../../paks/crypt/) - Cryptographic module

**Configuration:**
```json5
// ioto.json5
{
    tls: {
        certificate: '@certs/device.crt',  // Device certificate
        key: '@certs/device.key',          // Private key
        verify: true,                       // Verify peer certificates
        minVersion: 'TLS1.2'                // Minimum TLS version
    }
}
```

**Security Features:**
- Perfect forward secrecy (PFS) cipher suites preferred
- Certificate chain validation
- Hostname verification for server certificates
- Secure renegotiation support
- Session resumption with security checks

**Cipher Suite Selection:**
- OpenSSL: Uses secure defaults with preference for ECDHE and AES-GCM
- MbedTLS: Configured for resource-constrained devices with strong security
- Weak ciphers (RC4, DES, MD5) explicitly disabled

### Protocol Security

**HTTP/HTTPS:**
- HTTP allowed for development with self-signed certificates
- HTTPS enforced for production cloud connections
- Secure headers (HSTS, CSP, X-Frame-Options) configurable
- HTTP/1.1 with security extensions

**MQTT over TLS:**
- TLS 1.2+ required for cloud MQTT connections
- Client certificate authentication
- AWS IoT Core security best practices
- Connection throttling and rate limiting

---

## 2. Authentication & Authorization

### User Authentication (Web Server)

**Implementation:**
- Session-based authentication with secure session tokens
- Password hashing (not stored in plain text)
- Integration with embedded database for user/role management
- Login/logout API endpoints

**Files:**
- [src/webserver.c](../../src/webserver.c:45-50) - Ioto web server module
- [paks/web/](../../paks/web/) - Web HTTP engine

**Session Security:**
- Secure, random session identifiers
- Session timeout enforcement
- Session invalidation on logout
- HttpOnly and Secure cookie flags (HTTPS mode)

**Password Security:**
- Passwords never logged or transmitted in plain text
- Secure password hashing (implementation in web module)
- Password strength requirements configurable
- Brute-force protection via rate limiting

### Role-Based Access Control (RBAC)

**Implementation:**
```json5
// Route configuration with role requirements
{
    routes: [
        {
            match: '/api/admin/',
            role: 'admin',           // Requires admin role
            handler: 'action',
            methods: ['GET', 'POST']
        },
        {
            match: '/api/user/',
            role: 'user',            // Requires user role
            handler: 'action'
        }
    ]
}
```

**Role Hierarchy:**
- Roles defined in configuration file
- Fine-grained permissions per API endpoint

### Cloud Authentication

**Device Provisioning:**
- Unique device ID and API token
- Bearer token authentication for provisioning
- Time-limited registration tokens
- Certificate-based authentication post-provisioning

**Files:**
- [src/cloud/provision.c](../../src/cloud/provision.c:105-129) - Secure device provisioning
- [src/register.c](../../src/register.c) - Device registration

**Provisioning Flow:**
1. Device registers with unique ID and claim token
2. User claims device via Builder/Manager
3. Device provisions certificates and endpoint
4. All subsequent communication uses certificate authentication

---

## 3. Certificate Management

### Certificate Types

**Development Certificates:**
- Test certificates located in `state/certs/`
- Self-signed certificates for local development
- Not suitable for production use

**Production Certificates:**
- Generated during cloud device provisioning
- AWS IoT Core certificates for cloud connectivity
- Certificate chain validation required
- Private keys secured with file system permissions (600)

### Certificate Files

```bash
state/certs/
├── aws.crt          # AWS IoT Core root CA
├── ca.crt           # Certificate Authority (CA) - test only
├── ca.key           # CA private key - test only
├── device.crt       # Device certificate (provisioned)
├── device.key       # Device private key (provisioned)
├── roots.crt        # Root CA bundle
├── test.crt         # Test certificate
└── test.key         # Test private key
```

### Certificate Rotation

**Current Implementation:**
- Manual certificate rotation supported
- Certificates loaded at runtime from configuration
- Restart required for certificate changes

**Planned Enhancements (Q4 2025):**
- Automated certificate renewal
- Zero-downtime certificate updates
- Integration with AWS IoT certificate provisioning
- Certificate expiration monitoring

### Certificate Security

**Best Practices Enforced:**
- Private keys never transmitted over network
- Private keys have restrictive file permissions (600)
- Certificate validation before accepting connections
- Expired certificate detection and warning
- Certificate chain verification

**Files:**
- [src/cloud/provision.c](../../src/cloud/provision.c:132-180) - Certificate provisioning and storage

---

## 4. Input Validation & Sanitization

### API Signature Validation

**Overview:**
The web server supports JSON schema-based validation of HTTP requests and responses using API signatures. This provides strong input validation and output verification.

Invalid requests are rejected with a 400 Bad Request response. Invalid responses can optionally be rejected with a 500 Internal Server Error response.

**Implementation:**
```json5
// signatures.json5
{
    'user/update': {
        request: {
            params: {
                id: {type: 'string', required: true},
            },
            body: {
                name: {type: 'string', required: true},
                email: {type: 'string'},
                age: {type: 'number'}
            }
        },
        response: {
            success: {type: 'boolean'},
            user: {type: 'object'}
        }
    }
}
```

**Validation Features:**
- Type validation (string, number, boolean, array, object)
- Required field enforcement
- Nested object validation (recursive)
- Array element validation

**Future Validation Features:**
- Format validation (email, URL, date, etc.)
- Range validation (min/max for numbers, length for strings)

**Files:**
- [paks/web/dist/web.h](../../paks/web/dist/web.h:892-944) - Signature validation APIs
- Web server validates requests against signature definitions
- Configurable strict mode (reject invalid responses)

**Security Benefits:**
- Prevents injection attacks (SQL, command, etc.)
- Rejects malformed data before processing
- Prevents buffer overflows from oversized inputs
- Ensures type safety across API boundaries

### URL and Path Validation

**Implementation:**
- Path traversal prevention (../ sequences blocked)
- URL encoding validation
- Maximum URL length enforcement
- Query parameter validation

**Security Measures:**
- Canonicalization of file paths
- Validation against allowed document roots
- Rejection of suspicious patterns
- Null byte injection prevention

### HTTP Header Validation

**Security Controls:**
- Maximum header size limits (default: 10KB)
- Header count limits
- Cookie size limits (default: 8KB)
- Header value sanitization
- Rejection of malformed headers

**Configuration:**
```json5
// web.json5
{
    limits: {
        header: '10K',         // Maximum header size
        connections: '100',     // Maximum concurrent connections
        body: '100K',          // Maximum request body size
        upload: '20MB'         // Maximum upload size
    }
}
```

### Database Query Validation

**Parameterized Queries:**
- All database queries use parameterized statements
- No string concatenation for query construction

**Data Type Validation:**
- Schema-based type checking
- Range validation for numeric values
- Length validation for strings
- Foreign key constraint enforcement

---

## 5. Cryptographic Protections

### TLS Stacks Supported

**OpenSSL (Default - Production):**
- Version: 1.1.1+ or 3.0+
- Strong cipher suite defaults
- Hardware acceleration support
- Performance optimized

**MbedTLS (Embedded - ESP32):**
- Version: 3.0+
- Optimized for constrained devices
- Smaller memory footprint
- Configurable security features
- FreeRTOS/ESP32 platform default

**Configuration:**
```bash
# Build with OpenSSL (default)
make

# Build with MbedTLS
make ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0
```

### Cryptographic Operations

**Hashing:**
- SHA-256, SHA-384, SHA-512 for integrity checks
- MD5 supported for legacy compatibility only (clearly documented)
- HMAC for message authentication

**Random Number Generation:**
- Cryptographically secure RNG for:
  - Session tokens
  - XSRF tokens
  - Nonces

**Note on RAND() Usage:**
Some code uses `rand()` for non-security-critical operations (e.g., MQTT schedule jitter). These are clearly marked with security comments:
```c
// SECURITY Acceptable: rand() used for MQTT jitter, not security-critical
jitter = rand() % maxJitter;
```

### Key Management

**Private Keys:**
- Never stored in source code or configuration
- File system permissions enforced (600)
- Loaded from secure storage at runtime
- Never transmitted over network
- Never logged or exposed in debug output

**API Keys/Tokens:**
- Bearer tokens for API authentication
- Time-limited tokens for device registration
- Tokens never logged in production builds
- Secure token generation and validation

---

## 6. Web Server Security

### XSRF (Cross-Site Request Forgery) Protection

**Implementation:**
- XSRF tokens generated per session
- Token validation for state-changing requests (POST, PUT, DELETE)
- Token embedded in forms and checked on submission
- Token sent via HTTP header (`X-XSRF-TOKEN`)

**Configuration:**
```json5
// Route with XSRF protection enabled
{
    routes: [
        {
            match: '/api/',
            xsrf: true,        // Require XSRF token
            methods: ['POST', 'PUT', 'DELETE']
        }
    ]
}
```

**Files:**
- [paks/web/dist/web.h](../../paks/web/dist/web.h:98-112) - XSRF token definitions
- Session storage for XSRF tokens
- Automatic token validation

### Security Headers

**Implemented Headers:**
```json5
{
    headers: {
        'Access-Control-Expose-Headers': 'X-XSRF-TOKEN',
        'X-Content-Type-Options': 'nosniff',              // Prevent MIME sniffing
        'X-Frame-Options': 'SAMEORIGIN',                  // Clickjacking protection
        'X-XSS-Protection': '1; mode=block',              // XSS filter
        'Referrer-Policy': 'same-origin',                 // Limit referrer exposure
        'Content-Security-Policy': "default-src 'self'",  // CSP policy
        'Strict-Transport-Security': 'max-age=31536000'   // HSTS (HTTPS only)
    }
}
```

**CORS Configuration:**
```json5
{
    headers: {
        'Access-Control-Allow-Origin': 'https://trusted.com',
        'Access-Control-Allow-Methods': 'GET, POST',
        'Access-Control-Allow-Headers': 'content-type, authorization',
        'Access-Control-Allow-Credentials': 'true'
    }
}
```

### Resource Limits

**Request Limits:**
- Maximum request body size (configurable, default: 100KB)
- Maximum upload size (configurable, default: 20MB)
- Maximum header size (configurable, default: 10KB)
- Maximum URL length
- Maximum cookie size (8KB security limit)

**Connection Limits:**
- Maximum concurrent connections (configurable)
- Connection rate limiting
- Per-IP connection limits
- Idle connection timeout

**Timeout Protection:**
```json5
{
    timeouts: {
        inactivity: '300 secs',    // Idle timeout
        parse: '10 secs',          // Header parse timeout
        request: '10 mins',        // Total request timeout
        session: '30 mins',        // Session timeout
        tls: '1 day'               // TLS session cache
    }
}
```

### File Upload Security

**Security Measures:**
- Upload size limits enforced
- File type validation
- Temporary upload directory with restricted permissions
- Automatic cleanup of temporary files
- Path traversal prevention
- Virus scanning integration supported

**Configuration:**
```json5
{
    upload: {
        dir: 'tmp',             // Upload directory (sandboxed)
        remove: true,           // Auto-cleanup
        maxSize: '20MB'         // Size limit
    }
}
```

### WebSocket Security

**Implementation:**
- WebSocket upgrade over HTTPS only (production)
- Origin validation
- Authentication required before upgrade
- Message size limits
- UTF-8 validation (configurable)
- Ping/pong for connection liveness

**Configuration:**
```json5
{
    webSockets: {
        ping: '30 secs',           // Keepalive interval
        protocol: 'chat',          // Subprotocol
        validateUTF: true,         // UTF-8 validation
        maxFrame: '100K',          // Frame size limit
        maxMessage: '100K'         // Message size limit
    }
}
```

---

## 7. Database Security

### Access Control

**Schema-Based Security:**
- Database schema defines permissions
- Row-level security supported
- Query validation against schema
- Type-safe operations

**Parameterized Queries:**
- All queries use safe, parameterized API
- No string concatenation

### Data Integrity

**Validation:**
- Schema-based type validation
- Constraint enforcement
- Unique constraint checking

### Synchronization Security

**Cloud Sync Protection:**
- TLS encryption for all sync traffic
- Certificate-based authentication
- Sync log integrity checking
- Conflict resolution with validation

**Files:**
- [src/cloud/sync.c](../../src/cloud/sync.c) - Database synchronization
- Sync log format documented for integrity

---

## 8. MQTT Security

### Transport Security

**TLS Requirements:**
- MQTT over TLS required for cloud connections
- Certificate-based mutual authentication
- AWS IoT Core security standards compliance
- Secure cipher suite configuration

**Configuration:**
```json5
{
    mqtt: {
        authority: '@certs/aws.crt',    // AWS root CA
        certificate: '@certs/device.crt', // Device cert
        key: '@certs/device.key',       // Device key
        verify: true                     // Verify peer
    }
}
```

### Message Security

**Topic Authorization:**
- AWS IoT Core policy-based topic access
- Device-specific topic permissions
- Wildcard topic subscription restrictions

**Message Validation:**
- Size limits enforced
- Format validation for payloads
- Rate limiting and throttling
- Malformed message rejection

**Files:**
- [src/mqtt.c](../../src/mqtt.c) - MQTT client with TLS

### Connection Security

**Authentication:**
- Certificate-based client authentication
- Device identity verified via X.509 certificate
- No username/password authentication (more secure)

**Connection Management:**
- Automatic reconnection with backoff
- Connection throttling to prevent DoS
- Keep-alive for connection liveness
- Graceful disconnection

**Throttling:**
```c
// Connection throttling to prevent abuse
static void throttle(const MqttRecv *rp) {
    // Exponential backoff on connection failures
    // Rate limiting for excessive reconnects
}
```

---

## 9. Cloud Integration Security

### Device Provisioning

**Secure Provisioning Flow:**
1. Device generates unique ID
2. Device requests claim token from Builder
3. User claims device via authenticated Builder session
4. Device provisions certificates and endpoint via secure API
5. Device validates provisioning response
6. Device stores certificates securely

**Security Features:**
- Bearer token authentication for provisioning API
- Time-limited claim tokens
- One-time use provisioning tokens
- Certificate validation before storage
- Secure storage of provisioned credentials

**Files:**
- [src/cloud/provision.c](../../src/cloud/provision.c) - Secure device provisioning
- [src/register.c](../../src/register.c) - Device registration

**Rate Limiting:**
- Exponential backoff for provisioning failures
- Temporary blocking for excessive requests
- Configurable retry limits

**Configuration:**
```json5
{
    limits: {
        reprovision: 5      // Max reprovision attempts before blocking
    }
}
```

### AWS IoT Core Integration

**Security Best Practices:**
- Mutual TLS authentication (device + cloud)
- AWS IoT policies for fine-grained permissions
- Device shadows for secure state management
- Thing registry for device identity

**Device Shadow Security:**
- Authenticated access only
- State change validation
- Versioning for conflict detection
- Delta updates to minimize exposure

### OTA (Over-The-Air) Updates

**Update Security:**
- HTTPS download of update packages
- Digital signature verification of updates
- Rollback on verification failure
- Secure update script execution

**Planned Enhancements (Q1 2026):**
- A/B partition support for safe updates
- Delta updates to reduce attack surface
- Enhanced signature verification
- Update integrity checksums

**Files:**
- [src/cloud/update.c](../../src/cloud/update.c) - OTA update management

---

## 10. Memory Safety

### Safe Runtime Foundation

**Overview:**
All Ioto modules are built on the Safe Runtime (R module) which provides memory-safe operations and prevents common vulnerabilities.

**Files:**
- [paks/r/](../../paks/r/) - Safe runtime module

**Memory Safety Features:**

#### 1. Bounds Checking
```c
// Safe string operations with bounds checking
scopy(dest, destSize, src);        // vs strcpy (unsafe)
slen(str);                          // NULL-safe strlen
scmp(s1, s2);                       // NULL-safe strcmp
```

#### 2. NULL Tolerance
- Most functions gracefully handle NULL arguments
- No crashes from NULL pointer dereferences
- Consistent error handling

#### 3. Memory Allocation Safety
```c
// Global allocation failure handler
// No need to check every allocation
char *buf = rAlloc(size);    // Failure handled globally

// Proper cleanup with Safe Runtime
rFree(buf);                  // Use rFree for runtime allocations
```

**Important:** The project uses a global memory allocation failure handler. Code does NOT explicitly check every allocation for NULL since failures are handled at a higher level.

#### 4. Buffer Management
```c
// Safe buffer operations
RBuf *buf = rAllocBuf(size);
rPutStringToBuf(buf, data);      // Automatic resizing
rGetBufLength(buf);               // Safe length query
```

### Stack Safety

**Fiber Stack Management:**
- Configurable stack size per fiber
- Stack overflow detection (debug builds)
- Stack usage tracking and profiling
- Warnings for excessive stack usage

**Configuration:**
```json5
{
    limits: {
        fiberStack: '64k'    // Fiber stack size (adjustable)
    }
}
```

**Debug Features:**
```bash
# Enable stack checking in debug builds
DFLAGS=-DME_FIBER_CHECK_STACK=1 make clean build
```

### Prevention of Common Vulnerabilities

**Buffer Overflows:**
- All string operations use Safe Runtime APIs
- Bounds checking on all buffer operations
- No use of unsafe C functions (strcpy, sprintf, etc.)

**Use-After-Free:**
- Reference counting for shared objects
- Clear ownership semantics
- Automatic cleanup on scope exit

**Integer Overflows:**
- Use of fixed-size types (int32, uint64, etc.)
- Range validation on arithmetic operations
- Safe casting with validation

---

## 11. Secure Coding Practices

### Coding Standards

**Language Choice:**
- ANSI C for maximum portability
- Modern C features used safely
- Consistent coding style enforced

**Variable Declarations:**
- Variables declared at function top (not C99 inline)
- Clear initialization patterns
- Minimal use of global variables

### String Handling

**Safe String APIs:**
```c
// ALWAYS use Safe Runtime functions
slen(str)           // instead of strlen
scopy(d, len, s)    // instead of strcpy
scmp(s1, s2)        // instead of strcmp
sfmt(buf, len, fmt) // instead of sprintf
```

**Never Use:**
- `strcpy`, `strcat`, `sprintf` (buffer overflows)
- `gets` (buffer overflows)
- `strtok` (not thread-safe, modifies input)

### Format String Safety

**Secure Formatting:**
```c
// Safe formatting with bounds checking
rSprintf(buf, size, fmt, ...);

// Safe dynamic allocation
char *msg = rSprintf(NULL, -1, fmt, ...);  // Auto-size
```

**Never Use:**
- User-controlled format strings
- `printf` family with unsanitized input

### Error Handling

**Consistent Error Codes:**
```c
#define R_ERR_CANT_OPEN         -1
#define R_ERR_CANT_READ         -2
#define R_ERR_CANT_WRITE        -3
#define R_ERR_BAD_ARGS          -4
#define R_ERR_MEMORY            -5
// ... etc
```

**Error Propagation:**
- Clear error return values
- Error context preserved
- Logging at appropriate level
- No silent failures

### Resource Management

**Sockets:**
- Proper socket lifecycle management
- Cleanup on error paths
- Timeout enforcement

**Memory:**
- Use rAlloc/rFree exclusively (not malloc/free)
- Free on all error paths
- No memory leaks

### Logging Safety

**Secure Logging:**
```c
// Structured logging with levels
rError("component", "Error message");
rInfo("component", "Info message");
rDebug("component", "Debug message");  // Debug builds only
```

**Security Considerations:**
- Passwords never logged
- API tokens never logged in production
- Debug logs may contain sensitive data (documented)
- Production builds: minimal logging

**Configuration:**
```json5
{
    log: {
        types: 'error,info',           // Production: errors + info only
        sources: 'all,!mbedtls',       // Filter noisy components
        path: '/var/log/ioto.log'      // Secure log location
    }
}
```

---

## 12. Configuration Security

### Configuration Files

**File Types:**
- `ioto.json5` - Main configuration
- `web.json5` - Web server configuration
- `schema.json5` - Database schema
- `device.json5` - Device registration
- `signatures.json5` - API signatures

**Security Considerations:**
- Configuration files control security policies
- Developers responsible for secure configuration
- Production vs. development profiles
- Sensitive data in configuration protected

### Configuration Validation

**Validation Features:**
- JSON5 schema validation
- Type checking for all properties
- Range validation for numeric values
- Required field enforcement
- Invalid configuration rejected at startup

**Planned Enhancement (Q4 2025):**
- Configuration validator tool
- Pre-deployment validation
- Security policy checking
- Best practice recommendations

### Secrets Management

**Current Approach:**
- Certificates and keys in `state/` directory
- File system permissions (600 for keys)
- Not committed to source control
- Environment-specific secrets

**Best Practices:**
- Never commit real certificates/keys to Git
- Use `.gitignore` for `state/` directory
- Separate dev and production secrets
- Rotate credentials regularly

---

## 13. Logging & Monitoring

### Security Logging

**Log Levels:**
- `error` - Security-relevant errors
- `info` - Security events (login, logout, auth failures)
- `debug` - Detailed debug (may contain sensitive data)

**Security-Relevant Events:**
- Authentication failures
- Authorization failures
- Invalid inputs rejected
- Certificate validation failures
- Connection attempts
- Configuration changes

### Audit Logging

**Current Implementation:**
- Basic logging to file or stdout
- Structured log format
- Component-based filtering

**Planned Enhancement (Q4 2025):**
- Security audit logging
- Tamper-evident logs
- CloudWatch Logs integration
- Compliance reporting

**Files:**
- [src/cloud/cloudwatch.c](../../src/cloud/cloudwatch.c) - CloudWatch integration (dedicated clouds)

### Log Security

**Protection Measures:**
- Logs rotated with size/time limits
- Logs stored in secure directory
- Log file permissions restricted
- Sensitive data filtered from logs

**Configuration:**
```json5
{
    log: {
        path: '/var/log/ioto.log',
        format: '%D %H %A[%P] (%T, %S): %M',
        types: 'error,info',           // No debug in production
        sources: 'all,!mbedtls'        // Filter noisy sources
    }
}
```

---

## 14. Security Testing

### Unit Testing

**Test Coverage:**
- 80%+ code coverage target
- Security-critical paths prioritized
- Negative test cases for validation
- Injection attack prevention tests

**Test Framework:**
- TestMe framework for unit testing
- C, shell, and JavaScript test support
- Parallel test execution
- Automated CI/CD integration

**Running Tests:**
```bash
make APP=unit test        # Run all tests
cd test && tm web-*       # Run web tests
cd test && tm mqtt-*      # Run MQTT tests
```

### Security Testing Practices

**Input Validation Testing:**
- Fuzzing of API endpoints
- Malformed data injection
- Boundary value testing
- XSS attempts

**Authentication Testing:**
- Invalid credentials
- Session hijacking attempts
- XSRF token validation
- Expired session handling

**TLS Testing:**
- Certificate validation
- Cipher suite verification
- Protocol version enforcement
- Man-in-the-middle detection

---

## 15. Intentional Security Trade-offs

### Documented Acceptable Security Practices

The following are intentional design decisions that may appear as security issues but are acceptable in the Ioto context:

#### 1. Memory Allocation Checking

**Practice:** Code does NOT explicitly check all memory allocations for failure.

**Rationale:** Ioto uses a global memory allocation failure handler. When memory allocation fails, the handler is invoked to deal with the error at a system level.

**Marking:**
```c
// No explicit check needed - global handler manages allocation failures
char *buf = rAlloc(size);
```

#### 2. Debug Logging

**Practice:** `rDebug()` function may emit sensitive data (passwords, keys, tokens) in debug builds.

**Rationale:** Debug builds are for development only. Developers need full visibility for troubleshooting. Production builds disable debug logging.

**Marking:**
```c
// SECURITY Acceptable: Debug logging may contain sensitive data
rDebug("auth", "Token: %s", token);
```

#### 3. HTTP in Development

**Practice:** Test applications may use HTTP instead of HTTPS.

**Rationale:** Allows use of self-signed certificates in development without certificate warnings. HTTPS is enforced in production.

**Marking:**
```c
// SECURITY Acceptable: HTTP for development, HTTPS required for production
```

#### 4. Rand() for Non-Critical Operations

**Practice:** Standard `rand()` used for non-security operations (e.g., jitter).

**Rationale:** Cryptographically secure RNG not needed for non-security-critical randomness. Reduces overhead.

**Marking:**
```c
// SECURITY Acceptable: rand() used for MQTT jitter, not security-critical
jitter = rand() % maxJitter;
```

#### 5. MD5 for Legacy Support

**Practice:** MD5 hashing supported for legacy digest authentication.

**Rationale:** Required for backward compatibility with legacy systems. Not used for new security features.

**Marking:**
```c
// SECURITY Acceptable: MD5 for legacy digest support only
```

#### 6. DNS and File System Trust

**Practice:** DNS integrity and file system integrity are assumed.

**Rationale:** Embedded devices typically operate in controlled environments. DNS spoofing and file system attacks are out of scope.

**Marking:**
```c
// SECURITY Acceptable: DNS integrity out of scope
// SECURITY Acceptable: File system integrity out of scope
```

#### 7. Developer Responsibility

**Practice:** Local API inputs not extensively validated.

**Rationale:** Intended users are experienced embedded developers who are responsible for validating inputs to their applications.

**Marking:**
```c
// Input validation is the responsibility of the developer integrating Ioto
```

### Security Comment Prefix

All intentional security trade-offs are marked with:
```c
// SECURITY Acceptable: [reason why this is acceptable]
```

**Important:** These comments indicate intentional design decisions that should NOT be flagged as security issues during code review or security scanning.

---

## 16. Security Best Practices for Developers

### For Developers Using Ioto

When integrating the Ioto Device Agent into your embedded application, follow these security best practices:

#### Production Deployment

**Certificate Management:**
```bash
# 1. Never commit real certificates to source control
echo "state/certs/*.crt" >> .gitignore
echo "state/certs/*.key" >> .gitignore

# 2. Set proper file permissions
chmod 600 state/certs/*.key
chmod 644 state/certs/*.crt

# 3. Use production certificates, not test certificates
# Replace test certificates with real AWS IoT certificates
```

**Build Configuration:**
```bash
# Production build with minimal logging
PROFILE=prod OPTIMIZE=release ME_R_LOGGING=0 make

# Use OpenSSL for better performance (vs MbedTLS)
ME_COM_OPENSSL=1 ME_COM_MBEDTLS=0 make
```

**Configuration Security:**
```json5
// ioto.json5 - Production profile
{
    profile: 'prod',
    log: {
        types: 'error,info',        // No debug logs
        path: '/var/log/ioto.log'   // Secure location
    },
    tls: {
        verify: true,                // Always verify certificates
        minVersion: 'TLS1.2'         // Enforce TLS 1.2+
    }
}
```

#### Input Validation

**Always Validate External Inputs:**
```c
// Validate before passing to Ioto APIs
if (inputFromUser) {
    // Validate type, range, format
    if (!isValidInput(input)) {
        return R_ERR_BAD_ARGS;
    }
}
ioProcessInput(input);
```

**Use API Signatures:**
```json5
// Enable strict signature validation
{
    signatures: {
        enable: true,
        strict: true,
        path: 'signatures.json5'
    }
}
```

#### Secure Configuration

**Review Security Settings:**
- Enable XSRF protection for web routes
- Set appropriate resource limits
- Configure strong TLS cipher suites
- Enable security headers (HSTS, CSP)
- Set session timeouts

**Web Server Security Checklist:**
```json5
{
    web: {
        // Enable all security features
        signatures: {enable: true, strict: true},
        headers: {
            'Strict-Transport-Security': 'max-age=31536000',
            'Content-Security-Policy': "default-src 'self'",
            'X-Frame-Options': 'DENY'
        },
        routes: [
            {match: '/api/', xsrf: true, role: 'user'}
        ],
        limits: {
            body: '100K',
            upload: '10MB',
            connections: '50'
        },
        timeouts: {
            session: '15 mins',
            inactivity: '5 mins'
        }
    }
}
```

#### Device Lifecycle Security

**1. Manufacturing:**
- Unique device ID per device
- Secure storage of device credentials
- Serialization service for provisioning

**2. Deployment:**
- Device claim workflow
- Certificate provisioning
- Secure initial configuration

**3. Operation:**
- Monitor for security events
- Log authentication failures
- Detect abnormal behavior

**4. Decommissioning:**
- Revoke device certificates
- Deprovision from cloud
- Securely erase credentials

---

## Appendix A: Security Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    External Threat Landscape                 │
│  (Network attacks, Malicious cloud, Code injection, etc.)   │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       │ TLS 1.2+ Encryption
                       │ Certificate Authentication
                       │
         ┌─────────────┴──────────────┐
         │                            │
    ┌────▼────────┐            ┌──────▼──────┐
    │  MQTT/TLS   │            │  HTTPS/TLS  │
    │  (Port 8883)│            │  (Port 443) │
    └────┬────────┘            └──────┬──────┘
         │                            │
         │ Certificate Auth           │ Session Auth
         │ Topic Authorization        │ XSRF Protection
         │                            │
    ┌────▼────────────────────────────▼─────┐
    │         Ioto Device Agent              │
    │  ┌─────────────────────────────────┐  │
    │  │   Input Validation Layer        │  │
    │  │  - API Signatures               │  │
    │  │  - Type/Range Validation        │  │
    │  │  - Sanitization                 │  │
    │  └──────────┬──────────────────────┘  │
    │             │                          │
    │  ┌──────────▼──────────────────────┐  │
    │  │   Authentication & Authorization│  │
    │  │  - User/Role Management         │  │
    │  │  - Session Management           │  │
    │  │  - RBAC Enforcement             │  │
    │  └──────────┬──────────────────────┘  │
    │             │                          │
    │  ┌──────────▼──────────────────────┐  │
    │  │   Business Logic Layer          │  │
    │  │  - Web Server (Safe APIs)       │  │
    │  │  - MQTT Client                  │  │
    │  │  - Database (Parameterized)     │  │
    │  │  - Cloud Services               │  │
    │  └──────────┬──────────────────────┘  │
    │             │                          │
    │  ┌──────────▼──────────────────────┐  │
    │  │   Safe Runtime Foundation (R)   │  │
    │  │  - Memory Safety                │  │
    │  │  - Bounds Checking              │  │
    │  │  - NULL Tolerance               │  │
    │  │  - Resource Management          │  │
    │  └──────────┬──────────────────────┘  │
    │             │                          │
    └─────────────┼──────────────────────────┘
                  │
                  │ Protected by OS
                  │ File Permissions
                  │
         ┌────────▼────────┐
         │  State Storage  │
         │  - Certificates │
         │  - Database     │
         │  - Config Files │
         └─────────────────┘
```

---

## Related Documentation

- [Product Roadmap](../plans/ROADMAP.md) - Security feature roadmap
- [Design Documentation](DESIGN.md) - Technical architecture
- [Development Plan](../plans/PLAN.md) - Implementation plan
- [Development Procedures](../procedures/PROCEDURE.md) - Security procedures

---

## Document Control

- **Owner:** Security Team / Architecture Team
- **Review Cycle:** Quarterly
- **Next Review:** 2026-01-16
- **Classification:** Internal / Public (no sensitive data)

This document is proprietary and confidential. Distribution limited to approved customers and partners under NDA.

© 2025 Embedthis Software LLC. All rights reserved.

