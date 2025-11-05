# AI Documentation Directory

This directory contains structured documentation to assist Claude Code and AI agents in understanding and working with the Ioto Device Agent project.

## Important Caveat

**These documents are AI generated and are not guaranteed to be accurate or complete.**

They are provided to give AI context and a starting point for understanding the project. Use the official product documentation for the most accurate information.

Official documentation is available at: https://www.embedthis.com/doc/

## Directory Structure

### Active Documentation

- **agents/** - Claude sub-agent definitions for specialized tasks
- **skills/** - Claude skill definitions for reusable capabilities
- **prompts/** - Reusable prompts for common development tasks
- **workflows/** - Development workflows and procedures
- **commands/** - Custom slash commands for Claude Code

### Design & Planning

- **designs/** - Architectural and design documentation
  - `DESIGN.md` - Overview design document with index to other designs
  - Component-specific design documents

- **plans/** - Implementation plans and roadmaps
  - `PLAN.md` - Overview plan document with index to other plans
  - Component-specific plan documents

- **procedures/** - Testing and development procedures
  - `PROCEDURE.md` - Overview procedure document with index
  - Component-specific procedure documents

### Context & History

- **context/** - Current status and progress documentation
  - `CONTEXT.md` - Current project state and saved progress
  - Module-specific context files

- **logs/** - Change tracking and session history
  - `CHANGELOG.md` - Accumulated change log suitable for public release
  - `SESSION-YYYY-MM-DD.md` - Daily activity logs

- **references/** - External documentation and resources
  - `REFERENCES.md` - Links to external sites, documentation, and code

- **releases/** - Version release notes
  - `X.Y.Z.md` - Detailed release notes for each version
  - Comprehensive documentation of features, fixes, and changes

- **archive/** - Historical documentation (same structure as above)
  - Designs, plans, procedures, logs, and references from previous versions

## Documentation Guidelines

### For AI Agents

1. **Start with Overview Documents**: Read `DESIGN.md`, `PLAN.md`, and `CONTEXT.md` first
2. **Use Index Files**: Overview documents contain indices to component-specific docs
3. **Check Timestamps**: Documentation may be outdated; verify against actual code
4. **Update After Changes**: Maintain documentation when making significant changes

### For Developers

1. **Verify Information**: Always cross-reference with official documentation
2. **Context for AI**: This documentation helps AI understand project structure and conventions
3. **Not a Replacement**: These files supplement, but do not replace, official documentation
4. **Contribution**: Keep documents updated when making significant architectural changes

## Key Documentation Files

- [designs/DESIGN.md](designs/DESIGN.md) - System architecture and design overview
- [plans/PLAN.md](plans/PLAN.md) - Current implementation plan and roadmap
- [context/CONTEXT.md](context/CONTEXT.md) - Current project state and progress
- [logs/CHANGELOG.md](logs/CHANGELOG.md) - Public change log
- [releases/](releases/) - Version release notes (e.g., 2.8.1.md, 2.8.0.md)
- [references/REFERENCES.md](references/REFERENCES.md) - External resources

## Maintenance

- Documentation should be updated when:
  - Significant architectural changes are made
  - New features or modules are added
  - Development workflows change
  - Important decisions or tradeoffs are made

- Session logs should be created for:
  - Extended development sessions
  - Major feature implementations
  - Debugging complex issues
  - Planning sessions

## Related Documentation

- **Project Root**: `/Users/mob/c/agent/CLAUDE.md` - Project-specific instructions
- **Parent Project**: `/Users/mob/c/CLAUDE.md` - Overall architecture and conventions
- **Module Docs**: Each `src/*/` module has its own `CLAUDE.md`
- **API Reference**: Generated via `make doc` → `doc/index.html`

## Usage with Claude Code

This documentation structure follows Claude Code best practices:

1. **Hierarchical Organization**: Overview docs with indices to detailed docs
2. **Clear Separation**: Designs, plans, procedures, and context are distinct
3. **Historical Record**: Archive preserves previous iterations
4. **Active Context**: CONTEXT.md tracks current state and progress
5. **Change Tracking**: CHANGELOG.md and session logs maintain history

For more information about using Claude Code with this project, see the root [CLAUDE.md](../CLAUDE.md) file.
