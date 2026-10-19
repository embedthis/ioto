#!/usr/bin/env bash
#   Check hash callback types with clang -fsanitize=function, first proving a mismatched call is diagnosed.
#   Requires supporting clang >=17; use SANITIZE_CC or CC to select it.
here=$(cd "$(dirname "$0")" && pwd)
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
work="${TMPDIR:-/tmp}/r-hash-fntype.$$"
sanitize="-fsanitize=function -fno-sanitize-recover=function"

#   Windows has no separate libm or pthread library, and the runtime needs the platform libraries instead
case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*)
    libs="-lws2_32 -ladvapi32 -luser32 -lkernel32 -lshell32"
    ;;
*)
    libs="-lm -lpthread"
    ;;
esac

fail() {
    echo "✗ $1"
    rm -rf "$work"
    exit 1
}

mkdir -p "$work"

cat > "$work/probe.c" <<'EOT'
typedef int (*Proc)(const void *arg);

static int target(const char *arg)
{
    return arg != 0;
}

int main(void)
{
    Proc proc = (Proc) target;
    return proc("x") ? 0 : 1;
}
EOT

#   The first compiler that reports the probe's mismatched call
cc=""
for candidate in "$SANITIZE_CC" "$CC" clang /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang ; do
    [ -n "$candidate" ] || continue
    "$candidate" -w $sanitize -o "$work/probe" "$work/probe.c" >/dev/null 2>&1 || continue
    if ! ( "$work/probe"; exit $? ) >/dev/null 2>&1 ; then
        cc="$candidate"
        break
    fi
done
if [ -z "$cc" ] ; then
    rm -rf "$work"
    echo "Skipped: no compiler implements -fsanitize=function for C (set SANITIZE_CC to clang 17 or later)"
    exit 0
fi

cat > "$work/main.c" <<'EOT'
#include "r.h"

int main(void)
{
    RHash *hash, *caseless;
    int   ok;

    hash = rAllocHash(0, 0);
    rAddName(hash, "Alpha", "1", 0);
    caseless = rAllocHash(0, R_HASH_CASELESS | R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(caseless, "Alpha", "1", 0);
    ok = rLookupName(hash, "Alpha") && !rLookupName(hash, "ALPHA") && rLookupName(caseless, "ALPHA");
    rFreeHash(hash);
    rFreeHash(caseless);
    return ok ? 0 : 1;
}
EOT

base="-w -g -DR_USE_CONFIG=1 -DME_COM_SSL=0 -DME_COM_OPENSSL=0 -DME_COM_MBEDTLS=0"
"$cc" $base $sanitize $inc -o "$work/hash" "$work/main.c" $files $libs 2>"$work/build.log" \
    || fail "cannot build the runtime with $cc $sanitize: $(head -5 "$work/build.log")"

( "$work/hash"; exit $? ) >"$work/run.log" 2>&1
rc=$?
[ $rc -eq 0 ] || fail "hash lookup failed (status $rc): $(grep -m1 'runtime error' "$work/run.log")"

rm -rf "$work"
echo "✓ Hash functions are called through their own type ($cc)"
exit 0
