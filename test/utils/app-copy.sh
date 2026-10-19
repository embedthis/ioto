#!/usr/bin/env bash
#
#   app-copy.sh - Copy a shipped app to a scratch tree it can run from. Sourced, not run.
#
#   copyApp APP SCRATCH copies apps/APP to SCRATCH/apps/APP without its build products, database,
#   uploads or any tmp directory, and links SCRATCH/certs to the repository's certs, so the app
#   finds its TLS roots at ../../certs exactly as it does in the repository.
#

#   Copy entries including dotfiles, except named exclusions; ignore unmatched globs.
copyEntries() {
    local from=$1 to=$2 entry name skip

    shift 2
    mkdir -p "$to"
    for entry in "$from"/* "$from"/.[!.]* ; do
        [ -e "$entry" ] || continue
        name=$(basename "$entry")
        for skip in "$@" ; do
            if [ "$name" = "$skip" ] ; then
                continue 2
            fi
        done
        cp -R "$entry" "$to/"
    done
}

copyApp() {
    local app=$1 scratch=$2 root dir

    root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
    dir=$scratch/apps/$app
    if [ ! -f "$root/apps/$app/state/config/schema.json5" ] ; then
        echo "apps/$app/state/config is missing. Run make first." >&2
        return 1
    fi
    # Copy named configuration and site entries; tar exclude patterns differ between GNU tar and Git Bash.
    rm -rf "$dir"
    mkdir -p "$dir/state/db"
    copyEntries "$root/apps/$app" "$dir" build projects src esp32 tmp state
    copyEntries "$root/apps/$app/state" "$dir/state" db uploads
    case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$root/bin/make-private.ps1")" \
            -Path "$(cygpath -w "$dir/state")" || return 1
        ;;
    esac
    #   Replace rather than ln -sfn: Git Bash's ln -s copies a directory, and -sfn into that copy nests
    rm -rf "$scratch/certs"
    ln -s "$root/certs" "$scratch/certs"
    if [ ! -f "$dir/state/config/schema.json5" ] ; then
        echo "$app: the scratch copy has no state/config/schema.json5" >&2
        return 1
    fi
    return 0
}
