#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script
#

make-files() {
    COUNT=$1
    OUTPUT=$2
    if [ ! -f $OUTPUT ] ; then
        echo "   [Create] $OUTPUT"
        > $OUTPUT
        while [ $COUNT -gt 0 ] ; 
        do
            echo 0123456789012345678901234567890123456789012345678 >>$OUTPUT
            COUNT=$((COUNT - 1))
        done
        echo END OF DOCUMENT >>$OUTPUT
    fi
}

mkdir -p tmp site/upload state .test

#
#   The server refuses to start unless its upload directory is private: owned by the server user
#   and writable by no one else. On a build agent a new directory is neither, so make each parent
#   private with inheritable entries. C's _mkdir inherits the parent's ACL, so everything the
#   server and the tests create underneath - state/uploads, .test/upload-dir-<pid> - is private
#   too, without the C code having to know about Windows ACLs.
#
#   Detected with uname, not ${OS}: the test runner builds its own environment for service
#   scripts and does not necessarily carry Windows' own variables through.
#
case "$(uname -s 2>/dev/null)" in
MINGW*|MSYS*|CYGWIN*)
    SID=$(powershell -NoProfile -Command '[Security.Principal.WindowsIdentity]::GetCurrent().User.Value' | tr -d '\r')
    #
    #   MSYS_NO_PATHCONV, because Git Bash rewrites an argument that looks like an absolute path:
    #   "/setowner" arrived as "C:/Program Files/Git/setowner". The switches carrying a colon
    #   survive that heuristic; the ones without it do not.
    #
    #   One operation per invocation: combined with /grant, /inheritance:r was silently not
    #   applied. And the entries to remove are explicit, not inherited - MSYS's mkdir writes a
    #   POSIX-mode DACL of its own - so /inheritance:r has nothing to strip and they go by name.
    #
    #   Written to upload-acl.log because a prep script's output is not shown when it succeeds;
    #   setup.sh prints this file if the server then refuses to start.
    #
    makePrivate() {
        win=$(cygpath -w "$1")
        echo "-- $1"
        MSYS_NO_PATHCONV=1 icacls "${win}" /setowner "*${SID}" 2>&1
        MSYS_NO_PATHCONV=1 icacls "${win}" /inheritance:r 2>&1
        MSYS_NO_PATHCONV=1 icacls "${win}" \
            /grant:r "*${SID}:(OI)(CI)F" \
            /grant:r "*S-1-5-18:(OI)(CI)F" \
            /grant:r "*S-1-5-32-544:(OI)(CI)F" 2>&1
        MSYS_NO_PATHCONV=1 icacls "${win}" \
            /remove:g "*S-1-5-32-545" "*S-1-1-0" "*S-1-5-11" 2>&1
        MSYS_NO_PATHCONV=1 icacls "${win}" 2>&1
    }
    {
        echo "user SID: ${SID}"
        for d in tmp state .test ; do
            makePrivate "$d"
        done
    } > upload-acl.log 2>&1
    ;;
esac


. ./server-id.sh

#
#   Identity fixture for the probe in server-id.sh, and the foreign-server refusal. It belongs
#   here, not only in setup.sh: TestMe runs setup in the background while its healthcheck accepts
#   any 200, and a failed prep stops the run before any test.
#
echo "${SUITE_ID}" > site/${PROBE}
if ! probe && occupied; then
    refuseForeign
    exit 1
fi

#
#   Fixtures for the deny-by-default filename policy. Generated rather than tracked: a real ".git"
#   directory under the document root would make git treat site/ as a nested repository and stop
#   tracking everything in it, so the dot-directory case uses ".hidden" - the pattern that matches
#   it, ".*", is the same one that refuses ".git".
#
mkdir -p site/.hidden
printf 'API_KEY=secret\n' > site/.env
printf 'hidden\n' > site/.hidden/secret.txt
printf 'backup\n' > "site/index.html~"
printf 'backup\n' > site/x.bak
printf 'sourcemap\n' > site/app.js.map
printf 'secret\n' > site/x.secret

#
#   A FIFO in the document root. open() on it blocks until a writer appears, which on a
#   single-threaded server stalls every connection - so the file handler must refuse a
#   non-regular file rather than open it. Recreated each run; it is not tracked in git.
#
rm -f site/wedge.fifo
mkfifo site/wedge.fifo 2>/dev/null || true

#
#   Unicode fixture for the path-spelling tests. The name is written in NFC. On a
#   normalizing file system (APFS, HFS+) the NFD spelling opens the same file, so the
#   server must reject it on the resolved name rather than rely on the file system.
#
#
#   A protected directory whose on-disk spelling differs from the route that guards it
#   ({match: '/restricted/'}). On a case-folding file system a request for "/Restricted/..."
#   missed the route and the file handler resolved it to exactly the file it asked for, so the
#   protected body was served with a 200. Generated rather than tracked: a checkout on a
#   case-folding file system cannot hold both spellings.
#
mkdir -p site/Restricted
printf 'restricted-body\n' > site/Restricted/secret.html

mkdir -p site/unicode
# Remove an NFD name carried by a macOS archive before creating the sole NFC fixture.
rm -f "$(printf 'site/unicode/cafe\314\201.html')"
printf 'cafe-nfc\n' > "$(printf 'site/unicode/caf\303\251.html')"
printf 'mixed-case\n' > site/unicode/MixedCase.html

# Create a 100-byte read-only file for range and conditional request tests.
# Tests that modify documents create private files under site/upload.
printf '0123456789%.0s' {1..10} > site/range-test.txt 2>/dev/null || {
    # Fallback for shells without brace expansion
    printf '0123456789' > site/range-test.txt
    for i in 1 2 3 4 5 6 7 8 9; do
        printf '0123456789' >> site/range-test.txt
    done
}

make-files 20 site/size/1K.txt
make-files 205 site/size/10K.txt
make-files 512 site/size/25K.txt
make-files 2050 site/size/100K.txt
make-files 10250 site/size/500K.txt
make-files 21000 site/size/1M.txt
make-files 210000 site/size/10M.txt
