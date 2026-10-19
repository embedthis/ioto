#!/usr/bin/env bash
#
#   skip-windows.sh - Skip a group that cannot run on Windows
#
#   TestMe runs a group when its skip service exits 0 and skips it otherwise.
#
#   The soak and stress groups launch ioto-unit as a background process, watch it by pid through
#   a pidfile, and stop it with a signal from a trap. None of that is available on Windows, and
#   test/stress/upload.tst.c counts staged files through <dirent.h>, which MSVC does not ship.
#   Making these portable is worth doing; asserting a pass on a platform where they never ran is
#   not, so they skip loudly here until then.
#
if [ "${TESTME_OS}" = "windows" ] || [ "${OS}" = "Windows_NT" ]; then
    echo "skipped on Windows: this group launches and signals a background agent, which needs a POSIX process model"
    exit 1
fi
exit 0
