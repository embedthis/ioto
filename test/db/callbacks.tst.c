/*
    callbacks.c.tst - Unit tests for database callbacks and triggers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Locals **********************************/

static int callbackCount = 0;
static cchar *lastCmd = NULL;
static cchar *lastModel = NULL;
static int selfRemoveCalls = 0;
static int pairedCalls = 0;

/************************************ Code ************************************/

static void testCallback(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd, int events)
{
    callbackCount++;
    lastCmd = cmd;
    lastModel = model ? model->name : NULL;
    tmatch(arg, "test-arg");
    tnotnull(db);
    tnotnull(item);
    tnotnull(cmd);
}

static void testCallbacks()
{
    Db          *db;
    CDbItem     *item;
    char       *id;
    int         count;
    cchar      *arg = "test-arg";

    db = dbOpen("./db/callbacks.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Add callback for all changes
    dbAddCallback(db, testCallback, NULL, (void*) arg, DB_ON_CHANGE);

    //  Reset counters
    callbackCount = 0;
    lastCmd = NULL;
    lastModel = NULL;

    //  Create item - should trigger callback
    item = dbCreate(db, "User", DB_PROPS(
        "username", "admin",
        "email", "admin@test.com",
        "role", "admin"
    ), NULL);
    tnotnull(item);
    teqi(callbackCount, 1);
    tmatch(lastCmd, "create");
    tmatch(lastModel, "User");

    id = sclone(dbField(item, "id"));
    tnotnull(id);

    //  Update item - should trigger callback
    callbackCount = 0;
    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "user"), NULL);
    tnotnull(item);
    teqi(callbackCount, 1);
    tmatch(lastCmd, "update");

    //  Remove item - should trigger callback
    callbackCount = 0;
    count = dbRemove(db, "User", DB_PROPS("id", id), NULL);
    teqi(count, 1);
    teqi(callbackCount, 1);
    tmatch(lastCmd, "remove");

    //  Remove callback
    dbRemoveCallback(db, testCallback, NULL, (void*) arg);

    //  Create another item - should not trigger callback
    callbackCount = 0;
    item = dbCreate(db, "User", DB_PROPS(
        "username", "user2",
        "email", "user2@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    teqi(callbackCount, 0);

    rFree(id);
    dbClose(db);
}

static void testModelSpecificCallbacks()
{
    Db          *db;
    CDbItem     *item;
    cchar      *arg = "test-arg";

    db = dbOpen("./db/callbacks-model.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Add callback only for User model
    dbAddCallback(db, testCallback, "User", (void*) arg, DB_ON_CHANGE);

    //  Reset counters
    callbackCount = 0;

    //  Create User item - should trigger callback
    item = dbCreate(db, "User", DB_PROPS(
        "username", "admin",
        "email", "admin@test.com",
        "role", "admin"
    ), NULL);
    tnotnull(item);
    teqi(callbackCount, 1);

    //  Create Port item - should not trigger callback
    callbackCount = 0;
    item = dbCreate(db, "Port", DB_PROPS(
        "name", "eth0",
        "enable", "true"
    ), NULL);
    tnotnull(item);
    teqi(callbackCount, 0);

    dbClose(db);
}

static void selfRemovingCallback(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params,
                                 cchar *cmd, int events)
{
    selfRemoveCalls++;
    dbRemoveCallback(db, selfRemovingCallback, NULL, arg);
}

static void pairedCallback(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                           int events)
{
    pairedCalls++;
}

/*
    Self-removal must not skip the next callback.
 */
static void testSelfRemovingCallback()
{
    Db      *db;
    CDbItem *item;
    char    *id;
    cchar   *selfArg = "self-remove";
    cchar   *pairArg = "paired";

    db = dbOpen("./db/callbacks-self-remove.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    dbAddCallback(db, selfRemovingCallback, NULL, (void*) selfArg, DB_ON_CHANGE);
    dbAddCallback(db, pairedCallback, NULL, (void*) pairArg, DB_ON_CHANGE);

    selfRemoveCalls = 0;
    pairedCalls = 0;

    item = dbCreate(db, "User", DB_PROPS(
        "username", "onlyonce",
        "email", "onlyonce@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    teqi(selfRemoveCalls, 1);
    teqi(pairedCalls, 1);

    id = sclone(dbField(item, "id"));
    tnotnull(id);

    selfRemoveCalls = 0;
    pairedCalls = 0;
    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "admin"), NULL);
    tnotnull(item);
    teqi(selfRemoveCalls, 0);
    teqi(pairedCalls, 1);

    dbRemoveCallback(db, pairedCallback, NULL, (void*) pairArg);
    rFree(id);
    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testCallbacks();
    testModelSpecificCallbacks();
    testSelfRemovingCallback();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */