#!/usr/bin/env bash
#
#   prep.sh - TestMe pre script
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

mkdir -p tmp state site/size

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
        for d in tmp state ; do
            makePrivate "$d"
        done
    } > upload-acl.log 2>&1
    ;;
esac


make-files 20 site/size/1K.txt
make-files 205 site/size/10K.txt
make-files 512 site/size/25K.txt
make-files 2050 site/size/100K.txt
make-files 10250 site/size/500K.txt
make-files 21000 site/size/1M.txt
make-files 210000 site/size/10M.txt
