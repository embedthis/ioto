#!/usr/bin/env bash
#
#   prepare-private.tst.sh - bin/prepare leaves state/ private whatever the caller's umask
#
#   Web refuses to start unless the state directory holding its uploads is private to the server
#   user. Under umask 002 a plain mkdir makes state/ group-writable and no app starts.
#
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOP="$(mktemp -d "${TMPDIR:-/tmp}/prepare-private.XXXXXX")"
trap 'rm -rf "$TOP"' EXIT
status=0

#   Windows has no POSIX mode bits for chmod to set; web secures state/ there with an ACL instead.
#   Probe rather than test uname: WSL bash on a Windows drive reports Linux but ignores chmod.
mkdir "$TOP/probe" && chmod 700 "$TOP/probe"
if [ "$(ls -ld "$TOP/probe" | cut -c1-10)" != "drwx------" ]; then
    echo "✓ prepare-private: skipped, file system has no POSIX file modes"
    exit 0
fi

fail() {
    echo "✗ $*"
    status=1
}

checkPrivate() {
    local mode
    mode="$(ls -ld "$TOP/apps/blank/state" | cut -c1-10)"
    [ "$mode" = "drwx------" ] || fail "$1: state/ is $mode, expected drwx------"
}

mkdir -p "$TOP/src" "$TOP/apps/blank"

(umask 002 && cd "$TOP" && bash "$ROOT/bin/prepare" blank >/dev/null) || fail "prepare failed"
checkPrivate "new state/ under umask 002"

chmod 775 "$TOP/apps/blank/state"
(umask 002 && cd "$TOP" && bash "$ROOT/bin/prepare" blank >/dev/null) || fail "prepare failed"
checkPrivate "existing group-writable state/"

[ $status = 0 ] && echo "✓ prepare-private: state/ is 0700 under umask 002"
exit $status
