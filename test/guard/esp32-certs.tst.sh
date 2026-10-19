#!/usr/bin/env bash
#   ESP32 dev builds may install the published test key; other profiles require a provisioned device pair.
#   Exercise install-certs in a scratch project without ESP-IDF.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/esp32-certs.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
status=0

fail() {
    echo "✗ $*" >&2
    status=1
}

# Run install-certs in a scratch ESP-IDF project; return its output and status.
run() {
    local app=$1 ; shift
    rm -rf "$TMP/project" "$TMP/app"
    mkdir -p "$TMP/project/components" "$TMP/project/state/config" "$TMP/project/state/certs" "$TMP/app/state/config"
    ln -s "$ROOT" "$TMP/project/components/ioto"
    #   The app's prepared configuration, which the Makefile installs into the project (ADIR)
    cp "$ROOT/apps/$app/ioto.json5" "$TMP/app/state/config/ioto.json5"
    if [ -n "${PREPARE:-}" ] ; then
        eval "$PREPARE"
    fi
    (cd "$TMP/project" && env -u PROFILE make -s -f "$ROOT/projects/esp32/Makefile" APP="$app" ADIR="$TMP/app" "$@" \
        install-certs 2>&1)
}

# Set production credentials while preserving LF or CRLF endings.
setProdTls() {
    CERT=$1 KEY=$2 perl -0pi -e 's/(\r?\n)([ \t]*)prod: \{(\r?\n)/$1$2prod: {$3$2    tls: {certificate: \x27\@certs\/$ENV{CERT}\x27, key: \x27\@certs\/$ENV{KEY}\x27},$3/' \
        "$TMP/app/state/config/ioto.json5"
    grep -q "@certs/$1" "$TMP/app/state/config/ioto.json5" ||
        fail "setProdTls did not find a prod profile in $TMP/app/state/config/ioto.json5"
}

hasKey() {
    [ -f "$TMP/project/state/certs/test.key" ]
}

for app in http blink ; do
    #   The app's own profile is dev: the test pair is installed
    PREPARE= run $app >/dev/null || fail "$app: dev install-certs failed"
    hasKey || fail "$app: dev build did not install the test key"
    [ -f "$TMP/project/state/certs/roots.crt" ] || fail "$app: dev build did not install roots.crt"

    #   prod with nothing provisioned: refused, and no test pair installed
    if out=$(PREPARE= run $app PROFILE=prod) ; then
        fail "$app: prod build without a provisioned identity succeeded"
    fi
    case "$out" in *provision*) ;; *) fail "$app: prod refusal does not say to provision: $out" ;; esac
    hasKey && fail "$app: prod build installed the published test key"

    #   prod naming the published pair: refused
    PREPARE='cp "$ROOT/certs/test.crt" "$ROOT/certs/test.key" "$TMP/project/state/certs" ; setProdTls test.crt test.key'
    if PREPARE="$PREPARE" run $app PROFILE=prod >/dev/null ; then
        fail "$app: prod build accepted the published test pair"
    fi

    #   prod with a provisioned identity: accepted, and no test key installed
    PREPARE='echo device-crt > "$TMP/project/state/certs/device.crt" ; echo device-key > "$TMP/project/state/certs/device.key" ; setProdTls device.crt device.key'
    PREPARE="$PREPARE" run $app PROFILE=prod >/dev/null || fail "$app: prod build with a provisioned identity failed"
    hasKey && fail "$app: prod build with a provisioned identity installed the test key"
done

if [ $status -eq 0 ] ; then
    echo "✓ ESP32 builds install the test key only for the dev profile"
fi
exit $status
