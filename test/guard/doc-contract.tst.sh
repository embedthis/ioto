#!/usr/bin/env bash
#   Customer documentation must match the shipped code:
#   1. Every web.* key the web server or the web command reads is documented in the reference doc/web.json5.
#   2. Every ME_* build define the security documentation names is tested by code.
#   3. Customer documents link only to files the distribution ships (the README files only to files the public
#      repository also carries), and name no internal .claude/, feature, issue or ticket path.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 1
status=0

fail() {
    echo "✗ $*"
    status=1
}

if [ ! -f doc/web.json5 ] || [ ! -f lib/webLib.c ] ; then
    echo "✓ doc-contract: skipped, the reference configuration is not present in this tree"
    exit 0
fi
VER=$(sed -n 's/.*"version": *"\([^"]*\)".*/\1/p' package.json | head -1)

#
#   1. Configuration keys. Each key's leaf name must appear as a key in doc/web.json5.
#
for key in $(grep -ohE '"web\.[A-Za-z]+(\.[A-Za-z]+)*"' lib/webLib.c src/cmds/web.c | tr -d '"' | sort -u) ; do
    case "$key" in
        web.h|web.c|web.web) continue ;;
    esac
    leaf=${key##*.}
    grep -qE "(^|[[:space:]{,/])['\"]?${leaf}['\"]?[[:space:]]*:" doc/web.json5 || \
        fail "$key is read by the code but not documented in doc/web.json5"
done

#
#   2. Build defines named by the security documentation must be tested by code
#
DEFDOCS="doc/architecture/security/security-model.md doc/releases/$VER/user-information.md include/ioto.h"
for d in $(cat $DEFDOCS 2>/dev/null | grep -oE '\bME_[A-Z0-9_]+' | sort -u) ; do
    grep -qE "(#if|#ifdef|#ifndef|defined\().*\b$d\b" lib/*.c include/*.h src/*.c src/cmds/*.c || \
        fail "build define $d is documented but tested by no code"
done

#
#   3. Customer documents. Links are checked as the archive stages them: doc/releases/VERSION/* lands in doc/.
#
#   The README files also ship in the public repository, which carries no CRA conformity documents, so they
#   may link only the release notes.
STAGED_DOC="user-information.md|declaration-of-conformity.md|release-notes.md|sbom.json"
PUBLIC_DOC="release-notes.md"
DOCS="$(ls README*.md 2>/dev/null) doc/operations/claude-customer.md"
for f in user-information declaration-of-conformity release-notes ; do
    DOCS="$DOCS doc/releases/$VER/$f.md"
done
for f in $DOCS ; do
    [ -f "$f" ] || continue
    text="$(sed -e '/<!--/,/-->/d' "$f")"
    if echo "$text" | grep -qE '\.claude/|doc/features|doc/issues|tickets/[0-9]|#[0-9]{4,5}\b' ; then
        fail "$f names an internal .claude/, feature, issue or ticket path"
    fi
    case "$f" in
        doc/releases/*) continue ;;
    esac
    for link in $(echo "$text" | grep -oE '\]\([^)#]+' | sed 's/^](//' | grep -vE '^(https?|mailto):') ; do
        case "$link" in
            doc/*)
                case "$f" in
                    README*) allowed="$PUBLIC_DOC" ;;
                    *) allowed="$STAGED_DOC" ;;
                esac
                echo "${link#doc/}" | grep -qxE "$allowed" || \
                    fail "$f links $link, which the distribution or the public repository does not ship"
                ;;
            *)
                [ -e "$(dirname "$f")/$link" ] || fail "$f links $link, which does not exist"
                ;;
        esac
    done
done

if [ "$status" -eq 0 ] ; then
    echo "✓ doc-contract: customer documentation matches the code and links only to shipped files"
fi
exit $status
