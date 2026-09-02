# User Information — Embedthis Ioto 3.1.0

Information and instructions to the user, required by Regulation (EU) 2024/2847 (Cyber Resilience Act) Annex II.

**You are an integrator.** Ioto is a software component that you embed in a device you build and place on the market. Your device's conformity is your assessment, not ours. This document tells you what Ioto does for you, and — more importantly — what it does not do and you must therefore do yourself.

## 1. Manufacturer Information

| | |
|---|---|
| Manufacturer | Embedthis Software |
| Website | https://www.embedthis.com |
| General contact | dev@embedthis.com |
| **Security contact** | **security@embedthis.com** |
| Security advisories | https://github.com/embedthis/ioto/security/advisories |
| Support portal | https://admin.embedthis.com |

Report a suspected vulnerability to security@embedthis.com. Acknowledgement within 3 business days. We operate coordinated disclosure and will credit you unless you prefer otherwise.

## 2. Product Identification

| | |
|---|---|
| Product | Embedthis Ioto |
| Version | 3.1.0 |
| Type | Embeddable ANSI C device agent — library (`libioto.a`) plus reference applications |
| Archive | `ioto-3.1.0-src.tgz` |
| Integrity | SHA-256 published in `SHA256SUMS` alongside the download |
| SBOM | `doc/sbom.json` in this archive (CycloneDX 1.5) |

Verify the archive before use:

```bash
shasum -a 256 -c SHA256SUMS
```

## 3. Intended Purpose

Ioto provides an HTTP/1.1 server with TLS, WebSockets, an MQTT client, an embedded database, an HTTP client and an OpenAI client, for embedding in device firmware. It is single-threaded and uses fiber coroutines.

Intended for embedded devices on customer networks. Assume that network is hostile: devices of this class are routinely reachable from other devices on the same LAN.

## 4. Security Properties

### 4.1 Data Protection

Ioto persists configuration, TLS certificates and keys, the embedded database, served documents, upload temporaries and logs — **all unencrypted**. Encryption at rest is not provided and is your platform's responsibility. Session state is in memory only.

### 4.2 Authentication and Access Control

HTTP Basic, Digest and session/form authentication, with role-based route authorization. There is **no** brute-force protection: no rate limiting and no account lockout. Add these upstream if you need them.

> **Blowfish key length.** The Blowfish key schedule reads 72 bytes, and the key is formed as `salt:username:realm:password` with a 16-byte salt. A username and realm totalling 53 characters or more would therefore have displaced the password from the key entirely. Ioto now **refuses** such a key rather than truncating it, so the case fails closed: creating or verifying a `BF1:` password with an over-long identity returns failure and logs the reason. If you hit this, shorten the username or realm, or use `SHA256:`.

### 4.3 Cryptographic Features

TLS via OpenSSL (default) or MbedTLS, for both inbound and outbound connections. Peer certificate verification is enabled by default and validates expiry, validity window, hostname and issuer trust.

Two limits you must know:

- **The TLS library is yours.** OpenSSL and MbedTLS are not bundled; your toolchain supplies the version. **You must track advisories for the version you link** — we cannot know or control it.

The certificates in `certs/` are self-signed development material. **They must never reach production.**

### 4.4 Logging and Monitoring

Configurable logging with levels and filters, covering requests, authentication outcomes and errors. There is no separate security event channel — filter the operational log if you need an audit trail.

`rDebug` output may include passwords, keys and tokens. This is intentional for development. **Do not enable debug logging in production.**

### 4.5 Secure Defaults

The shipped application templates bind **loopback on an unprivileged port**, so out of the box the server is reachable only from the device itself and does not need root. They carry baseline security headers, a connection limit and finite timeouts.

They are still starting points, not deployment baselines. Before the device leaves a trusted network you must edit `web.json5`: bind the interface you intend to serve, use an https endpoint with a real certificate, and give every route an explicit `role` unless it is genuinely public.

Ioto emits `X-Content-Type-Options: nosniff`, `X-Frame-Options: SAMEORIGIN` and `Referrer-Policy: no-referrer` from code unless your configuration names them, so a configuration written from scratch still gets them.

An unrecognised route key is logged with a warning naming the route, so a misspelled `role` or `authType` is visible at startup — but it is still ignored, so read your startup log rather than assuming the route table parsed as intended.

