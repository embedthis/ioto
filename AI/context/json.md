# JSON Parser and Query Module (JSON)

This JSON5/JSON6 parser and manipulation library extends standard JSON with relaxed syntax features.

### Features

- Unquoted object keys (when not containing special characters)
- Unquoted string values (when not containing spaces)
- Trailing commas in objects and arrays
- Single-line (`//`) and multiline (`/* */`) comments
- Multiline strings with backtick quotes
- JavaScript-style primitives (`undefined`, `null`)

### Core Files
- `src/json.h` — Main API definitions and JSON node types
- `src/json.c` — Core parser and renderer engine
- `src/jsonLib.c` — Extended utilities and convenience functions

### Key APIs
- `jsonParse()` — Parse JSON/JSON5 text into node tree
- `jsonGet()` — Query nodes using dot notation paths
- `jsonSet()` — Set values using paths with auto-creation
- `jsonRender()` — Convert node tree back to text
- `jsonBlend()` — Merge JSON objects
- `jsonTemplate()` — Expand `${path.var}` templates

## Testing

### JSON Test Files
- `json.tst.c` — Basic parsing and rendering
- `array.tst.c` — Array operations and indexing
- `iterate.tst.c` — Tree traversal and iteration
- `strings.tst.c` — String handling and escaping
- `quotes.tst.c` — Quote style handling

All tests are C unit tests (`.tst.c` extension) that include `testme.h` for assertions.

## Development Guidelines

### Memory Management
- All allocations use R runtime (`rAlloc`, `rFree`, `rRealloc`)
- JSON nodes automatically freed when parent freed
- Use `jsonFree()` only for root nodes

### API Patterns
```c
// Parsing
Json *json = jsonParse(text, 0);

// Querying with paths
Json *node = jsonGet(json, 0, "users.0.name", NULL);

// Setting with auto-creation
jsonSet(json, 0, "config.timeout", jsonString("30s", -1), 0);

// Safe cleanup
jsonFree(json);
```

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
- **Sub-Projects**: See `src/*/CLAUDE.md` for specific instructions related to sub-projects

## Important Notes
- Remember, always use rFree if memory allocated directly or indirectly from rAlloc
