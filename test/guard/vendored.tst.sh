#!/usr/bin/env bash
#
#   vendored.tst.sh - Fail the suite when a vendored copy diverges from the module that owns it
#
#   The agent vendors two things from the sibling modules: the amalgamations under lib/, include/
#   and src/cmds, which "pak sync" writes, and the test suites under test/db, test/json, test/mqtt,
#   test/r, test/url and test/web, which "pak sync" does not touch at all.
#
#   Both drift silently. A fix made in a vendored amalgamation survives until the next sync and is
#   then reverted with the build clean and the tests green - the sibling Appweb repo lost twenty-one
#   security fixes that way. A test added upstream is simply never vendored, and the agent goes on
#   reporting a pass for code it no longer covers.
#
#   Neither guard is any use as a target a developer has to remember, so both run here, in the
#   ordinary suite.
#
#   Exit 2 from either script means the sources it compares are absent - paks/ is gitignored, and
#   the sibling checkouts do not exist in a source distribution or a git worktree. That is a skip,
#   and it is announced rather than swallowed, so "nothing to check" is never read as "everything
#   matched".
#
#   The two scripts are themselves withheld from the source distribution: bin/make-dist stages a
#   named list of bin/ scripts and neither guard is on it, because neither has anything to compare
#   against outside a full development checkout. test/ does ship, so this file runs in a customer's
#   tree with nothing to call - also a skip, for the same reason and reported the same way.
#
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
status=0

check() {
    local name="$1" script="$2"
    shift 2
    local output rc

    if [ ! -f "$script" ]; then
        echo "✓ $name: skipped, $(basename "$script") is not present in this tree"
        return
    fi
    output=$(bash "$script" "$@" 2>&1)
    rc=$?

    echo "$output"
    case "$rc" in
    0) echo "✓ $name: vendored copies match their sources" ;;
    2) echo "✓ $name: skipped, the sources it compares are not present in this tree" ;;
    *) echo "✗ $name: a vendored copy diverges from the module that owns it" >&2
       status=1 ;;
    esac
}

check check-amalgamation "$ROOT/bin/check-amalgamation.sh"
check sync-tests         "$ROOT/bin/sync-tests.sh" --check

exit $status
