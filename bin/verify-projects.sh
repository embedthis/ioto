#!/bin/bash
#
#   verify-projects.sh -- Prove the committed makefiles are what the generator produces
#
#   Regenerates projects/gmake2 and apps/<app>/projects/gmake2 into a scratch tree and diffs them
#   against the committed files. Any difference is a failure: either someone edited a generated file
#   by hand, or someone changed a premake5.lua and did not regenerate.
#
#   Hand edits to a generated file survive until the next regeneration and are then silently
#   reverted. The house build output (the [CC] and [Link] tags) used to be applied after generation
#   by ~/bin/fixmake, which made the committed files impossible to reproduce without that tool; it is
#   now projects/house-style.lua, applied during generation, and this script is what proves it.
#
#   Scope is gmake2, which is what every Unix build uses. The vs2022 and xcode outputs are generated
#   by the same premake5.lua files and are not checked here: no host in this project builds them from
#   a check, so a diff would report generator-version drift that nobody can act on.
#
#   Usage: bin/verify-projects.sh
#

set -e
unset CDPATH

TOP=$(cd "$(dirname "$0")/.." && pwd)
APPS="ai blank blink http unit"

if ! command -v premake5 >/dev/null 2>&1 ; then
    echo "      [Skip] premake5 not installed - cannot verify the generated makefiles" >&2
    exit 0
fi

SCRATCH=$(mktemp -d "${TMPDIR:-/tmp}/ioto-projects.XXXXXX")
trap 'rm -rf "$SCRATCH"' EXIT

#
#   The generators read the version from ../package.json and dofile the shared lua under projects/,
#   so the scratch tree needs both.
#
mkdir -p "$SCRATCH/projects"
cp "$TOP/package.json" "$SCRATCH/package.json"
cp "$TOP"/projects/*.lua "$SCRATCH/projects/"

status=0

verify() {
    local name="$1" gendir="$2" committed="$3"

    (cd "$gendir" && premake5 gmake >/dev/null 2>&1) || {
        echo "      [Fail] premake5 could not generate from $name/premake5.lua" >&2
        status=1
        return
    }
    if diff -r -u "$gendir/gmake2" "$committed" > "$SCRATCH/diff" 2>&1 ; then
        return
    fi
    echo "      [Fail] $committed differs from what $name/premake5.lua generates" >&2
    echo "" >&2
    sed 's/^/               /' "$SCRATCH/diff" >&2
    echo "" >&2
    status=1
}

verify "projects" "$SCRATCH/projects" "$TOP/projects/gmake2"

for app in $APPS; do
    [ -f "$TOP/apps/$app/projects/premake5.lua" ] || continue
    mkdir -p "$SCRATCH/apps/$app/projects"
    cp "$TOP/apps/$app/projects/premake5.lua" "$SCRATCH/apps/$app/projects/"
    verify "apps/$app/projects" "$SCRATCH/apps/$app/projects" "$TOP/apps/$app/projects/gmake2"
done

if [ "$status" -ne 0 ] ; then
    echo "               Never edit a generated makefile. Change the premake5.lua and run:" >&2
    echo "                   make projects" >&2
    exit 1
fi

echo "      [Info] The generated makefiles match projects/premake5.lua"
