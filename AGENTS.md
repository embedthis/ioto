# Repository Guidelines

## Project Structure & Module Organization
- `src/` — Core Ioto agent (ANSI C) and services.
- `paks/` — Modular packages: `crypt`, `db`, `json`, `mqtt`, `ssl`, `web`,
`url`, `websockets`.
- `apps/` — Example apps (`demo` default) with `config/` and `src/`.
- `state/` — Runtime state, copied config, database, certificates.
- `build/` — Build outputs; `projects/` — Generated Makefiles; `test/` —
Tests and configs.

## Build, Test, and Development Commands
- Build default app: `make` or `make build`
- Build specific app: `make APP=<demo|ai|auth|blank|unit>`
- Run agent: `make run` or `build/bin/ioto -v`
- Clean: `make clean`; Help: `make help`
- Useful env: `OPTIMIZE=debug|release`, `PROFILE=dev|prod`, `SHOW=1`
- Switch TLS stack: `ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0 make`
- Unit tests: `make APP=unit && make run` (uses `test/` configs)

## Coding Style & Naming Conventions
- Languages: ANSI C for agent; JavaScript/TypeScript for cloud/app utilities.
- Indentation: 4 spaces, max line length 120 chars.
- Naming: camelCase for variables/functions; keep symbols descriptive.
- JavaScript formatting: Prettier (repo `.prettierrc`).
- Runtime: single-threaded with fiber coroutines—prefer non-blocking I/O and
minimal allocations in hot paths.

## Testing Guidelines
- Tests live under `test/`; app-specific test configs live in subdirectories.
- Use the unit app harness: `make APP=unit && make run`.
- Add focused tests for database sync, MQTT flows, and web endpoints where
applicable.
- Name tests clearly by feature (e.g., `test/db-sync/*.json5`,
`test/mqtt/*.json5`).

## Commit & Pull Request Guidelines
- Commit style: prefer Conventional Commits (`feat:`, `fix:`, `docs:`,
`chore:`, `test:`).
- Keep commits small and scoped; reference issues (e.g., `Fixes #123`).
- PRs must include: purpose, summary of changes, testing steps/outputs, and any
config or schema updates (`apps/<APP>/config/`).
- Include platform notes if behavior differs across `PROFILE=dev|prod`.

## Security & Configuration Tips
- Treat `state/` as runtime-only; do not commit real secrets or certs.
- Configure services in `apps/<APP>/config/ioto.json5` (profiles for dev/prod).
- Control fiber stack via `limits.stack` in `ioto.json5`.
- Prefer AWS-native flows; ensure TLS certs are valid before connecting to IoT
Core.

