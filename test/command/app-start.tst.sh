#!/usr/bin/env bash
#
#   app-start.tst.sh - Every shipped app starts from a fresh copy with no upload directory
#
#   Each app is copied, without its database or any tmp directory, to a scratch tree that
#   mirrors the repository layout (the apps find their TLS roots at ../../certs). It must start,
#   answer on a loopback port, and create the private default upload directory state/uploads.
#
#   The http app is also logged in to, with accounts made by the password utility exactly as its
#   README says, through the agent's login at auth.login: the right password must log in, and an
#   unknown user and a wrong password must get the same refusal. getAuth must then answer with
#   well-formed JSON, even for a username containing a quote, and nothing may carry the stored hash.
#
#   The unit app is a test harness whose documents are supplied by the group that runs it, so its
#   copy is given an empty site directory.
#
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT/build/bin"
SCRATCH="$(pwd)/.testme/app-start"
PORT=4462
PASSWORD='Synthetic-Test-1'
QUOTED='qu"ote'
COOKIES="$SCRATCH/cookies"

status=0
PID=

. "$ROOT/test/utils/app-copy.sh"

fail() {
    echo "✗ $*" >&2
    status=1
}

stopApp() {
    if [ -n "$PID" ] ; then
        kill -TERM "$PID" 2>/dev/null
        for i in 1 2 3 4 5 6 7 8 9 10 ; do
            kill -0 "$PID" 2>/dev/null || break
            sleep 0.3
        done
        kill -9 "$PID" 2>/dev/null
        wait "$PID" 2>/dev/null
        PID=
    fi
}
trap stopApp EXIT

#   GNU stat first: there, -f selects the filesystem, so the BSD form "succeeds" with the wrong output
modeOf() {
    stat -c %a "$1" 2>/dev/null || stat -f %Lp "$1"
}

#   Windows privacy is the directory's ACL; Git Bash reports an emulated mode for every directory
isPrivate() {
    case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        powershell -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$ROOT/bin/make-private.ps1")" \
            -Check -Path "$(cygpath -w "$1")" >&2
        ;;
    *)
        [ "$(modeOf "$1")" = "700" ] || { echo "$1 has mode $(modeOf "$1"), expected 700" >&2 ; return 1 ; }
        ;;
    esac
}

startApp() {
    local app=$1 dir=$SCRATCH/apps/$1 code i

    if [ ! -x "$BIN/ioto-$app" ] ; then
        fail "ioto-$app is not built"
        return 1
    fi
    if ! copyApp "$app" "$SCRATCH" ; then
        fail "$app: cannot make a scratch copy"
        return 1
    fi
    if [ "$app" = "http" ] ; then
        printf "{User: [{username: 'guest', password: '%s', role: 'user'}, {username: '%s', password: '%s', role: 'user'}]}\n" \
            "$(makeHash guest)" "$QUOTED" "$(makeHash "$QUOTED")" > "$dir/state/config/db.json5"
    fi
    if [ "$app" = "unit" ] ; then
        mkdir -p "$dir/site"
    fi

    (cd "$dir" && exec "$BIN/ioto-$app" --listen "['http://127.0.0.1:${PORT}']" > "$dir/ioto.out" 2>&1) &
    PID=$!
    for i in $(seq 1 50) ; do
        code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 "http://127.0.0.1:${PORT}/" 2>/dev/null)
        if [ "$code" != "000" ] ; then
            break
        fi
        if ! kill -0 "$PID" 2>/dev/null ; then
            break
        fi
        sleep 0.2
    done
    if ! kill -0 "$PID" 2>/dev/null ; then
        fail "ioto-$app exited at startup"
        #   The reason is the line the agent logged before it gave up, which can be well above the last few
        sed -n '1,40p' "$dir/ioto.out" >&2
        PID=
        return 1
    fi
    if [ "$code" = "000" ] ; then
        fail "ioto-$app did not answer on port $PORT"
        stopApp
        return 1
    fi
    if [ ! -d "$dir/state/uploads" ] ; then
        fail "ioto-$app did not create state/uploads"
    elif ! isPrivate "$dir/state/uploads" ; then
        fail "ioto-$app state/uploads is not private"
    fi
    if [ -d "$dir/tmp" ] ; then
        fail "ioto-$app created a CWD-relative tmp directory"
    fi
    return 0
}

