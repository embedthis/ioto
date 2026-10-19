#!/usr/bin/env bash
#   Build the web samples against the built libioto.a, run each from a temporary copy on a test port and
#   check that it serves what its README promises. A sample whose configuration its entry point cannot
#   read builds cleanly and serves nothing, so only running it catches that.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PORT=4581
URL="http://127.0.0.1:${PORT}"
TMP="$ROOT/test/guard/tmp/samples-run"
status=0
PID=

if [ ! -f "$ROOT/build/bin/libioto.a" ] || [ ! -d "$ROOT/samples" ] ; then
    echo "✓ samples-run: skipped, no built libioto.a or samples/ in this tree"
    exit 0
fi
command -v curl >/dev/null 2>&1 || { echo "✓ samples-run: skipped, curl is not installed" ; exit 0 ; }
case "$(uname -s)" in
    Darwin|Linux) ;;
    *) echo "✓ samples-run: skipped, the sample Makefiles support only Linux and macOS" ; exit 0 ;;
esac

fail() {
    echo "✗ $1"
    status=1
}

stop() {
    if [ -n "$PID" ] ; then
        kill "$PID" 2>/dev/null
        wait "$PID" 2>/dev/null
        PID=
    fi
}
trap 'stop; rm -fr "$TMP"' EXIT

#   Copy a sample, point its listener at the test port and build it against this tree
prepare() {
    local name="$1"
    rm -fr "$TMP/$name"
    mkdir -p "$TMP"
    cp "$ROOT/samples/defines.mk" "$TMP/"
    cp -R "$ROOT/samples/$name" "$TMP/$name"
    rm -f "$TMP/$name/sample" "$TMP/$name/sample.o"
    sed -i.bak "s|http://127.0.0.1:8080|${URL}|" "$TMP/$name/web.json5"
    if ! make -C "$TMP/$name" TOP="$ROOT" > "$TMP/$name.build.log" 2>&1 ; then
        fail "$name: does not build against build/bin/libioto.a"
        cat "$TMP/$name.build.log"
        return 1
    fi
}

start() {
    local name="$1"
    (cd "$TMP/$name" && exec ./sample) > "$TMP/$name.log" 2>&1 &
    PID=$!
    for _ in $(seq 1 50) ; do
        curl -s -m 1 -o /dev/null "$URL/" 2>/dev/null && return 0
        kill -0 "$PID" 2>/dev/null || break
        sleep 0.1
    done
    fail "$name: not listening on $URL"
    cat "$TMP/$name.log"
    stop
    return 1
}

code() {
    curl -s -m 5 -o /dev/null -w '%{http_code}' "$@"
}

#
#   web-static: serves its site directory
#
if prepare web-static && start web-static ; then
    got=$(code "$URL/")
    [ "$got" = 200 ] && echo "✓ web-static: GET / -> 200" || fail "web-static: GET / -> $got, expected 200"
    stop
fi

#
#   web-dynamic: serves its site directory and the /test action answers
#
if prepare web-dynamic && start web-dynamic ; then
    got=$(code "$URL/")
    [ "$got" = 200 ] && echo "✓ web-dynamic: GET / -> 200" || fail "web-dynamic: GET / -> $got, expected 200"
    body=$(curl -s -m 5 -d x "$URL/test")
    case "$body" in
        "Hello World"*) echo "✓ web-dynamic: POST /test -> hello" ;;
        *) fail "web-dynamic: POST /test -> '$body'" ;;
    esac
    stop
fi

