# Ioto Device Agent 3.3.0 Release Notes

Release Date: October 19, 2026

## Recommended Action

- [ ] Optional Upgrade -- Upgrade only if convenient
- [ ] Recommended Upgrade -- Upgrade recommended but not essential
- [x] **Essential Upgrade** -- All users strongly advised to upgrade

## What's New in 3.3.0

3.3.0 is a security release. A second audit of the assembled agent, its build configuration and its application templates, together with further audits of the modules it vendors, found defects that are fixed here. The default build is now a hardened release build, login failures are limited per client address, and the SBOM is generated from the source tree and names the third-party code each module carries.

### Security

Fixed in this release. Each was verified against the shipped code and carries a regression test.

- The release build configuration did not compile, so every build was a debug build, and the debug build registered an unauthenticated test action table. The default build is now release.
- The fiber ceiling defaulted to 4, so four half-sent requests denied service to the whole agent. It is now derived from `web.limits.connections`. The standalone `web` command had the same default and is fixed the same way.
- The sample and reference `web.json5` were nested one level too deep, so route initialisation failed open to an unauthenticated catch-all file route.
- The `http` template shipped three working accounts with the published password "demo", one with an undocumented super role. Templates seed no accounts, and the built-in login refuses that password and names each such account in the startup log.
- A 37-byte unauthenticated request crashed the `unit` test application through a NULL WebSocket dereference.
- The standalone `web` command fell back to a built-in configuration serving files anonymously on every interface when its configuration was missing or unparseable. It now refuses to start.
- Item responses wrote a borrowed buffer across a fiber yield, so a response suspended on a full socket could send freed memory.
- Users created through the web login API got an empty stored password, which Digest authentication accepted.
- A request whose query satisfied the route's query signature skipped validation of its body, and filtering a nested field stopped validation of every later field.
- Chunked framing accepted a bare CR in the size line and left trailers unparsed, so a trailer could swallow the next pipelined request.
- A failed body drain after an early error stranded the connection slot, and keep-alive handlers stayed armed during later pipelined requests.
- A WebSocket frame containing a NUL byte was delivered with a length beyond its allocation.
- The HTTP client underflowed on an empty header value, and an SSE callback that freed its client caused a double free.
- The runtime's template expander read past an unterminated `${`, `printf` could hang on a full bounded buffer, and the TLS handshake ignored its deadline.
- Unknown users skipped the password check at login and the only throttle was a fixed delay, which leaked which accounts exist. Unknown users are now checked against a decoy, and failures are limited per client address.
- Removing or demoting a user did not revoke their cached role or sessions.
- A database user could log in under the name of a user defined with a password in `web.json5`.
- The `http` template's `getAuth` built its JSON without escaping, and its own login handler could enumerate users and return the password hash. The handler is removed in favour of the built-in login.
- Refused authentications and failed logins are now logged at `info` under the `auth` source, with the client address and route, never the credentials.
- Logout accepted GET, so a cross-site link could log a user out.
- ESP32: certificate validity dates were not checked, the firmware lacked stack protection, and the build installed the published test key in non-development profiles.
- `reset()` restored the database with group-readable permissions, and `state/` was created under the caller's umask.

### Improvements

- **Hardened builds.** The generated Linux and macOS projects compile with `-fstack-protector-strong -D_FORTIFY_SOURCE=3 -fPIE` and link with `-pie -z relro -z now -z noexecstack`. Windows links the dynamic CRT, so Windows Update services CRT security fixes.
- **Warning-free builds are enforced.** The test suite fails on any compiler warning in the debug, release or MbedTLS configurations.
- **Generated SBOM.** The CycloneDX SBOM is generated from the source tree with a SHA-256 for each vendored file, and records the third-party code each module carries.
- **TLS floors stated.** OpenSSL 3.0 and MbedTLS 3.6.7 are enforced at compile time and stated in the README, the SBOM and the ESP-IDF manifests. CI builds and tests MbedTLS 3.6.7.
- **CA bundle.** The bundled Mozilla CA bundle is checked for freshness, and the build no longer fetches an unverified copy.
- `ioto --background` daemonizes correctly on macOS and FreeBSD.
- Applications relink when `libioto.a` changes.
- The `stdout` and `stderr` log paths always mean the console.
- Windows: applications start, because the generated schema no longer carries a byte order mark.
- The `http` template's pages run under its Content Security Policy.
- A restarted web host resumes pruning expired sessions, and a start that cannot bind every endpoint leaves no endpoint listening.
- The samples link against the TLS stack `libioto.a` was built with, so they build with MbedTLS.
- The TLS selection documentation gives the premake regeneration procedure; `make` variables do not select the TLS stack.

