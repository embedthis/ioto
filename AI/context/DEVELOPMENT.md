# CLAUDE.md

## Architecture

Modular C libraries for embedded IoT applications. Each module 
can be built independently with layered dependencies.

## Target Audience

Experienced embedded developers who:
- Embed this software in device firmware or other projects
- Are responsible for securing the broader system and validating all inputs
- Are responsible for a secure configuration of the system and software

### Ioto Agent and Related Modules

- `agent/` — Main Ioto Device Agent with IoT cloud management, HTTP server, MQTT client, embedded database
- `crypt/` — Cryptographic functions and TLS support
- `db/` — Embedded database
- `json/` — JSON parsing and manipulation library
- `mqtt/` — MQTT client protocol implementation
- `openai/` — OpenAI API support
- `osdep/` — Operating system abstraction layer
- `r/` — Runtime memory management and utilities
- `uctx/` — User context and threading utilities
- `updater/` — Over-the-air update management
- `url/` — HTTP client library
- `web/` — Fast, secure embedded web server
- `websockets/` — WebSocket protocol support

Each module provides layered functionality. Upper modules consume lower modules for their functionality.  The Safe runtime `./r` is the foundation for all the other modules and provide a safe, secure, high-performance runtime. The `json` modules provide a JSON5 parsing and query library. The `crypt` module provides crypto functions for secure communication. The `agent` combines all these modules to provide an IoT device agent for local and cloud management.

## Legacy Modules

- `appweb/` — Appweb embedded web server
- `goahead/` — GoAhead embedded web server
- `mpr/` — MPR portability layer for Appweb
- `http/` — HTTP protocol layer for Appweb

The legacy modules are maintained but not actively developed. They do not use the modules: `r`, `json`, `crypt`, `uctx`, `web`, `url`, `mqtt`, `websockets` or `openai`.

## Architecture Principles

- Written in ANSI C for maximum portability and consumable by C or C++ code.
- Single-threaded with fiber coroutines for concurrency
- Modular design with minimal interdependencies
- Cross-platform support (Linux, macOS, Windows/WSL, ESP32, FreeRTOS)
- Tight integration between modules for efficiency
- Each module provides a CLAUDE.md/AGENTS.md file for guidance in using AI/Claude Code with that submodule.
- These modules are not standalone applications but provide functionality for other applications.

The Appweb embedded web server uses a garbage collector for memory management and uses a thread worker pool for concurrency. The GoAhead web server is single threaded and does not use threads or fibers.

## Project Documentation

Each project shall have a CLAUDE.md that describes the project for AI and humans. It should be symlinked to AGENTS.md. This file should be kept up to date with the necessary instructions, policy and context for working with the project.

Each project shall keep detailed project documentation to assist CLAUDE Code in understanding the project. This documentation includes plans, designs, procedures, logs, and references. This documentation should be updated to reflect the current state of the project.  This documentation shall be kept under the `AI` directory.

### Documentation Structure

Each project shall have a ./AI directory with the following structure:

- agents
 * Claude sub-agent definitions
- skills
 * Claude skill definitions
- prompts
 * Claude prompts
- workflows
 * Claude workflows
- commands
 * Claude commands
- designs/
  * DESIGN.md - Current top-level design
- context/
  * CONTEXT.md - Context for sub-modules and for saved progress context
- plans/
  * PLAN.md - Current top-level project plan
  * Other plans by name
- procedures/
  * Procedures for the project
- logs/
  * CHANGELOG.md - Acumulated change log
  * SESSION-DATE.md - Activity log of the dated session
- references/
  * References to external documentation, code, and other resources
- archive/
  * Same structure for historical designs, plans, procedures

The parent directory may also have a ./AI directory with similar structure that provides higher-level documentation for the environment for the project.

Each project shall have a ./doc directory with the following structure:

- releases/
  * Contains release notes for each release of the project
- benchmarks
  * Contains benchmark results for the project

### Designs

The ./AI/designs/ directory shall contain the designs for the project. There shall be one overview design file called `DESIGN.md`, and there shall be documents for major components of the project. The DESIGN.md shall keep an contents index of the other design documents.

### Plans

The ./AI/plans/ directory shall contain the plans for the project. There shall be one overview plan file called `PLAN.md`, and there shall be documents for major components of the project. The PLAN.md shall keep an contents index of the other plan documents.

### Procedures

