# Embedded Database (DB)

This is the **DB** (Database) module from the EmbedThis Ioto suite - an embedded NoSQL document database written in ANSI C. It provides JSON document storage with flexible query APIs and efficient red/black tree indexing.

## Module-Specific Features

- High-performance NoSQL document database
- JSON5/JSON6 document storage and manipulation
- Red/black tree indexing for efficient queries
- Transaction journaling and recovery
- Schema validation and enforcement
- Time-based item expiration
- Result pagination for large datasets
- Optional cloud synchronization via triggers

## Key Source Files

- `src/db.h` — Main database API header with all public interfaces
- `src/dbLib.c` — Core database implementation (storage, indexing, query logic)
- `src/mains/db.c` — Command-line database tool for testing and administration

## Red-Black Tree (RbTree)

The db module includes a self-balancing red-black tree implementation for ordered collections and efficient indexing.

### Key Types
- `RbTree` — Tree container with comparison and free callbacks
- `RbNode` — Individual tree node with data pointer

### Key APIs
- `rbAlloc()` — Create a new red-black tree
- `rbFree()` — Free the tree and all nodes
- `rbInsert()` — Insert a node into the tree
- `rbRemove()` — Remove a node from the tree
- `rbLookup()` — Find a node by key
- `rbLookupFirst()` / `rbLookupNext()` — Iterate matching nodes
- `rbFirst()` / `rbNext()` — Iterate all nodes in order

### Iteration Macros
- `ITERATE_TREE(rbt, node)` — Traverse all nodes
- `ITERATE_INDEX(rbt, node, data, ctx)` — Traverse matching nodes

> **Note**: The RbTree implementation was moved from the r (Safe Runtime) module as of version 3.0.4.

## DB-Specific Build Configuration

Module-specific environment variables:
- `ME_R_LOGGING=1` — Enable application logging
- `ME_R_TRACING=1` — Enable debug tracing for database operations

Example: `ME_R_LOGGING=1 ME_COM_OPENSSL=1 make`

## Testing

### Test Files Structure

Test files are located in `test/` directory:
- `test/data.json` — Test data fixtures
- `test/schema.json` — Database schema for testing
- `test/test.db` — Test database file
- `test/test.db.jnl` — Test journal file

### Key Database Test Files

- `test/create-items.c.tst` — Item creation and insertion
- `test/find.c.tst` — Query and search functionality
- `test/update.c.tst` — Document update operations
- `test/remove.c.tst` — Item deletion
- `test/pagination.c.tst` — Result pagination
- `test/expire.c.tst` — Time-based expiration
- `test/fields.c.tst` — Field projection and selection
- `test/save.c.tst` — Database persistence
- `test/open.c.tst` — Database opening and initialization

## Database-Specific Development Notes

### API Usage Patterns
- Use R runtime memory functions (`rAlloc`, `rFree`, etc.) rather than stdlib
- All database functions follow null-tolerant design
- Most operations return status codes for error handling

### Memory Management

- Calls to `dbField()` return pointers to strings within the DbItem structure
- These pointers become invalid after item updates - always copy the data if persistence is needed
- Use R runtime string functions: `sclone()`, `slen()`, `scopy()`, `scmp()`

### Database Features

- Document-oriented NoSQL storage with JSON5 format
- Indexed queries using red/black trees for O(log n) performance
- Transaction journaling ensures durability and crash recovery
- Schema validation enforces document structure constraints
- Time-based expiration automatically removes stale items

### Performance Considerations

- Red/black tree indexes provide efficient range queries and sorting
- Minimize heap allocations in query-intensive code paths
- Use field projection to reduce data transfer for large documents
- Pagination prevents memory exhaustion with large result sets

## TODO Items

- Add unit tests for journal recovery when database closes without explicit save
- Enhance query optimization for complex multi-field indexes

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
