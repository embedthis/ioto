#!/usr/bin/env bash
#
#   prod-profile.tst.sh - A plain build is the release configuration an integrator ships
#
#   The build configuration is the OPTIMIZE make variable and nothing else: one libioto.a serves
#   every app, so no app's ioto.json5 profile can select it. An "optimize" key in ioto.json5 once
#   claimed to, was read by nothing, and every default build path built debug. Check the defaults
#   the Makefile, make.bat and the promote build actually use, and that the unread key is gone.
#
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/prod-profile.XXXXXX")
trap 'rm -fr "$WORK"' EXIT
status=0

fail() {
    echo "✗ $*"
    status=1
}

#   Evaluate the Makefile's CONFIG as a plain "make" would, free of the caller's make variables.
#   The Makefile refuses hosts it has no gmake2 projects for; Windows builds with make.bat.
case "$(uname -s)" in
Darwin|Linux|FreeBSD)
    cp "$ROOT/Makefile" "$WORK/Makefile"
    printf 'print-config:\n\t@echo $(CONFIG)\n' > "$WORK/print.mk"
    config=$(cd "$WORK" && env -u OPTIMIZE -u CONFIG -u MAKEFLAGS -u MFLAGS -u MAKELEVEL \
        make -s --no-print-directory -f Makefile -f print.mk print-config 2>/dev/null)
    case "$config" in
    release_*) echo "✓ make builds $config by default" ;;
    *)         fail "make builds \"$config\" by default, not release" ;;
    esac
    ;;
*)
    echo "✓ make: skipped, the Makefile does not build on $(uname -s)"
    ;;
esac

if [ -f "$ROOT/make.bat" ] ; then
    first=$(grep -m1 -i '^set "CONFIG=' "$ROOT/make.bat")
    case "$first" in
    *'CONFIG=release"'*) echo "✓ make.bat builds release by default" ;;
    *)                   fail "make.bat default is $first, not release" ;;
    esac
fi

if [ -f "$ROOT/.local.mk" ] ; then
    if grep -A2 '^release-build:' "$ROOT/.local.mk" | grep -q 'OPTIMIZE=release' ; then
        echo "✓ release-build selects release explicitly"
    else
        fail ".local.mk release-build does not pass OPTIMIZE=release"
    fi
fi

for file in "$ROOT"/apps/*/ioto.json5 ; do
    if grep -nE '^[[:space:]]*optimize[[:space:]]*:' "$file" ; then
        fail "${file#"$ROOT"/} carries an optimize key, which the build never reads"
    fi
done

[ $status = 0 ] && echo "✓ no app configuration claims to select the build"
exit $status
