#!/usr/bin/env bash
#
#   app-relink.tst.sh - An app binary is relinked when libioto.a changes
#
#   The apps link libioto.a from another premake workspace, so their generated makefiles carry no
#   dependency on it. Without one, "make" rebuilt the library and left ioto-unit, ioto-http and
#   ioto-blank running the old code, and a test could pass against code no longer in the tree.
#
#   Nothing is built or touched. A dry run of the top-level "make apps" shows the link dependency
#   it hands each app's makefile; "make -n -W libioto.a" then asks that makefile what it would do
#   were the library newer, and every app must answer that it would relink. (-W is not passed to
#   sub-makes, so it cannot be asked of the top-level make directly.)
#
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT/build/bin"
status=0

if [ ! -f "$BIN/.config" ] || [ ! -f "$BIN/libioto.a" ] ; then
    echo "✓ app-relink: skipped, no build in build/bin"
    exit 0
fi
case "$(uname -s)" in
Darwin|Linux|FreeBSD) ;;
*) echo "✓ app-relink: skipped, no gmake2 build on $(uname -s)" ; exit 0 ;;
esac

#   Ask about the configuration build/bin actually holds
CONFIG=$(cat "$BIN/.config")
OPTIMIZE=${CONFIG%_*}

for app in blank http unit ; do
    if [ ! -f "$BIN/ioto-$app" ] ; then
        echo "✓ app-relink: skipped, ioto-$app is not built"
        exit 0
    fi
done

#   The link dependency the top-level Makefile gives the app makefiles, if any
top=$(env -u MAKEFLAGS -u MFLAGS -u MAKELEVEL -u LDDEPS \
    make -n --no-print-directory -C "$ROOT" OPTIMIZE="$OPTIMIZE" apps 2>&1)
deps=$(echo "$top" | sed -n 's/.* LDDEPS=\([^ ]*\).*/\1/p' | head -1)

appMake() {     # app [make args]
    local app=$1 ; shift
    env -u MAKEFLAGS -u MFLAGS -u MAKELEVEL -u LDDEPS \
        make -n --no-print-directory -C "$ROOT/apps/$app/projects/gmake2" -f "ioto-$app.make" \
        config="$CONFIG" ${deps:+LDDEPS="$deps"} "$@" 2>&1
}

for app in blank http unit ; do
    if appMake $app | grep -q "Link\] ioto-$app" ; then
        echo "✓ app-relink: skipped, ioto-$app is not up to date with $CONFIG"
        continue
    fi
    if appMake $app -W "$BIN/libioto.a" | grep -q "Link\] ioto-$app" ; then
        echo "✓ ioto-$app relinks when libioto.a changes"
    else
        echo "✗ ioto-$app is not relinked when libioto.a changes"
        status=1
    fi
done
exit $status
