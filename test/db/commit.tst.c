/*
    commit.c.tst - Unit tests for delayed commits and journal rollover

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/************************************ Code ************************************/

/*
    A delayed change is held back from the journal until its commit event fires
 */
static void testDelayedCommit()
{
    Db      *db;
    CDbItem *item;
    char    *id;
    ssize   before, after;

    db = dbOpen("./db/commit-delayed.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    before = rGetFileSize("./db/commit-delayed.db.jnl");

    item = dbCreate(db, "User", DB_PROPS(
        "username", "delayed",
        "email", "delayed@test.com",
        "role", "user"
    ), DB_PARAMS(.delay = 60));
    tnotnull(item);
    id = sclone(dbField(item, "id"));

    //  Nothing is written to the journal while the change is pending
    teqll(rGetFileSize("./db/commit-delayed.db.jnl"), before);

    //  A second change to the same item with a shorter delay brings the commit forward
    item = dbUpdate(db, "User", DB_PROPS("id", id, "role", "admin"), DB_PARAMS(.delay = 20));
    tnotnull(item);
    teqll(rGetFileSize("./db/commit-delayed.db.jnl"), before);

    rSleep(120);
    rRunEvents();

    after = rGetFileSize("./db/commit-delayed.db.jnl");
    tgtll(after, before);

    rFree(id);
    dbClose(db);
}

/*
    With several changes outstanding, commitChange writes those that are due and
    reschedules itself for the rest
 */
static void testStaggeredCommits()
{
    Db      *db;
    CDbItem *item;
    ssize   before, middle, after;

    db = dbOpen("./db/commit-staggered.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    before = rGetFileSize("./db/commit-staggered.db.jnl");

    item = dbCreate(db, "User", DB_PROPS(
        "username", "soon",
        "email", "soon@test.com",
        "role", "user"
    ), DB_PARAMS(.delay = 20));
    tnotnull(item);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "later",
        "email", "later@test.com",
        "role", "user"
    ), DB_PARAMS(.delay = 400));
    tnotnull(item);

    rSleep(120);
    rRunEvents();

    //  The first change is committed, the second is still pending
    middle = rGetFileSize("./db/commit-staggered.db.jnl");
    tgtll(middle, before);

    rSleep(400);
    rRunEvents();

    after = rGetFileSize("./db/commit-staggered.db.jnl");
    tgtll(after, middle);

    dbClose(db);
}

/*
    A pending change that is removed before its commit event fires must not be written
 */
static void testCommitAfterRemove()
{
    Db      *db;
    CDbItem *item;
    char    *id;
    int     count;

    db = dbOpen("./db/commit-removed.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "transient",
        "email", "transient@test.com",
        "role", "user"
    ), DB_PARAMS(.delay = 60));
    tnotnull(item);
    id = sclone(dbField(item, "id"));

    count = dbRemove(db, "User", DB_PROPS("id", id), NULL);
    teqi(count, 1);

    rSleep(120);
    rRunEvents();

    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnull(item);

    rFree(id);
    dbClose(db);
}

/*
    A change made with an immediate delay overrides the model delay and is written at once
 */