#
#   web-auth: password verified against the stored hash, role taken from the server's user record
#
if prepare web-auth ; then
    hash=$("$ROOT/build/bin/password" --algorithm bcrypt --realm web --password 'Sample-run-1' alice | \
        sed -n "s/.*password: '\([^']*\)'.*/\1/p" | head -1)
    guest=$("$ROOT/build/bin/password" --algorithm bcrypt --realm web --password 'Sample-run-2' guest | \
        sed -n "s/.*password: '\([^']*\)'.*/\1/p" | head -1)
    users="alice: { password: '${hash}', role: 'user' }, guest: { password: '${guest}', role: 'public' }"
    sed -i.bak "s|users: {},|users: { ${users} },|" "$TMP/web-auth/web.json5"
    if [ -z "$hash" ] || [ -z "$guest" ] ; then
        fail "web-auth: the password command produced no hash"
    elif start web-auth ; then
        jar="$TMP/web-auth.cookies"
        got=$(code "$URL/welcome.html")
        [ "$got" = 401 ] && echo "✓ web-auth: anonymous GET /welcome.html -> 401" || \
            fail "web-auth: anonymous GET /welcome.html -> $got, expected 401"

        got=$(code -d 'username=nobody&password=demo' "$URL/auth/login")
        [ "$got" = 401 ] && echo "✓ web-auth: unknown user with password demo -> 401" || \
            fail "web-auth: unknown user with password demo -> $got, expected 401"

        got=$(code -d 'username=alice&password=wrong' "$URL/auth/login")
        [ "$got" = 401 ] && echo "✓ web-auth: wrong password -> 401" || fail "web-auth: wrong password -> $got"

        got=$(curl -s -m 5 -o /dev/null -c "$jar" -w '%{http_code} %{redirect_url}' \
            -d 'username=alice&password=Sample-run-1' "$URL/auth/login")
        loc=${got#* }
        got=${got%% *}
        if [ "$got" = 302 ] && [ "${loc##*/}" = welcome.html ] ; then
            echo "✓ web-auth: correct password -> 302 /welcome.html"
        else
            fail "web-auth: correct password -> $got $loc, expected 302 to /welcome.html"
        fi
        got=$(code -b "$jar" "$URL/welcome.html")
        [ "$got" = 200 ] && echo "✓ web-auth: logged-in GET /welcome.html -> 200" || \
            fail "web-auth: logged-in GET /welcome.html -> $got, expected 200"

        got=$(code -b "$jar" "$URL/auth/logout")
        [ "$got" != 302 ] && echo "✓ web-auth: GET /auth/logout does not log out ($got)" || \
            fail "web-auth: GET /auth/logout logged out"
        got=$(code -b "$jar" -X POST "$URL/auth/logout")
        [ "$got" = 302 ] && echo "✓ web-auth: POST /auth/logout -> 302" || fail "web-auth: POST /auth/logout -> $got"
        got=$(code -b "$jar" "$URL/welcome.html")
        [ "$got" = 401 ] && echo "✓ web-auth: GET /welcome.html after logout -> 401" || \
            fail "web-auth: GET /welcome.html after logout -> $got, expected 401"

        #   The session carries the role configured for the user, so a public-role user cannot reach a user page
        rm -f "$jar"
        got=$(code -c "$jar" -d 'username=guest&password=Sample-run-2' "$URL/auth/login")
        [ "$got" = 302 ] || fail "web-auth: guest login -> $got, expected 302"
        got=$(code -b "$jar" "$URL/welcome.html")
        case "$got" in
            401|403) echo "✓ web-auth: a public-role user is refused /welcome.html ($got)" ;;
            *) fail "web-auth: a public-role user got /welcome.html -> $got" ;;
        esac
        stop
    fi
fi

#
#   web-upload: /api/upload is routed and the upload lands in the private default directory
#
if prepare web-upload && start web-upload ; then
    echo "sample upload" > "$TMP/upload.txt"
    got=$(code -F "photo=@$TMP/upload.txt" -F 'Name=x' "$URL/api/upload")
    [ "$got" = 200 ] && echo "✓ web-upload: POST /api/upload -> 200" || fail "web-upload: POST /api/upload -> $got"
    dir="$TMP/web-upload/state/uploads"
    if [ ! -d "$dir" ] ; then
        fail "web-upload: no private upload directory at state/uploads"
    else
        mode=$(stat -c '%a' "$dir" 2>/dev/null || stat -f '%Lp' "$dir")
        [ "$mode" = 700 ] && echo "✓ web-upload: uploads go to state/uploads (mode 0700)" || \
            fail "web-upload: state/uploads has mode $mode, expected 700"
    fi
    grep -q "Upload upload.txt to .*state/uploads/" "$TMP/web-upload.log" && \
        echo "✓ web-upload: the action received the file under state/uploads" || \
        fail "web-upload: the action did not receive the file under state/uploads"
    stop
fi

if [ "$status" -eq 0 ] ; then
    echo "✓ samples-run: every web sample serves on loopback as documented"
fi
exit $status
