#!/bin/bash
#
#   dbcmd.tst.sh - Tests for the dbcmd command line tool
#
#   Drives the tool through its commands: reset, create and update by assignment, query by
#   property with field selection, list a model, load seed data, expire, and the error paths.
#
DB=db/dbcmd.db
SCHEMA=schema.json

fail() {
    echo "✗ $1"
    exit 1
}

#   The db module builds the tool as dbcmd and the agent builds it as db
here=$(cd "$(dirname "$0")" && pwd)
for tool in "$here/../build/bin/dbcmd" "$here/../../build/bin/db" "$(command -v dbcmd)"; do
    if [ -n "$tool" ] && [ -x "$tool" ]; then
        DBCMD=$tool
        break
    fi
done
[ -n "$DBCMD" ] || fail "cannot find the db command"

$DBCMD --schema $SCHEMA --reset $DB >/dev/null || fail "reset failed"
[ -f $DB ] || fail "reset did not create $DB"

#   An assignment upserts the item and saves the database
$DBCMD --schema $SCHEMA $DB User id=u1 username=alice email=alice@example.com role=user >/dev/null || fail "create failed"

#   A property query selects the item and the bare argument selects the field to print
out=$($DBCMD --schema $SCHEMA $DB User id:u1 email) || fail "query failed"
[ "$out" = "alice@example.com" ] || fail "query printed '$out', expected alice@example.com"

#   An update with --show prints the updated item
out=$($DBCMD --schema $SCHEMA --show $DB User id=u1 role=admin) || fail "update failed"
echo "$out" | grep -q "admin" || fail "update did not show the new role: $out"
out=$($DBCMD --schema $SCHEMA $DB User id:u1 role)
[ "$out" = "admin" ] || fail "update was not persisted, role is '$out'"

#   A model with no query lists every item as a JSON array
out=$($DBCMD --schema $SCHEMA $DB User) || fail "list failed"
echo "$out" | grep -q "alice" || fail "list did not include alice"

#   No model dumps the entire database
out=$($DBCMD --schema $SCHEMA $DB) || fail "dump failed"
echo "$out" | grep -q "alice@example.com" || fail "dump did not include the user"

#   --load seeds from a JSON file and skips items that already exist
$DBCMD --schema $SCHEMA --load seed.json5 $DB >/dev/null || fail "load failed"
out=$($DBCMD --schema $SCHEMA $DB User username:seed-admin role)
[ "$out" = "admin" ] || fail "seed-admin was not loaded, role is '$out'"
out=$($DBCMD --schema $SCHEMA $DB Port id:p2 speed)
[ "$out" = "100" ] || fail "seed port p2 was not loaded, speed is '$out'"
$DBCMD --schema $SCHEMA --load seed.json5 $DB >/dev/null || fail "reload failed"
count=$($DBCMD --schema $SCHEMA $DB Port | grep -c "tty0")
[ "$count" = "3" ] || fail "reload duplicated items, found $count ports"

#   --expire removes expired items and exits cleanly when there are none
$DBCMD --schema $SCHEMA --expire $DB >/dev/null || fail "expire failed"

#   Error paths
$DBCMD $DB >/dev/null 2>&1 && fail "a missing --schema was accepted"
$DBCMD --schema >/dev/null 2>&1 && fail "a --schema with no value was accepted"
$DBCMD --schema $SCHEMA >/dev/null 2>&1 && fail "a missing database path was accepted"
$DBCMD --schema $SCHEMA $DB NoSuchModel >/dev/null 2>&1 && fail "an unknown model was accepted"
$DBCMD --schema $SCHEMA --load no-such-file.json5 $DB >/dev/null 2>&1 && fail "a missing load file was accepted"

echo "✓ dbcmd tests passed"
exit 0
