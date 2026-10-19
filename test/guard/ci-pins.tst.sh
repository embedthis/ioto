#!/usr/bin/env bash
#
#   ci-pins.tst.sh - CI runs only third-party code it has named exactly
#
#   The workflow runs actions, Bun and TestMe with the job's token, and their results are the
#   release-gate evidence. A mutable tag, "latest" or an unversioned install runs whatever the
#   publisher serves on the day. Every action must be pinned to a full commit SHA, Bun and every
#   package installed at an exact version, and the token limited to reading the repository.
#
#   .github/ is not staged into the source distribution, so in a customer's tree this skips.
#
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
status=0

fail() {
    echo "✗ $*"
    status=1
}

shopt -s nullglob
workflows=("$ROOT"/.github/workflows/*.yml "$ROOT"/.github/workflows/*.yaml)
if [ ${#workflows[@]} -eq 0 ] ; then
    echo "✓ ci-pins: skipped, no .github/workflows in this tree"
    exit 0
fi

for wf in "${workflows[@]}" ; do
    name="${wf#"$ROOT"/}"

    #   uses: owner/repo[/path]@<40 hex> - local actions (./path) carry no reference
    while IFS= read -r line ; do
        ref=$(echo "$line" | sed -E 's/^[^:]*:[[:space:]]*uses:[[:space:]]*//; s/[[:space:]]+#.*$//; s/[[:space:]]*$//')
        case "$ref" in
        ./*) continue ;;
        esac
        if ! echo "$ref" | grep -qE '^[^@[:space:]]+@[0-9a-f]{40}$' ; then
            fail "$name: \"$ref\" is not pinned to a full commit SHA"
        fi
    done < <(grep -nE '^[[:space:]-]*uses:' "$wf")

    while IFS= read -r line ; do
        version=$(echo "$line" | sed -E 's/^.*bun-version:[[:space:]]*//; s/[[:space:]]+#.*$//; s/["'"'"']//g')
        echo "$version" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$' || fail "$name: bun-version \"$version\" is not an exact version"
    done < <(grep -E 'bun-version:' "$wf")

    #   Every package named by a bun install carries @<exact version>
    while IFS= read -r line ; do
        for word in ${line#*bun install} ; do
            case "$word" in
            -*) continue ;;
            esac
            if ! echo "$word" | grep -qE '^(@[^/@]+/)?[^@]+@[0-9]+\.[0-9]+\.[0-9]+$' ; then
                fail "$name: bun install of \"$word\" is not an exact version"
            fi
        done
    done < <(grep -E 'bun (install|add)[[:space:]]' "$wf" | grep -v '^[[:space:]]*#')

    if ! awk '/^permissions:/ { on = 1; next } on && /^[^ ]/ { on = 0 } on && /^  contents: read/ { found = 1 } END { exit !found }' "$wf" ; then
        fail "$name: no workflow-level \"permissions: contents: read\""
    fi
done

[ $status = 0 ] && echo "✓ ci-pins: actions pinned by SHA, tools by exact version, token read-only"
exit $status
