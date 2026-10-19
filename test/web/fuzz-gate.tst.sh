#!/usr/bin/env bash
#   Run fuzz cleanup against planted ASan/UBSan report files and require failure.
set -u

DIR=tmp/fuzz-gate
CLEANUP=$(pwd)/fuzz/cleanup.sh

[ -f "$CLEANUP" ] || { echo "SKIP: fuzz/cleanup.sh is absent" ; exit 77 ; }

#   runCleanup planted-file... - Run cleanup in a fresh directory holding the planted files
runCleanup() {
    rm -fr "$DIR"
    mkdir -p "$DIR/crashes"
    for f in "$@" ; do
        echo "==1==ERROR: planted report" > "$DIR/$f"
    done
    (cd "$DIR" && TESTME_SUCCESS=1 bash "$CLEANUP") > "$DIR.log" 2>&1
}

status=0

runCleanup
if [ $? -ne 0 ] ; then
    echo "✗ cleanup failed with no reports present" ; cat "$DIR.log" ; status=1
else
    echo "✓ cleanup passes with no reports present"
fi

for report in crashes/asan.123 crashes/ubsan.456 asan.789 ; do
    runCleanup "$report"
    if [ $? -eq 0 ] ; then
        echo "✗ cleanup passed although $report was present" ; cat "$DIR.log" ; status=1
    else
        echo "✓ cleanup fails on $report"
    fi
done

rm -fr "$DIR" "$DIR.log"
exit $status
