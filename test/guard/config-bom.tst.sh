#!/usr/bin/env bash
#
#   config-bom.tst.sh - No configuration file the build writes or ships starts with a byte order mark
#
#   The JSON5 parser rejects a leading UTF-8 BOM, so an app whose state/config/schema.json5 carries
#   one cannot open its database and exits at startup. Windows PowerShell's "Out-File -Encoding
#   utf8" writes exactly that, which is how bin/prepare.ps1 once broke every app on Windows. On
#   Windows CI this checks the files make.bat actually produced; elsewhere it checks bin/prepare's.
#
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
status=0
checked=0

fail() {
    echo "✗ $*"
    status=1
}

hasBom() {
    [ "$(head -c 3 "$1" | od -An -tx1 | tr -d ' \n')" = "efbbbf" ]
}

shopt -s nullglob
for file in "$ROOT"/apps/*/state/config/*.json5 "$ROOT"/apps/*/*.json5 "$ROOT"/test/utils/agent/*.json5 ; do
    checked=$((checked + 1))
    hasBom "$file" && fail "${file#"$ROOT"/} starts with a UTF-8 byte order mark"
done

#   The writer: a PowerShell script must let json write the file rather than re-encode its output
for script in "$ROOT"/bin/*.ps1 ; do
    if grep -nE '^[^#]*\|[[:space:]]*(Out-File|Set-Content|Add-Content)' "$script" ; then
        fail "${script#"$ROOT"/} pipes output through a PowerShell writer, which can add a byte order mark"
    fi
done

[ $status = 0 ] && echo "✓ config-bom: $checked configuration files, none with a byte order mark"
exit $status
