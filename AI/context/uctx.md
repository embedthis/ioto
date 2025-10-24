# User Context and Threading Utilities (UCTX)

High-performance, cross-platform C library providing user-space context switching and coroutine primitives for cooperative multitasking systems. This module forms the foundation for fiber coroutines used throughout the Ioto Agent ecosystem.

## Uctx-Specific Build Configuration

Beyond the standard build commands documented in the parent `CLAUDE.md`, this module supports:

```bash
# Architecture-specific builds (overrides auto-detection)
ARCH=x64 make              # Force x64 architecture
ARCH=arm64 make            # Force ARM64 architecture
```

## Core Architecture

### Source Files
- **uctx.h/uctx.c** - Main API providing context switching functions
- **uctx-os.h** - Operating system and architecture detection
- **uctx-defs.h** - Platform-specific definitions
- **uctxAssembly.S** - Assembly language implementations for context switching

### Supported Architectures
- **x86/x64** - Intel/AMD 32-bit and 64-bit
- **ARM/ARM64** - ARM 32-bit and 64-bit (AArch64)
- **MIPS/MIPS64** - MIPS 32-bit and 64-bit
- **RISC-V/RISC-V64** - RISC-V 32-bit and 64-bit
- **PowerPC/PowerPC64** - IBM PowerPC 32-bit and 64-bit
- **Xtensa** - ESP32 and other Xtensa processors
- **Windows** - Windows Fibers API
- **FreeRTOS** - Real-time operating system support
- **Pthreads** - Fallback implementation using POSIX threads

### Architecture Directory Structure
Each architecture has its own subdirectory in `arch/` containing:
- `bits.h` - Register definitions and context structure
- `defs.h` - Architecture-specific definitions (optional)
- `*.c` - C implementation files
- `*.S` - Assembly implementation files

## API Functions

```c
int uctx_getcontext(uctx_t *ucp);           // Save current context
int uctx_setcontext(uctx_t *ucp);           // Restore context
int uctx_swapcontext(uctx_t *from, uctx_t *to); // Atomic context switch
int uctx_makecontext(uctx_t *ucp, void (*fn)(void), int argc, ...); // Create new context
void uctx_setstack(uctx_t *up, void *stack, size_t stackSize); // Configure stack
void uctx_freecontext(uctx_t *ucp);         // Free context resources
```

## Distribution System

Creates amalgamated source files for easy distribution:
- **uctx.h** - Combined header with inline architecture-specific bits
- **uctxLib.c** - Combined C implementation with inline architecture code
- **uctxAssembly.S** - Combined assembly with inline architecture code

## Adding New Architecture Support

1. Create new directory in `arch/` with architecture name
2. Implement required files:
   - `bits.h` - Define `uctx_t` structure and register layout
   - Context switching functions in C or assembly
3. Update `uctx-os.h` with new architecture constant
4. Update architecture detection logic

## Platform-Specific Notes

### VxWorks
- Fully supported through existing architecture implementations
- No VxWorks-specific implementation required
- Uses native CPU architecture detection:
  - VxWorks on ARM/ARM64 → Uses `arch/arm` or `arch/arm64`
  - VxWorks on x86/x64 → Uses `arch/x86` or `arch/x64`
  - VxWorks on PowerPC → Uses `arch/ppc` or `arch/ppc64`
  - VxWorks on MIPS → Uses `arch/mips` or `arch/mips64`
- Assembly implementations are OS-agnostic and work correctly on VxWorks
- VxWorks-specific OS concerns (task management, semaphores) are handled by the application layer (Safe Runtime)
- Tested on VxWorks 6.x and 7.x

### ESP32/FreeRTOS
- Uses Xtensa architecture implementation based on pthreads
- Special handling for FreeRTOS task stacks
- Limited to single-core context switching

### Windows
- Uses native Windows Fibers API
- Automatic stack management
- Lazy thread-to-fiber conversion
- See `doc/DESIGN.md` for detailed implementation notes

### Embedded Systems
- Minimal memory footprint
- No heap allocations in core switching code
- Stack management delegated to caller

## Module-Specific Considerations

- **NULL Intolerance**: Unlike other modules in this codebase, uctx functions are NOT NULL tolerant. Passing NULL to any function will result in undefined behavior and crashes.
- **Stack Management**: Caller is responsible for providing and managing stack memory for contexts.
- **Single-threaded Design**: Aligns with the parent architecture's single-threaded execution model with fiber coroutines.

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

## Important Notes
- Ignore source in the dist directory