static void testNoDelayOverride()
{
    Db      *db;
    CDbItem *item;
    ssize   before;

    db = dbOpen("./db/commit-nodelay.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    before = rGetFileSize("./db/commit-nodelay.db.jnl");

    item = dbCreate(db, "User", DB_PROPS(
        "username", "immediate",
        "email", "immediate@test.com",
        "role", "user"
    ), DB_PARAMS(.delay = DB_NODELAY));
    tnotnull(item);
    tgtll(rGetFileSize("./db/commit-nodelay.db.jnl"), before);

    //  An in-memory change is never written to the journal
    before = rGetFileSize("./db/commit-nodelay.db.jnl");
    item = dbCreate(db, "User", DB_PROPS(
        "username", "memonly",
        "email", "memonly@test.com",
        "role", "user"
    ), DB_PARAMS(.mem = true));
    tnotnull(item);
    teqll(rGetFileSize("./db/commit-nodelay.db.jnl"), before);

    dbClose(db);
}

/*
    Exceeding the maximum journal size saves the database and starts a fresh journal
 */
static void testJournalRollover()
{
    Db      *db;
    CDbItem *item;
    char    ubuf[80];
    ssize   size;
    int     i;

    db = dbOpen("./db/commit-rollover.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  A tiny maximum size makes every change roll the journal over
    dbSetJournalParams(db, 5 * TPS, 128);

    for (i = 0; i < 20; i++) {
        item = dbCreate(db, "User", DB_PROPS(
            "username", SFMT(ubuf, "roll%d", i),
            "email", "roll@test.com",
            "role", "user"
        ), NULL);
        tnotnull(item);
    }
    //  The database has been saved and the journal restarted, so it stays small
    size = rGetFileSize("./db/commit-rollover.db.jnl");
    tltll(size, 1024);
    tgtll(rGetFileSize("./db/commit-rollover.db"), 0);

    dbClose(db);

    //  All items survive
    db = dbOpen("./db/commit-rollover.db", "./schema.json", 0);
    tnotnull(db);
    item = dbFindOne(db, "User", DB_PROPS("username", "roll19"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    Expiring items with notification writes remove records through the journal
 */
static void testExpiryNotification()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    char    idbuf[80];
    char    *expires;
    int     count, i;

    db = dbOpen("./db/commit-expiry.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    dbSetJournalParams(db, 5 * TPS, 64);

    expires = rGetIsoDate(rGetTime() - TPS);
    for (i = 0; i < 5; i++) {
        item = dbCreate(db, "Event", DB_PROPS(
            "message", "expired event",
            "subject", "Expiry",
            "source", "test",
            "severity", "info",
            "expires", expires,
            "id", SFMT(idbuf, "exp-%d", i)
        ), NULL);
        tnotnull(item);
    }
    rFree(expires);

    count = dbRemoveExpired(db, 1);
    teqi(count, 5);

    items = dbFind(db, "Event", NULL, NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);

    dbClose(db);
}

/*
    Commit callback that schedules further delayed writes, growing the change hash while
    commitChange is walking it
 */
static int spawned;
static int committed;

static void onCommit(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params,
                     cchar *cmd, int event)
{
    char ubuf[80];
    int  i;

    committed++;
    /*
        Add more than are being retired so the pending set outgrows the hash's slots while
        commitChange is still walking it
     */
    for (i = 0; i < 3 && spawned < 60; i++) {
        spawned++;
        dbCreate(db, "User", DB_PROPS(
            "username", SFMT(ubuf, "spawned%d", spawned),
            "email", "spawned@test.com",
            "role", "user"
        ), DB_PARAMS(.delay = 20));
    }
}

/*
    A callback may write to the database from inside a commit.
    Every delayed change must still be committed exactly once.
 */
static void testReentrantCommit()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    char    ubuf[80];
    int     i, total;

    db = dbOpen("./db/commit-reentrant.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    spawned = 0;
    committed = 0;
    dbAddCallback(db, (DbCallbackProc) onCommit, "User", NULL, DB_ON_COMMIT);

    for (i = 0; i < 20; i++) {
        item = dbCreate(db, "User", DB_PROPS(
            "username", SFMT(ubuf, "seed%d", i),
            "email", "seed@test.com",
            "role", "user"
        ), DB_PARAMS(.delay = 20));
        tnotnull(item);
    }

    //  Drain until everything has settled
    for (i = 0; i < 25; i++) {
        rSleep(40);
        rRunEvents();
    }
    tgti(spawned, 0);

    //  Each item is created once with a delay, so each must commit exactly once
    total = 20 + spawned;
    teqi(committed, total);

    items = dbFind(db, "User", NULL, NULL);
    teqi(rGetListLength(items), total);
    rFreeList(items);

    dbRemoveCallback(db, (DbCallbackProc) onCommit, "User", NULL);
    dbClose(db);

    //  All of them survived to disk
    db = dbOpen("./db/commit-reentrant.db", "./schema.json", 0);
    tnotnull(db);
    items = dbFind(db, "User", NULL, NULL);
    teqi(rGetListLength(items), total);
    rFreeList(items);
    dbClose(db);
}

/*
    A delayed change for an item that is removed without notification is discarded
 */
static void testOrphanedChange()
{
    Db      *db;
    CDbItem *item;
    char    idbuf[80];
    char    *expires;
    int     count, i;

    db = dbOpen("./db/commit-orphan.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  Expiring items removes them from the tree without going through change()
    expires = rGetIsoDate(rGetTime() - TPS);
    for (i = 0; i < 4; i++) {
        item = dbCreate(db, "Event", DB_PROPS(
            "message", "stale event",
            "subject", "Expiry",
            "source", "test",
            "severity", "info",
            "expires", expires,
            "id", SFMT(idbuf, "orph-%d", i)
        ), DB_PARAMS(.delay = 20));
        tnotnull(item);
    }
    rFree(expires);

    count = dbRemoveExpired(db, 0);
    teqi(count, 4);

    //  The pending changes can never commit, so they are dropped rather than retained
    rSleep(60);
    rRunEvents();
    rSleep(60);
    rRunEvents();

    dbClose(db);
}

static int  commitsA, commitsB, commitWrites;
static char *idA, *idB;

/*
    Commit another pending item from the callback to test invalidation of the due list.
 */
static void onCommitWriteOther(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                               int events)
{
    CDbItem *updated;
    cchar   *id, *other;

    id = dbField(item, "id");
    if (smatch(id, idA)) {
        commitsA++;
    } else if (smatch(id, idB)) {
        commitsB++;
    }
    if (events == DB_ON_COMMIT && commitWrites++ == 0) {
        other = smatch(id, idA) ? idB : idA;
        updated = dbUpdate(db, "User", DB_PROPS("id", other, "role", "admin"), DB_PARAMS(.delay = DB_NODELAY));
        tnotnull(updated);
    }
}

static void testCommitCallbackWritesDueItem()
{
    Db      *db;
    CDbItem *item;

    db = dbOpen("./db/commit-write-due.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS("username", "alpha", "email", "a@test.com", "role", "user"),
                    DB_PARAMS(.delay = 20));
    tnotnull(item);
    idA = sclone(dbField(item, "id"));
    item = dbCreate(db, "User", DB_PROPS("username", "beta", "email", "b@test.com", "role", "user"),
                    DB_PARAMS(.delay = 20));
    tnotnull(item);
    idB = sclone(dbField(item, "id"));

    commitsA = commitsB = commitWrites = 0;
    dbAddCallback(db, (DbCallbackProc) onCommitWriteOther, "User", NULL, DB_ON_COMMIT);
    rSleep(60);
    rRunEvents();
    rSleep(60);
    rRunEvents();

    //  Each item commits exactly once: the immediately written one is not committed again
    teqi(commitsA, 1);
    teqi(commitsB, 1);

    dbRemoveCallback(db, (DbCallbackProc) onCommitWriteOther, "User", NULL);
    dbClose(db);
    rFree(idA);
    rFree(idB);
}

static int commitsSelf;

/*
    On the first commit, make a further delayed change to the same item
 */
static void onCommitDelaySelf(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                              int events)
{
    CDbItem *updated;

    if (commitsSelf++ == 0) {
        updated = dbUpdate(db, "User", DB_PROPS("id", dbField(item, "id"), "role", "admin"), DB_PARAMS(.delay = 20));
        tnotnull(updated);
    }
}

static void testDelayedWriteDuringCommit()
{
    Db      *db;
    CDbItem *item;
    char    *id;
    int     i;

    db = dbOpen("./db/commit-delay-self.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS("username", "gamma", "email", "g@test.com", "role", "user"),
                    DB_PARAMS(.delay = 20));
    tnotnull(item);
    id = sclone(dbField(item, "id"));

    commitsSelf = 0;
    dbAddCallback(db, (DbCallbackProc) onCommitDelaySelf, "User", NULL, DB_ON_COMMIT);
    for (i = 0; i < 4; i++) {
        rSleep(40);
        rRunEvents();
    }
    //  The change made during the first commit is committed in its own right, not discarded
    teqi(commitsSelf, 2);
    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);
    tmatch(dbField(item, "role"), "admin");
    tfalse(item->delayed);

    dbRemoveCallback(db, (DbCallbackProc) onCommitDelaySelf, "User", NULL);
    dbClose(db);
    rFree(id);
}

/*
    A read only database never writes a journal or a database file
 */
static void testReadOnly()
{
    Db      *db;
    CDbItem *item;
    char    *id;
    int     rc;

    db = dbOpen("./db/commit-readonly.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "User", DB_PROPS(
        "username", "frozen",
        "email", "frozen@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    id = sclone(dbField(item, "id"));
    dbClose(db);

    db = dbOpen("./db/commit-readonly.db", "./schema.json", DB_READ_ONLY);
    tnotnull(db);
    tfalse(rFileExists("./db/commit-readonly.db.jnl"));

    //  Existing data is readable
    item = dbGet(db, "User", DB_PROPS("id", id), NULL);
    tnotnull(item);

    //  Saves are silently ignored
    rc = dbSave(db, NULL);
    teqi(rc, 0);
    tfalse(rFileExists("./db/commit-readonly.db.jnl"));

    dbClose(db);
    rFree(id);
}

/*
    Saving to an alternate path leaves the original database and journal untouched
 */
static void testSaveToPath()
{
    Db      *db;
    CDbItem *item;
    int     rc;

    db = dbOpen("./db/commit-savepath.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "copy",
        "email", "copy@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);

    rc = dbSave(db, "./db/commit-savecopy.db");
    teqi(rc, 0);
    tgtll(rGetFileSize("./db/commit-savecopy.db"), 0);

    //  A save to an unwritable directory reports an error
    rc = dbSave(db, "./db/no-such-dir/x.db");
    tltei(rc, 0);

    dbClose(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testDelayedCommit();
    testStaggeredCommits();
    testCommitAfterRemove();
    testNoDelayOverride();
    testJournalRollover();
    testExpiryNotification();
    testReentrantCommit();
    testOrphanedChange();
    testCommitCallbackWritesDueItem();
    testDelayedWriteDuringCommit();
    testReadOnly();
    testSaveToPath();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