#   The BF1 hash the README's password command makes for a user
makeHash() {
    "$BIN/password" --cipher blowfish --realm web --password "$PASSWORD" "$1" |
        sed -n "s/^{ username: .*, password: '\(BF1:[^']*\)'.*/\1/p"
}

#   POST a login and print the status, then the response headers and body
postLogin() {
    curl -s --max-time 5 -i -c "$COOKIES" -o - -w '\n%{http_code}' --data-urlencode "username=$1" \
        --data-urlencode "password=$2" "http://127.0.0.1:${PORT}/api/public/login"
}

#   The auth state the pages read: print the Content-Type, then the body
getAuth() {
    curl -s --max-time 5 -b "$COOKIES" -o - -w '\n%{content_type}' "http://127.0.0.1:${PORT}/api/public/getAuth"
}

checkGetAuth() {
    local user=$1 response body type

    response=$(getAuth)
    type=$(printf '%s' "$response" | tail -1)
    body=$(printf '%s' "$response" | sed '$d')
    case "$type" in
    application/json*) ;;
    *) fail "getAuth answered Content-Type '$type', expected application/json" ;;
    esac
    if ! printf '%s' "$body" | "$BIN/json" --stdin --check --strict >/dev/null 2>&1 ; then
        fail "getAuth for $user is not well-formed JSON: $body"
    elif [ "$(printf '%s' "$body" | "$BIN/json" --stdin username 2>/dev/null)" != "$user" ] ; then
        fail "getAuth does not name $user: $body"
    fi
    case "$body" in
    *password*|*BF1:*|*_type*|*\"sk\"*) fail "getAuth carries stored record fields: $body" ;;
    esac
}

checkLogin() {
    local unknown wrong response

    if [ -z "$(makeHash guest)" ] ; then
        fail "the password utility printed no database record"
        return
    fi
    rm -f "$COOKIES"
    response=$(getAuth | sed '$d')
    if [ "$response" != "{}" ] ; then
        fail "getAuth without a session answered '$response', expected {}"
    fi
    unknown=$(postLogin nosuchuser "$PASSWORD" | grep -viE '^(date|keep-alive):')
    wrong=$(postLogin guest wrong-password | grep -viE '^(date|keep-alive):')
    if [ "$(printf '%s' "$wrong" | tail -1)" != "401" ] ; then
        fail "a wrong password answered $(printf '%s' "$wrong" | tail -1), expected 401"
    fi
    if [ "$unknown" != "$wrong" ] ; then
        fail "an unknown user and a wrong password must get the same response"
        printf 'unknown:\n%s\nwrong:\n%s\n' "$unknown" "$wrong" >&2
    fi
    rm -f "$COOKIES"
    response=$(postLogin guest "$PASSWORD")
    if [ "$(printf '%s' "$response" | tail -1)" != "302" ] ; then
        fail "guest with the README-made hash answered $(printf '%s' "$response" | tail -1), expected 302"
    fi
    case "$response" in
    *password*|*BF1:*|*_type*) fail "the login response carries stored record fields: $response" ;;
    esac
    checkGetAuth guest

    rm -f "$COOKIES"
    response=$(postLogin "$QUOTED" "$PASSWORD")
    if [ "$(printf '%s' "$response" | tail -1)" != "302" ] ; then
        fail "$QUOTED answered $(printf '%s' "$response" | tail -1), expected 302"
    fi
    checkGetAuth "$QUOTED"
}

for app in blank http unit ; do
    if startApp "$app" ; then
        if [ "$app" = "http" ] ; then
            checkLogin
        fi
        stopApp
    fi
done

if [ $status -eq 0 ] ; then
    echo "✓ Apps start with the private default upload directory"
    rm -rf "$SCRATCH"
fi
exit $status
