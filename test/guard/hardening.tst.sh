#!/usr/bin/env bash
#
#   hardening.tst.sh - Fail when a generated build configuration loses a hardening flag
#
#   Reads the ALL_CFLAGS and ALL_LDFLAGS that projects/gmake2 and apps/*/projects/gmake2 actually
#   compile and link with, so a flag present in the premake source but dropped or misplaced in
#   generation is caught. A flag in buildoptions reaches only the compile line: -pie there links
#   nothing, which is how executables were once linked without it on every compiler that does not
#   default to PIE.
#
#   The flags are defined once, in projects/ioto-config.lua, and shared by the library, the tools
#   and every app. A second copy in another premake5.lua is how the two sets drifted, so that is
#   checked too. Windows must link the dynamic CRT so Windows Update services CRT security fixes.
#
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
status=0

fail() {
    echo "✗ $*"
    status=1
}

#   Print the value of VAR inside the "ifeq ($(config),CFG)" block of a generated makefile
flags() {
    awk -v cfg="(\$(config),$2)" -v var="$3 +=" '
        /ifeq \(\$\(config\),/ { on = (index($0, cfg) > 0); next }
        on && index($0, var) == 1 { print }' "$1"
}

has() {     # label haystack needle
    case " $2 " in *" $3 "*) ;; *) fail "$1: missing $3" ;; esac
}

lacks() {   # label haystack needle
    case " $2 " in *" $3 "*) fail "$1: has $3" ;; esac
}

shopt -s nullglob
makefiles=("$ROOT"/projects/gmake2/*.make "$ROOT"/apps/*/projects/gmake2/*.make)
if [ ${#makefiles[@]} -eq 0 ] ; then
    echo "✓ hardening: skipped, no generated gmake2 makefiles in this tree"
    exit 0
fi

checked=0
for mk in "${makefiles[@]}" ; do
    name="${mk#"$ROOT"/}"
    #   A static library is archived, so only its compile flags apply
    exe=1
    grep -q '^LINKCMD = $(AR)' "$mk" && exe=0
    for cfg in debug release ; do
        for plat in macosx linux freebsd ; do
            cf=$(flags "$mk" "${cfg}_${plat}" ALL_CFLAGS)
            lf=$(flags "$mk" "${cfg}_${plat}" ALL_LDFLAGS)
            [ -z "$cf" ] && continue
            checked=$((checked + 1))
            label="$name ${cfg}_${plat}"
            has "$label CFLAGS" "$cf" -fstack-protector-strong
            has "$label CFLAGS" "$cf" -Wformat-security
            lacks "$label CFLAGS" "$cf" -pie
            lacks "$label CFLAGS" "$cf" --param=ssp-buffer-size=4
            if [ "$plat" != macosx ] ; then
                has "$label CFLAGS" "$cf" -fPIE
                if [ "$cfg" = release ] ; then
                    has "$label CFLAGS" "$cf" -U_FORTIFY_SOURCE
                    has "$label CFLAGS" "$cf" -D_FORTIFY_SOURCE=3
                fi
                if [ $exe = 1 ] ; then
                    has "$label LDFLAGS" "$lf" -pie
                    has "$label LDFLAGS" "$lf" -Wl,-z,relro,-z,now
                    has "$label LDFLAGS" "$lf" -Wl,-z,noexecstack
                    lacks "$label LDFLAGS" "$lf" -Wl,--no-warn-execstack
                fi
            elif [ "$cfg" = release ] && [ $exe = 1 ] ; then
                has "$label LDFLAGS" "$lf" -Wl,-x
            fi
        done
    done
done
if [ $checked -eq 0 ] ; then
    fail "hardening: no configuration blocks found in ${#makefiles[@]} makefiles"
fi

#
#   One definition. The premake sources other than projects/ioto-config.lua must not carry their own
#   configuration or platform flags.
#
for lua in "$ROOT"/projects/premake5.lua "$ROOT"/apps/*/projects/premake5.lua ; do
    name="${lua#"$ROOT"/}"
    for word in stack-protector FORTIFY_SOURCE fPIE noexecstack staticruntime 'optimize ' 'symbols ' ; do
        if grep -v '^ *--' "$lua" | grep -q -- "$word" ; then
            fail "$name: defines \"$word\" itself; it belongs in projects/ioto-config.lua"
        fi
    done
    grep -q 'dofile(ROOT .. "/projects/ioto-config.lua")' "$lua" || fail "$name: does not dofile projects/ioto-config.lua"
done

#   Windows: dynamic CRT in every configuration of every project
for proj in "$ROOT"/projects/vs2022/*.vcxproj "$ROOT"/apps/*/projects/vs2022/*.vcxproj ; do
    name="${proj#"$ROOT"/}"
    if grep -qE '<RuntimeLibrary>MultiThreaded(Debug)?</RuntimeLibrary>' "$proj" ; then
        fail "$name: links the static CRT"
    fi
done

[ $status = 0 ] && echo "✓ hardening: $checked configurations carry the full flag set"
exit $status