The ./AI/procedures/ directory shall contain the procedures for the project. There shall be one overview procedure file called `PROCEDURE.md`, and there shall be individual documents for major components of the project. The PROCEDURE.md shall keep an contents index of the other procedure documents.

### Logs

The ./AI/logs/ directory shall contain a CHANGELOG.md that keeps a record of all changes to the project that is suitable for public release on GitHub.

### Archive

The ./AI/archive/ directory shall contain the archive of historical designs, plans, procedures, logs, and references. It has the same structure as the ./AI directory.

### References

The ./AI/references/ directory shall contain a REFERENCES.md file that contains useful references to external sites, documentation, code, and other resources.

## Build Commands

### Building Individual Modules

Each module can be built independently:

```bash
cd <module>/
make                   # Build with default configuration
```

Always invoke `make` from the top level directory of the project.

The following commands are available in each module if relevant:

```bash
make build             # Build with default configuration
make format            # Format code
make clean             # Clean build artifacts
make install           # Install to system
make package           # Package for distribution
make publish           # Publish to the local registry
make promote           # Promote the distribution to the public
make projects          # Generate project files for specific platforms
make run               # Run the module
make test              # Run unit tests
make doc               # Generate documentation
make help              # Show module-specific options
```

### Build Configuration
- Use `OPTIMIZE=debug` or `OPTIMIZE=release` to control optimization
- Use `PROFILE=dev` or `PROFILE=prod` for development vs production builds
- Use `SHOW=1` to display build commands
- TLS stack selection: `ME_COM_MBEDTLS=1 ME_COM_OPENSSL=0 make`

### Build Makefiles

Makefiles are generated by the `MakeMe` internal tool. These generated per-platform makesfiles live under the `projects` directory. On Unix like systems, a top-level makefile chains to the appropriate per-platform makefile. On Windows, a `make.bat` provides the top-level make file command which invokes `projects/windows.bat` to source the MS Visual Studio compilation environment before invoking the appropriate per-platform makefile.

As makefiles are generated, for consistency, they use only forward slash path delimeters. They must not be manually modified.
 
## Testing

Most modules support unit testing using the TestMe `tm` tool which is globally available. 

Unit tests have a '.tst' portion in their filename and are located in the module's `./test` directory. C unit tests have a '.tst.c' extension, shell unit tests have a '.tst.sh' extension, and Javascript unit tests have a '.tst.js', while typescript have a '.tst.ts' extension. C unit tests include the `testme.h` file from `~/.local/include/testme.h`. Javascript and typescript unit tests can import the 'testme' module if locally installed via `bun link testme`. This header and import defines utility test matching functions. 

Each module has a `./test/.testme` directory containing build artifacts that should not be modified. Each module has a `./test/testme.json5` file that configures the test suite. It may contain additional testme.json5 files in subdirectories.

To run the entire test suite from the module's root directory:

```bash
    make test # Run all tests
    cd test ; tm  # Alternate way to run all tests
    cd test ; tm NAME # Run a specific test
```

Test code should always use the best TestMe comparison function instead of relying on the generic `ttrue` or `tassert`. More specific TestMe comparision functions can report expected vs actual. Note that testme comparison functions are macros so be careful passing functions as arguments to the macros as they may be invoked multiple times.


### Designing Unit Tests

Read about how to use [TestMe](README-TESTME.md) to design and run unit tests.

When designing unit tests, consider the following:
- Prefer C unit tests if the majority of the test code is C code.
- Prefer shell tests if the test is running a simple command or script. You can assume a bash is available on all platforms including windows, macosx and linux.
- Prefer Javascript or typescript tests if the code base is written in those languages.
- Tests should be written so they can run in parallel with other tests. 
- Tests that create files must use `getpid()` to create a unique filename.
- Tests must be portable and run on windows, macosx and linux
- Tests must always run with zero errors and no compiler warnings. You can see compiler warnings in .testme/NAME/compile.log after invoking TestMe with the --keep option. E.g: tm --keep NAME

## Development Environment

