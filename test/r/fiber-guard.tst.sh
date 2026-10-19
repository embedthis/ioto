#!/usr/bin/env bash
#   Inline fiber stacks must detect overflow in release builds without guard pages. Exercise overflowing and
#   valid stacks under the no-MMU configuration.
here=$(cd "$(dirname "$0")" && pwd)

#   The pad guards stacks the runtime allocates. Windows fibers allocate their own stacks.
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
    echo "Skipped: Windows fibers allocate their own stacks"
    exit 0
    ;;
esac

if [ -f "$here/../src/r.c" ]; then
    inc="-I$here/../src -I$here/../src/osdep -I$here/../src/uctx"
    files=""
    for f in buf event fiber fs hash list log mem printf r run socket string thread time unix wait ; do
        files="$files $here/../src/$f.c"
    done
    files="$files $here/../src/uctx/uctxLib.c $here/../src/uctx/uctxAssembly.S"
else
    #   Vendored into the agent, which builds the runtime from its amalgamation
    inc="-I$here/../../include"
    files="$here/../../lib/rLib.c $here/../../lib/uctxLib.c $here/../../lib/uctxAssembly.S"
fi
work="${TMPDIR:-/tmp}/r-fiber-guard.$$"
cc="${CC:-cc}"

fail() {
    echo "✗ $1"
    rm -rf "$work"
    exit 1
}

mkdir -p "$work"
base="-w -g -DR_USE_CONFIG=1 -DME_COM_SSL=0 -DME_COM_OPENSSL=0 -DME_COM_MBEDTLS=0"

#   Value of a configuration macro for a set of defines
macro() {
    printf '#include "r.h"\n#if %s\nVALUE 1\n#else\nVALUE 0\n#endif\n' "$1" > "$work/probe.c"
    "$cc" -E $base $inc $2 "$work/probe.c" 2>/dev/null | sed -n 's/^ *VALUE \(.*\)$/\1/p'
}

expect() {
    got=$(macro "$1" "$2" | tr -d ' ()')
    [ "$got" = "$3" ] || fail "$1 is '$got' with '$2', expected '$3'"
}

#   The pad exists on inline stacks in release and debug, and not where guard pages protect the stack
expect ME_FIBER_GUARD_PAD "-DME_FIBER_VM_STACK=0 -DME_DEBUG=0" 1
expect ME_FIBER_GUARD_PAD "-DME_FIBER_VM_STACK=0 -DME_DEBUG=1" 1
expect ME_FIBER_GUARD_PAD "-DME_FIBER_VM_STACK=1 -DME_DEBUG=0" 0
expect ME_FIBER_GUARD_PAD "-DME_FIBER_VM_STACK=1 -DME_DEBUG=1" 0

#   Stack usage measurement walks the stack on every allocation, so it stays a debug-only aid
expect ME_FIBER_STACK_USAGE "-DME_FIBER_VM_STACK=0 -DME_DEBUG=0" 0
expect ME_FIBER_STACK_USAGE "-DME_FIBER_VM_STACK=0 -DME_DEBUG=1" 1

cat > "$work/main.c" <<'EOT'
#include "r.h"

static int mode;

static void handler(int cause, size_t size)
{
    (void) size;
    _exit(cause == R_MEM_STACK ? 7 : 8);
}

//  Use a good part of the stack and return
static int recurse(int depth)
{
    volatile char pad[256];

    pad[0] = (char) depth;
    return depth ? recurse(depth - 1) + pad[0] : pad[0];
}

static void fiberMain(void *arg)
{
    char *bottom;

    (void) arg;
    if (mode == 1) {
        //  Run off the end of the stack into the pad below it
        bottom = (char*) rGetFiberStack() - rGetFiberStackSize();
        memset(bottom - 4, 0, 4);
    } else {
        recurse(100);
    }
    //  The pad is checked when the fiber stops running
    rSleep(10);
    rStop();
}

int main(int argc, char **argv)
{
    mode = argc > 1 ? atoi(argv[1]) : 0;
    rSetMemHandler(handler);
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}
EOT

"$cc" $base -DME_FIBER_VM_STACK=0 -DME_DEBUG=0 $inc -o "$work/guard" "$work/main.c" $files -lm -lpthread 2>"$work/build.log" \
    || fail "cannot build the runtime with inline fiber stacks: $(head -5 "$work/build.log")"

"$work/guard" 0 >/dev/null 2>&1
rc=$?
[ $rc -eq 0 ] || fail "a fiber within its stack exited with status $rc"

"$work/guard" 1 >/dev/null 2>&1
rc=$?
[ $rc -eq 7 ] || fail "a fiber that overflowed its stack was not reported (status $rc, expected 7)"

rm -rf "$work"
echo "✓ Fiber stack overflow is detected without an MMU in release builds"
exit 0
