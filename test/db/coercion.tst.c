/*
    coercion.c.tst - Unit tests for schema type mapping and enum checking

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"
#include    "json.h"

/************************************ Code ************************************/

/*
    A numeric date is converted to an ISO date, anything else is rejected
 */
static void testDateCoercion()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/coercion-date.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Event", DB_PROPS(
        "id", "d1",
        "message", "epoch date",
        "subject", "Dates",
        "source", "test",
        "severity", "info",
        "expires", "1581636633399"
    ), NULL);
    tnotnull(item);
    tcontains(dbField(item, "expires"), "Z");
    teqll(dbFieldDate(item, "expires"), 1581636633399LL);

    //  A date that is neither numeric nor ISO is rejected
    item = dbCreate(db, "Event", DB_PROPS(
        "id", "d2",
        "message", "bad date",
        "subject", "Dates",
        "source", "test",
        "severity", "info",
        "expires", "next tuesday"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    dbClose(db);
}

/*
    A date string must be a complete ISO date and is stored in canonical UTC form so that
    TTL comparisons, which are lexicographic, are meaningful
 */
static void testDateValidation()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/coercion-date-iso.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  A bare "Z" is not a date, even though it ends in Z and sorts after every real date
    item = dbCreate(db, "Event", DB_PROPS(
        "id", "z1",
        "message", "bare Z",
        "subject", "Dates",
        "source", "test",
        "severity", "info",
        "expires", "Z"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    item = dbCreate(db, "Event", DB_PROPS(
        "id", "z2",
        "message", "trailing Z",
        "subject", "Dates",
        "source", "test",
        "severity", "info",
        "expires", "garbage Z"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    //  A valid date without milliseconds is stored canonically
    item = dbCreate(db, "Event", DB_PROPS(
        "id", "z3",
        "message", "no milliseconds",
        "subject", "Dates",
        "source", "test",
        "severity", "info",
        "expires", "2030-06-01T12:00:00Z"
    ), NULL);
    tnotnull(item);
    tmatch(dbField(item, "expires"), "2030-06-01T12:00:00.000Z");

    //  A valid date with a numeric UTC offset is accepted and converted to UTC
    item = dbCreate(db, "Event", DB_PROPS(
        "id", "z4",
        "message", "offset date",
        "subject", "Dates",
        "source", "test",
        "severity", "info",
        "expires", "2030-06-01T14:30:00+02:30"
    ), NULL);
    tnotnull(item);
    tmatch(dbField(item, "expires"), "2030-06-01T12:00:00.000Z");

    //  An expired item supplied in a non-canonical form still expires
    item = dbCreate(db, "Event", DB_PROPS(
        "id", "z5",
        "message", "already expired",
        "subject", "Dates",
        "source", "test",
        "severity", "info",
        "expires", "2020-01-01T00:00:00Z"
    ), NULL);
    tnotnull(item);
    teqi(dbRemoveExpired(db, 0), 1);
    tnull(dbGet(db, "Event", DB_PROPS("id", "z5"), NULL));

    //  The unexpired items are untouched
    tnotnull(dbGet(db, "Event", DB_PROPS("id", "z3"), NULL));

    //  A date supplied to a query is canonicalized the same way, so it matches the stored value
    item = dbFindOne(db, "Event", DB_PROPS("id", "z4", "expires", "2030-06-01T14:30:00+02:30"), NULL);
    tnotnull(item);

    dbClose(db);
}

/*
    Booleans accept true/false and 1/0 and reject anything else
 */
static void testBooleanCoercion()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/coercion-bool.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Port", DB_PROPS(
        "id", "b1", "name", "eth0", "enable", "1", "jumbo", "0"
    ), NULL);
    tnotnull(item);
    ttrue(dbFieldBool(item, "enable"));
    tfalse(dbFieldBool(item, "jumbo"));

    item = dbCreate(db, "Port", DB_PROPS(
        "id", "b2", "name", "eth1", "enable", "maybe"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    dbClose(db);
}

/*
    Numbers are stored as json primitives and non numeric values are rejected
 */
static void testNumberCoercion()
{
    Db      *db;
    CDbItem *item;
    cchar   *str;

    db = dbOpen("./db/coercion-number.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  A quoted number is converted to a json primitive
    item = dbCreate(db, "Port", DB_JSON("{id:'n1', name:'eth0', speed:'2500'}"), NULL);
    tnotnull(item);
    teqll(dbFieldNumber(item, "speed"), 2500);
    str = dbString(item, 0);
    tcontains(str, "speed:2500");

    item = dbCreate(db, "Port", DB_PROPS(
        "id", "n2", "name", "eth1", "speed", "fast"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    dbClose(db);
}


static void testNumberGrammar()
{
    Db      *db;
    CDbItem *item;
    Json    *json;
    cchar   *bad[] = { "-", "--5", "1-2", "1e", "+-", "-.", "+5", "01", "1.", ".5", "1 2", "true", 0 };
    cchar   *good[] = { "0", "-7", "42", "3.25", "-0.5", "1e3", "2E-2", 0 };
    char    id[16];
    int     i;

    db = dbOpen("./db/coercion-number-grammar.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    for (i = 0; bad[i]; i++) {
        item = dbCreate(db, "Port", DB_PROPS("id", SFMT(id, "b%d", i), "name", "eth0", "speed", bad[i]), NULL);
        tnull(item, "Number field accepted \"%s\"", bad[i]);
    }
    for (i = 0; good[i]; i++) {
        item = dbCreate(db, "Port", DB_PROPS("id", SFMT(id, "g%d", i), "name", "eth0", "speed", good[i]), NULL);
        tnotnull(item, "Number field rejected \"%s\"", good[i]);
        if (item) {
            json = jsonParse(dbString(item, JSON_JSON), JSON_STRICT_PARSE);
            tnotnull(json);
            jsonFree(json);
        }
    }
    item = dbCreate(db, "Port", DB_PROPS("id", "g0", "name", "eth0"), NULL);
    tnull(dbUpdate(db, "Port", DB_PROPS("id", "g0", "speed", "1-2"), NULL));
    dbClose(db);
}


static void testNumericDateRange()
{
    Db      *db;
    CDbItem *item;
    cchar   *bad[] = { "99999999999999999999", "9223372036854775807", "253402300800000", "-1", "1.5", "1e3", 0 };
    char    id[16];
    int     i;

    db = dbOpen("./db/coercion-date-range.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    for (i = 0; bad[i]; i++) {
        item = dbCreate(db, "Event", DB_PROPS(
            "id", SFMT(id, "r%d", i),
            "message", "range",
            "subject", "Dates",
            "source", "test",
            "severity", "info",
            "expires", bad[i]
        ), NULL);
        tnull(item, "Date field accepted \"%s\"", bad[i]);
    }

    //  The latest acceptable date and the epoch are stored canonically
    item = dbCreate(db, "Event", DB_PROPS(
        "id", "max", "message", "range", "subject", "Dates", "source", "test", "severity", "info",
        "expires", "253402300799999"
    ), NULL);
    tnotnull(item);
    tmatch(dbField(item, "expires"), "9999-12-31T23:59:59.999Z");

    item = dbCreate(db, "Event", DB_PROPS(
        "id", "epoch", "message", "range", "subject", "Dates", "source", "test", "severity", "info",
        "expires", "0"
    ), NULL);
    tnotnull(item);
    tmatch(dbField(item, "expires"), "1970-01-01T00:00:00.000Z");

    //  A past numeric date expires on time
    teqi(dbRemoveExpired(db, 0), 1);
    tnull(dbGet(db, "Event", DB_PROPS("id", "epoch"), NULL));
    tnotnull(dbGet(db, "Event", DB_PROPS("id", "max"), NULL));
    dbClose(db);
}


static void testContainerValues()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/coercion-container.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    tnull(dbCreate(db, "Port", DB_JSON("{id:'c1', name:{nested:{deep:'x'}}}"), NULL));
    tnull(dbGet(db, "Port", DB_PROPS("id", "c1"), NULL));
    tnull(dbCreate(db, "Port", DB_JSON("{id:'c2', name:['a','b']}"), NULL));
    tnull(dbCreate(db, "Port", DB_JSON("{id:'c3', speed:{n:1}}"), NULL));
    tnull(dbCreate(db, "Port", DB_JSON("{id:'c4', enable:[true]}"), NULL));
    tnull(dbCreate(db, "Event", DB_JSON("{id:'c5', message:'m', source:'s', severity:'info', subject:'j',"
                                        " expires:{a:1}}"), NULL));

    item = dbCreate(db, "Port", DB_JSON("{id:'c6', name:'eth0'}"), NULL);
    tnotnull(item);
    tnull(dbUpdate(db, "Port", DB_JSON("{id:'c6', name:{deep:'x'}}"), NULL));
    item = dbGet(db, "Port", DB_PROPS("id", "c6"), NULL);
    tmatch(dbField(item, "name"), "eth0");
    tnull(dbField(item, "deep"));
    dbClose(db);
}

/*
    Unquoted strings are converted to json strings
 */
static void testStringCoercion()
{
    Db      *db;
    CDbItem *item;
    cchar   *str;

    db = dbOpen("./db/coercion-string.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Port", DB_JSON("{id:'s1', name:42}"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "42");
    str = dbString(item, 0);
    tcontains(str, "name:'42'");

    dbClose(db);
}

/*
    An undefined value is left untouched and unknown properties are dropped
 */
static void testUndefinedAndUnknown()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/coercion-undefined.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Port", DB_JSON("{id:'u1', name:'eth0', speed:undefined}"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "eth0");

    //  A property that is not in the model is silently dropped
    item = dbCreate(db, "Port", DB_PROPS(
        "id", "u2", "name", "eth1", "notAField", "dropped"
    ), NULL);
    tnotnull(item);
    tnull(dbField(item, "notAField"));

    dbClose(db);
}

/*
    Enum checking requires an exact match against a listed value
 */
static void testEnumChecking()
{
    Db      *db;
    CDbItem *item;
    char    *id;

    db = dbOpen("./db/coercion-enum.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "e1", "email", "e1@test.com", "role", "admin"
    ), NULL);
    tnotnull(item);

    id = sclone(dbField(item, "id"));

    //  A value outside the enum is rejected
    item = dbCreate(db, "User", DB_PROPS(
        "username", "e2", "email", "e2@test.com", "role", "wizard"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    //  A prefix of a listed value is not a match
    item = dbCreate(db, "User", DB_PROPS(
        "username", "e3", "email", "e3@test.com", "role", "adm"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    //  A suffix of a listed value is not a match either
    item = dbCreate(db, "User", DB_PROPS(
        "username", "e4", "email", "e4@test.com", "role", "min"
    ), NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    //  Enums are enforced on update as well as create
    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "wizard"), NULL);
    tnull(item);
    tnotnull(dbGetError(db));
    rFree(id);

    dbClose(db);
}

/*
    An incomplete sort key is only allowed for find and limited remove
 */
static void testIncompleteSortKey()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    int     count;

    db = dbOpen("./db/coercion-sortkey.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "Port", DB_PROPS("id", "k1", "name", "eth0"), NULL);
    tnotnull(item);
    item = dbCreate(db, "Port", DB_PROPS("id", "k2", "name", "eth1"), NULL);
    tnotnull(item);

    //  Find without an id does a begins-with search on the key prefix
    items = dbFind(db, "Port", NULL, NULL);
    teqi(rGetListLength(items), 2);
    rFreeList(items);

    //  Get without an id cannot resolve the key
    item = dbGet(db, "Port", NULL, NULL);
    tnull(item);
    tnotnull(dbGetError(db));

    //  Remove without an id needs a limit
    count = dbRemove(db, "Port", DB_PROPS("name", "eth0"), DB_PARAMS(.limit = 1));
    teqi(count, 1);
    count = dbRemove(db, "Port", DB_PROPS("name", "eth1"), NULL);
    tltei(count, 0);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testDateCoercion();
    testDateValidation();
    testBooleanCoercion();
    testNumberCoercion();
    testNumberGrammar();
    testNumericDateRange();
    testContainerValues();
    testStringCoercion();
    testUndefinedAndUnknown();
    testEnumChecking();
    testIncompleteSortKey();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