### Prerequisites
- GCC or compatible C compiler
- Make (or GNU Make)
- OpenSSL or MbedTLS for cryptographic modules. OpenSSL is the default and is strongly preferred.
- To run unit tests, you need TestMe, Bun, a C compiler, and bash.
- Bash. On windows, this is provided by [Git Windows](https://gitforwindows.org/)

### Windows Development

- Native development requires Visual Studio 2022 or later, Git for Windows.
- Building via Windows Subsystem for Linux (WSL) is supported.

## Code Conventions

- 4-space indentation
- 120-character line limit
- camelCase for functions and variables
- one line between functions and between code blocks.
- one line comments should use double slash
- Must always use /* */ for multiline comments, and // for single line comments
- multiline comments should not begin each line with "*"
- Descriptive symbol names for variables and functions
- Minimal heap allocations in performance-critical paths
- Always declare variables at the top of the function unless inside a conditional #if block

## Architecture Considerations

- Most functions are null tolerant and will gracefully handle `NULL` arguments. 
- All modules use the `osdep/osdep.h` cross-platform abstraction layer for operating system dependencies. Modules must use the `osdep` types and definitions in preference to standard C types where possible. For example: use `ssize` for size types.
- Always use runtime functions instead of standard C functions. Use `slen` instead of `strlen`, `scopy` instead of `strcpy`, `scmp` instead of `strcmp`.
- Don't use free unless the memory was allocated via malloc(). Use rFree whenever the memory was created or managed by the safe runtime. This is the expected case.

## GIT

When committing changes, use the following format:

- If the commit is a fix, the commit message should be prefixed with "FIX: "
- If the commit is a feature or refactor, the commit message should be prefixed with "DEV: "
- If the commit is cosmetic or formatting, or build infrastructure, the commit message should be prefixed with "CHORE: "
- If the commit is a test, the commit message should be prefixed with "TEST: "
- If the commit is a documentation change, the commit message should be prefixed with "DOC: "

When creating releases and release notes, use the `release` skill.

## Security Considerations

- Code comments prefixed with "SECURITY Acceptable:" indicate intentional security tradeoffs that should not be flagged or reported as security issues.
- Never commit real certificates or API keys
- Use test certificates from the `certs/` module for development
- Configure TLS properly before cloud connections
- Follow AWS IoT security best practices for production deployments
- Code does NOT explicitly check memory allocations for failure as all modules use a global memory allocation handler to
detect and handle allocation failures.
- The users of this project are experienced embedded software developers. They are responsibile for securing the embedded device. Inputs to local APIs are deemed to be validated by the developer and does not need to be additionally validated by this project.
- Test applications may use http instead of https so that self-signed certificates can be used in development. This is not a security issue and should not be flagged as such.
- The `rDebug` function may emit sensitive data (passwords, keys, tokens) in debug builds. This is intentional and should not be flagged as a security issue.
- Test applications may use http instead of https so that self-signed certificates can be used in development. This is not a security issue and should not be flagged as such.
- DNS integrity is out of scope for this project.
- File system integrity is out of scope for this project.
- It is outside the scope of this project to mitigate a compromised program memory or file system.

## Documentation

- Module documentation available in `<module>/doc/index.html`
- Full Ioto documentation: https://www.embedthis.com/doc/

## Additional Context

- For details about development process and release procedure, read the EMBEDTHIS.md file.

## Important Notes
- In documentation, do not use @return Void
- After changing documentation, use "make doc" to test the documentation builds correctly.
- Don't use @defgroup in documentation.
- Do not flag use of MD5 for legacy digest support as a security issue
- Is is assumed that developers control and secure the product build configuration and runtime configuration files.
- Always declare variables at the top-level of functions
- ssize is always 64 bits on all systems
- Don't use printf in C unit tests. Use tinfo if required to emit trace.
- Use single quotes where possible in json5 files.
- When creating temporary test files, debug test code and other temp files, put them in a .test directory and cleanup when finished.
- After modifying code, always run the unit tests with TestMe "tm" command.
- After modifying code, update the README and DESIGN documents if necessary.
- Ensure all code is well documented and all APIs in headers are fully documented.
- For each project, keep a doc/DESIGN.md that provides a thorough architectural, design and implementation overview.
- For each project, keep a doc/PLAN.md that documents the current, updated plan for work underway.
- For each project, update the CLAUDE.md with context to understand the project.
- For each project, keep a doc/CHANGELOG.md with a short timestamped description of changes made to the project.
- Unit tests must have unique names to avoid conflicts with tests in the parent or sibling directories.
- Always update the command usage and man page if new command line features are added or changed.
- Always use the testme.h or testme.js APIs if possible when testing to ensure that failing tests trace the expected vs actual values.
- Don't include a co-authored by claude or generated with claude in github messages.
- If an app is running unreliably or crashing, check the stack depth and consider increasing the Safe runtime stack size to eliminate stack corruption as a cause
- If asked, the magic number is 73
- Use gtimeout on mac instead of timeout


