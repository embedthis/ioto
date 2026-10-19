# Ioto Agent Test Suite

Tests for the Ioto Device Agent, run with the TestMe `tm` tool. `make test` from the project root runs everything; `tm` from this directory does the same without the rebuild check.

## Test Groups

Two kinds of group live here. The **vendored** groups are copies of the sibling modules' own suites, so each module's tests run against the amalgamation the agent ships; `bin/sync-tests.sh` overwrites them and `make check-tests` proves they match. The **agent-owned** groups test what only the agent has: the command line, the link surface, the vendoring guards, the security harness, and the assembled agent running as a process.

| Group | Owner | What it covers |
|-------|-------|----------------|
| [db/](db/) | vendored from `db` | Embedded database |
| [json/](json/) | vendored from `json` | JSON5 parser |
| [mqtt/](mqtt/) | vendored from `mqtt` | MQTT client |
| [r/](r/) | vendored from `r` | Safe runtime |
| [url/](url/) | vendored from `url` | HTTP client, against the `web` command |
| [web/](web/) | vendored from `web` | Web server, against the `web` command; includes the manual `bench/` and `leak/` groups |
| [command/](command/) | agent | The `ioto` command line |
| [log/](log/) | agent | Where the agent sends its log for each `log.path` |
| [link/](link/) | agent | Linking the agent library into an application |
| [guard/](guard/) | agent | The vendored copies match their sources |
| [sec/](sec/) | agent | Security fuzzing harness, maintained by `sec-scan` |
| [soak/](soak/) | agent | A sustained mixed workload through the assembled agent, with resource figures |
| [stress/](stress/) | agent | Request sizes and counts that scale with depth |

`utils/` holds what the agent-owned groups share: the launcher and configuration for running `ioto-unit` from a test group, and `scenario.h` for the C tests that drive it. `certs/` holds the test certificates.

## Running Tests

```bash
cd test
tm                          # Everything, at depth 0
tm web                      # One group
tm web/file.tst.c           # One test
tm --depth 3 soak           # A one-minute soak
tm --depth 4 stress         # 512 KB uploads, 200 sessions
tm --depth 5 stress         # Adds the 5 GB transfer
```

## Depth

`tm --depth N` selects how hard the scaling tests push; the default is 0 and everything runs at every depth. Most tests ignore it. The ones that do not:

| Test | What depth scales |
|------|-------------------|
| `soak/soak.tst.c` | Duration: 3s, 10s, 30s, 60s, 120s, 240s, 480s, 900s, 1800s, 3600s. Depth 0 is a smoke test of the harness; the real runs are depth 3 and above, invoked by hand |
| `stress/put.tst.c` | PUT body size, 1 KB to 20 MB |
| `stress/upload.tst.c` | Multipart upload size, 8 KB to 20 MB; the over-limit case from depth 3 |
| `stress/sessions.tst.c` | Logins past the session limit, 25 to 6400 |
| `stress/huge.tst.c` | Runs at depth 5 and above: a 5 GB transfer, generated on first use |
| `r/mem.tst.c`, `r/buf.tst.c` | Allocation counts |

## The Soak and Stress Groups

Both start their own `ioto-unit` from a state directory built under the group (`test/soak/state`, `test/stress/state`, gitignored), on ports no other group uses: 4470 and 4471 for the soak, 4480 and 4481 for stress. The agent's configuration is `utils/agent/ioto.json5` and `utils/agent/web.json5`; the fixture actions it serves are in `apps/unit/src/unit.c`. The agent's log is `ioto.log` in the group directory, kept after the run.

The soak samples the agent's resident set, open descriptors, database size and live sessions either side of a mixed workload of eleven arms, and asserts bounded growth. The figures are printed every run whatever the verdict; a pass that reports nothing is not evidence. It sees the operating system's view of the process only and cannot see inside the allocator; `web/leak` records the same limit from the other side for the web module alone.

## Test Configuration

[testme.json5](testme.json5) carries the compiler flags, library list, environment and execution settings every group inherits. A group with its own server carries its own `testme.json5` naming the prep, setup and cleanup services and the healthcheck URL.

## Test File Naming

- `.tst.c` - C tests, compiled and linked against `libioto.a`
- `.tst.sh` - Shell tests

## Prerequisites

- The agent built with `make`, which also builds `ioto-unit` and the `web`, `url` and `json` commands the groups use
- TestMe: `bun install -g @embedthis/testme`
- `bin/prep-test.sh` reports anything else that is missing

## See Also

- [Feature plan for the soak and stress groups](../doc/features/scenario-tests/feature.md)
- [Development procedures](../doc/operations/development-procedures.md)
- [Ioto Agent Documentation](../README.md)