## 5. Foreseeable Misuse

- Deploying a template application unchanged — see §4.5.
- Shipping the development certificates in `certs/`.
- Enabling debug logging in production.
- Using `BF1:` passwords in this version.
- Running the agent as root. Ioto does **not** drop privileges; it runs with whatever privilege you give it, and so does any defect in it.
- Assuming Ioto updates itself. It does not — see §8.
- Performing blocking work in a handler. The event loop is single-threaded; one blocking call stalls every connection.
- Exposing the management interface to the internet without an authenticating reverse proxy.

## 6. Support Period

| | |
|---|---|
| Support period | 5 years from release |
| Support ends | 5 years from the 3.1.0 release date |
| Download availability | At least 10 years |
| Cost of security updates | Free of charge |

During the support period we remediate vulnerabilities without undue delay and notify subscribers before public disclosure.

## 7. Secure Commissioning Guidance

### 7.1 Installation

Verify the archive checksum. Build from source with your toolchain. Recommended hardening flags, which the Ioto build does not set because your toolchain governs:

```
-fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -fPIE -pie
-Wl,-z,relro,-z,now -Wl,-z,noexecstack
```

### 7.2 Initial Configuration

1. Replace the development certificates with production material.
2. Enable TLS on every listener carrying credentials or sensitive data.
3. Give every route an explicit `role`; remove or authenticate the catch-all.
4. Review `web.limits.sessions` (default 100) against your expected concurrency. Sessions are created before authentication, but the server evicts the least recently used *unauthenticated* session before refusing, so anonymous traffic cannot lock out logins.
5. Set `web.limits.body`, `upload`, `header` and `connections` to your device's real limits.
6. Confirm debug logging is off.

### 7.3 Integration

Run unprivileged. Compile out what you do not use (`ME_WEB_UPLOAD`, `ME_WEB_SESSIONS`, `ME_COM_WEBSOCK`, `ME_COM_MQTT`, `ME_COM_OPENAI`) — this is the most effective attack-surface reduction available to you. Size `limits.fiberStack` for your deepest call chain. Never block in a handler.

## 8. Update Installation Guidance

> **Ioto contains no update mechanism.** There is no over-the-air update path, no signature verification and no rollback in the device agent. Building a secure update mechanism for your device is **your responsibility**, and CRA Annex I Part I(3) will hold you to it in your own conformity assessment.

### 8.1 Obtaining Updates

From the Builder portal (https://admin.embedthis.com) or the public repository. Subscribers receive private email notification of security updates.

### 8.2 Verifying Integrity

Verify the published SHA-256 before use. Downloads are TLS-protected.

### 8.3 Applying Updates

Rebuild your firmware against the updated source and distribute it through your own update mechanism. A fix released by us is not a fix on your devices until you do this.

### 8.4 Automatic Updates

Not applicable — see above. If you implement automatic updates, CRA requires them to verify authenticity before installation and to fail safe.

## 9. Secure Decommissioning and Data Removal

### 9.1 Data Removal

Ioto provides no secure-erase facility. On decommissioning, erase: the embedded database; the TLS private key and certificates; configuration files containing usernames and password hashes; residual upload temporaries; and logs.

### 9.2 Decommissioning Steps

Stop the agent, erase the items above using a method appropriate to your storage medium (flash requires more than file deletion), then verify. Revoke any device credentials held by your cloud or broker.

### 9.3 Data Transfer

Ioto exposes no data-export function. Extracting data before decommissioning is your application's responsibility.

## 10. Residual Risks You Inherit

Stated so you inherit them knowingly:

1. **No update mechanism** — you must build one.
2. **No privilege separation** — Ioto does not drop privileges or sandbox itself.
3. **No encryption at rest** — hashes, keys and database contents are stored in the clear.
4. **No secure erase** — decommissioning is yours.
5. **TLS library version not controlled by us** — you select and must monitor it.
6. **Single-threaded stall model** — any blocking operation stalls the whole agent.
7. **No platform containment on constrained targets** — on ESP32 and FreeRTOS there is no MMU or process isolation, so a memory-safety defect is not contained.
8. **No brute-force protection** on authentication.
9. **No dedicated security log channel** — security events are interleaved with operational tracing.

---

*Accompanies Embedthis Ioto 3.1.0 as required by CRA Annex II. Retained by the manufacturer for the support period.*