### Breaking Changes

- **The default build is release.** `make` and `make.bat` build release. Use `make OPTIMIZE=debug` or `make.bat debug` for a debug build. `make test` still builds debug.
- **Logout requires POST.** A client that logs out with GET must use POST.
- **The `web` command refuses to start without a valid configuration.** Supply a `web.json5`; there is no built-in fallback.
- **The agent no longer overrides `web.upload.dir`.** With none configured, uploads go to the private `@state/uploads` directory.
- **The AI endpoint is read from `ai.endpoint`.** Move a top-level `endpoint` key there.
- **The built-in login verifies database passwords over `username:realm:password`,** as HTTP Basic does, so a hash made by the `password` utility logs in and a hash made over the bare password does not. The utility's default realm is now `web`, the server's default, instead of `example.com`.
- **Failed authentications are limited per client address.** After `web.auth.maxFailures` failures (default 10) within `web.auth.failurePeriod` (default 5 minutes), HTTP Basic, Digest and built-in login attempts from that address get a `429` until the period ends. Set `maxFailures` to 0 to disable.

### Vulnerability Disposition

All SBOM component vulnerabilities reviewed. No known exploitable vulnerabilities at time of release.

### Compatibility

- Platform support: Tier 1 Linux, macOS, Windows. Tier 2 ESP32, FreeBSD, FreeRTOS, VxWorks.
- TLS: OpenSSL 3.0 or later, or MbedTLS 3.6.7 or later in the 3.x line. MbedTLS 4.x is not supported.
- API: source compatible with 3.2.0, except for the changes listed above.
- Module versions: r 1.5.2, url 1.4.0, web 1.4.1, json 1.2.1, crypt 1.2.1, db 1.2.1, mqtt 1.2.1, websock 1.2.1, openai 1.2.1, osdep 1.2.4, uctx 1.2.2, certs 1.3.2.

## Upgrade Instructions

1. Verify the archive with `shasum -a 256 -c SHA256SUMS`.
2. Rebuild. If you relied on the default build being debug, build with `OPTIMIZE=debug`.
3. If you call logout with GET, change it to POST.
4. If you start the `web` command without a configuration file, supply one.
5. If you set a top-level `endpoint` for the AI service, move it to `ai.endpoint`.
6. If the device was built from the 3.1.x `http` template, delete or re-password the `admin`, `guest` and `super` accounts in the `User` table before redeploying.
7. If your application creates `BF1:` hashes for database users itself, make them over `username:realm:password` and re-hash existing ones. Hashes made with the `password` utility's old default realm `example.com` need that realm configured or a re-hash.
8. Review `web.auth.maxFailures` and `web.auth.failurePeriod` for your deployment.
9. Rebuild your firmware and distribute it through your own update mechanism.

## Download

Download from the [Builder Site](https://admin.embedthis.com/product). Verify the archive with `shasum -a 256 -c SHA256SUMS`.

## Documentation

Full documentation available at: https://www.embedthis.com/doc/

## Support

- Documentation: https://www.embedthis.com/doc/
- Security: security@embedthis.com

## EU Cyber Resilience Act

The commercial source distribution carries the Annex II user information as `doc/user-information.md`, the EU Declaration of Conformity as `doc/declaration-of-conformity.md`, and the SBOM as `doc/sbom.json`. The open source release in the public GitHub repository carries none of them: a manufacturer that integrates it into a product placed on the EU market is responsible for that product's conformity.
