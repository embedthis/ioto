#!/usr/bin/env bash
#
#   agent-setup.sh - Start the agent for a scenario group and hold it open until TestMe stops it
#
#   Runs with the group directory as the working directory, after agent-prep.sh has built state/.
#   The agent's default state directory is "state" under its working directory, so it needs no
#   arguments: it reads state/config/ioto.json5 and web.json5 from there.
#
#   Unlike test/web/setup.sh this never adopts a server that is already answering. The soak
#   measures one process across one interval, and a server that had already served something
#   would start from an unknown baseline. Something already on the port is an error.
#
#   The pid is written to .pidfile for the tests, which sample the process by pid and never by
#   name: a name match reaches every ioto-unit on the machine, including another worktree's.
#
set -m

PIDFILE=.pidfile
PROBE=index.html
ENDPOINT=$(json 'listen[0]' state/config/web.json5)

if [ -z "$ENDPOINT" ] ; then
    echo "Error: cannot read listen[0] from state/config/web.json5. Did agent-prep.sh run?" >&2
    exit 1
fi

#
#   A server is one that answers 200 for the healthcheck document. The status is tested rather
#   than curl's exit code: -f would also fail on a body that did not arrive, and a body can be
#   withheld by the environment (a sandbox that refuses sendfile) when the server is fine.
#
probe() {
    [ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 "${ENDPOINT}/${PROBE}" 2>/dev/null)" = "200" ]
}

#
#   Free the port if a previous run of this directory left its agent behind. The pid was recorded
#   here when it started, so signal that pid and nothing else, and only after ps confirms it is
#   still an ioto-unit rather than a recycled pid belonging to something unrelated.
#
if [ -f "$PIDFILE" ] ; then
    STALE=$(cat "$PIDFILE" 2>/dev/null)
    case "$STALE" in
    ''|*[!0-9]*) ;;
    *)
        case "$(ps -p "$STALE" -o command= 2>/dev/null)" in
        *ioto-unit*)
            kill -9 "$STALE" 2>/dev/null
            sleep 0.3
            ;;
        esac
        ;;
    esac
    rm -f "$PIDFILE"
fi

if probe ; then
    echo "Error: something already answers on ${ENDPOINT}. This group must start its own agent." >&2
    lsof -nP -iTCP:"${ENDPOINT##*:}" -sTCP:LISTEN >&2 2>/dev/null
    exit 1
fi

ioto-unit &
PID=$!
echo "$PID" > "$PIDFILE"

#
#   SIGTERM first so the agent closes its database cleanly; SIGKILL only if it lingers.
#
#   Installed the moment the agent exists, before the readiness wait below. TestMe stops this
#   script with SIGTERM as soon as its own healthcheck passes and the tests finish, which at
#   depth 0 is within a second - so a trap installed after the wait can be too late, and the
#   agent it should have stopped outlives the run holding the port.
#
stopAgent() {
    kill -TERM "$PID" 2>/dev/null
    for i in 1 2 3 4 5 6 7 8 9 10 ; do
        kill -0 "$PID" 2>/dev/null || break
        sleep 0.5
    done
    kill -9 "$PID" 2>/dev/null
    rm -f "$PIDFILE"
}

#   Being asked to stop is success; an exit of our own keeps its status
trap 'stopAgent; exit 0' SIGINT SIGTERM SIGQUIT
trap stopAgent EXIT

#
#   Wait for the agent to answer before returning, so tests cannot race the startup. Opening the
#   database and loading the schema takes longer than the web command's start, hence the budget.
#
for i in $(seq 1 100) ; do
    if probe ; then
        break
    fi
    if ! kill -0 "$PID" 2>/dev/null ; then
        echo "Error: ioto-unit exited immediately. See ioto.log" >&2
        tail -20 ioto.log 2>/dev/null >&2
        exit 1
    fi
    sleep 0.1
done
if ! probe ; then
    echo "Error: ioto-unit did not answer on ${ENDPOINT}/${PROBE} within 10 seconds" >&2
    exit 1
fi

wait "$PID"
