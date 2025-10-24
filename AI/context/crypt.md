# Crypt Module - (CRYPT)

Cryptographic library providing Base64, SHA1/SHA256 hashing, and Bcrypt functions for embedded IoT applications.

## Module Features

- Base64 encoding/decoding (`cryptEncode64`, `cryptDecode64`)
- SHA1 and SHA256 hashing (MD5 disabled for security)
- Bcrypt password hashing with configurable rounds
- Minimal memory footprint for embedded systems
- Optional MbedTLS/OpenSSL backend integration

## Module-Specific Configuration

Control crypto features via environment variables:
```bash
ME_COM_MBEDTLS=1/0      # Enable/disable MbedTLS backend
ME_COM_OPENSSL=1/0      # Enable/disable OpenSSL backend
ME_CRYPT_BCRYPT=1/0     # Enable/disable Bcrypt support
ME_CRYPT_SHA256=1/0     # Enable/disable SHA256 support
```

Example: `ME_COM_OPENSSL=1 ME_COM_MBEDTLS=0 make`

## Source Organization

```
src/crypt.c             # Main crypto implementation
src/crypt.h             # Public API header
src/r/                  # Embedded safe runtime dependency
src/osdep/              # OS abstraction layer dependency
src/ssl/                # TLS backend (mbedtls or openssl)
test/*.tst.c            # Unit tests for crypto functions
```

## Key APIs

### Base64 Functions
- `char *cryptEncode64(cchar *str)` - Encode string to Base64
- `char *cryptDecode64(cchar *str)` - Decode Base64 string
- `char *cryptEncode64Block(cvoid *buf, ssize len)` - Encode binary data

### Hash Functions
- `char *cryptGetSha1(cchar *str)` - SHA1 hash of string
- `char *cryptGetSha256(cchar *str)` - SHA256 hash of string
- `char *cryptGetSha1Block(cvoid *buf, ssize len)` - SHA1 of binary data

### Password Functions
- `char *cryptMakePassword(cchar *password, int saltLength, int rounds)` - Create Bcrypt hash
- `bool cryptCheckPassword(cchar *plaintext, cchar *password)` - Verify password

## Testing

Module-specific test files:
```bash
cd test
testme base64           # Test Base64 encoding/decoding
testme bcrypt           # Test Bcrypt password functions
testme sha1             # Test SHA1 hashing
testme sha256           # Test SHA256 hashing
```

## Distribution

Creates standalone crypto library:
- `make package` generates `dist/cryptLib.c` amalgamated source
- Single-file integration for embedding in other projects
- All dependencies included in amalgamated output

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
- **API Documentation**: `doc/index.html` (generated via `make doc`)

## Important Notes
