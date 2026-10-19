/*
    journal.c.tst - Unit tests for journal replay after an unclean shutdown

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

/*********************************** Locals ***********************************/

#define DBPATH  "./db/journal.db"
#define JNLPATH "./db/journal.db.jnl"

/************************************ Code ************************************/

/*
    Write a little endian record length, as the portable format does
 */
static void putLength(FILE *fp, uint32 length)
{
    uchar buf[4];

    buf[0] = length & 0xFF;
    buf[1] = (length >> 8) & 0xFF;
    buf[2] = (length >> 16) & 0xFF;
    buf[3] = (length >> 24) & 0xFF;
    fwrite(buf, sizeof(buf), 1, fp);
}

/*
    Write a journal block: the length including the trailing null, then the string
 */
static void putBlock(FILE *fp, cchar *str)
{
    uint32 len;

    len = (uint32) (slen(str) + 1);
    putLength(fp, len);
    fwrite(str, (size_t) len, 1, fp);
}

/*
    Write a complete journal change record of three null terminated strings
 */
static void putRecord(FILE *fp, cchar *cmd, cchar *model, cchar *value)
{
    putLength(fp, (uint32) (slen(cmd) + slen(model) + slen(value) + 3));
    putBlock(fp, cmd);
    putBlock(fp, model);
    putBlock(fp, value);
}

static FILE *openJournal(cchar *path, uint16 version)
{
    FILE  *fp;
    uchar header[4];

    fp = fopen(path, "wb");
    tnotnull(fp);
    header[0] = 'D';
    header[1] = 'B';
    header[2] = version & 0xFF;
    header[3] = (version >> 8) & 0xFF;
    fwrite(header, sizeof(header), 1, fp);
    return fp;
}

/*
    Version 2 journal records: no magic and host byte order lengths
 */
static void putLegacyBlock(FILE *fp, cchar *str)
{
    uint32 len;

    len = (uint32) (slen(str) + 1);
    fwrite(&len, sizeof(len), 1, fp);
    fwrite(str, (size_t) len, 1, fp);
}

static FILE *openLegacyJournal(cchar *path, cchar *cmd, cchar *model, cchar *value)
{
    FILE   *fp;
    uint16 version;
    uint32 bufsize;

    fp = fopen(path, "wb");
    tnotnull(fp);
    version = 2;
    fwrite(&version, sizeof(version), 1, fp);
    bufsize = (uint32) (slen(cmd) + slen(model) + slen(value) + 3);
    fwrite(&bufsize, sizeof(bufsize), 1, fp);
    putLegacyBlock(fp, cmd);
    putLegacyBlock(fp, model);
    putLegacyBlock(fp, value);
    return fp;
}

/*
    Create and populate a database, then simulate a crash by preserving the journal and
    discarding the saved database. The reopen must replay the journal to recover.
 */
static void testJournalReplay()
{
    Db      *db;
    CDbItem *item;
    RList   *items;
    char    *jnl, *keepId, *dropId;
    size_t  jnlLen;
    ssize   written;
    int     count;

    db = dbOpen(DBPATH, "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "keeper",
        "email", "keeper@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    keepId = sclone(dbField(item, "id"));

    item = dbCreate(db, "User", DB_PROPS(
        "username", "dropped",
        "email", "dropped@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    dropId = sclone(dbField(item, "id"));

    //  An update and a remove so the journal holds all three change commands
    item = dbUpdate(db, "User", DB_PROPS("id", keepId, "role", "admin"), NULL);
    tnotnull(item);
    count = dbRemove(db, "User", DB_PROPS("id", dropId), NULL);
    teqi(count, 1);

    //  An upsert records an "upsert" command in the journal
    item = dbUpdate(db, "Settings", DB_PROPS("numeric", "7"), DB_PARAMS(.upsert = 1));
    tnotnull(item);

    jnl = rReadFile(JNLPATH, &jnlLen);
    tnotnull(jnl);
    tgtz(jnlLen, 0);

    //  A clean close saves the database and removes the journal
    dbClose(db);
    tfalse(rFileExists(JNLPATH));

    //  Discard the saved database and restore the journal to simulate a crash before the save
    unlink(DBPATH);
    written = rWriteFile(JNLPATH, jnl, jnlLen, 0600);
    tgtll((int64) written, 0);
    rFree(jnl);

    db = dbOpen(DBPATH, "./schema.json", 0);
    tnotnull(db);
    tmatch(dbGetError(db), NULL);

    //  The created and updated item is recovered with its update applied
    item = dbGet(db, "User", DB_PROPS("id", keepId), NULL);
    tnotnull(item);
    tmatch(dbField(item, "username"), "keeper");
    tmatch(dbField(item, "role"), "admin");

    //  The removed item stays removed
    item = dbGet(db, "User", DB_PROPS("id", dropId), NULL);
    tnull(item);

    //  The upserted item is recovered
    item = dbGet(db, "Settings", NULL, NULL);
    tnotnull(item);
    teqll(dbFieldNumber(item, "numeric"), 7);

    items = dbFind(db, "User", NULL, NULL);
    teqi(rGetListLength(items), 1);
    rFreeList(items);

    //  Replaying wrote a fresh database, so a second open needs no journal
    dbClose(db);
    db = dbOpen(DBPATH, "./schema.json", 0);
    tnotnull(db);
    item = dbGet(db, "User", DB_PROPS("id", keepId), NULL);
    tnotnull(item);
    dbClose(db);

    rFree(keepId);
    rFree(dropId);
}

static int replayCommits;

static void onReplayCommit(void *arg, Db *db, DbModel *model, DbItem *item, DbParams *params,
                           cchar *cmd, int event)
{
    replayCommits++;
}

/*
    Replayed changes were journalled and notified before the restart. Recovery must apply them
    in memory only: a model commit delay must not re-journal them, nor fire commit callbacks
    registered after the open.
 */
static void testReplayBypassesCommit()
{
    Db      *db;
    CDbItem *item;
    char    *jnl;
    size_t  jnlLen;
    ssize   written, fresh;
    int     i;

    db = dbOpen("./db/journal-replay.db", "./replay.json5", DB_OPEN_RESET);
    tnotnull(db);
    //  A recreated journal has this size until the next change is flushed to it
    fresh = rGetFileSize("./db/journal-replay.db.jnl");

    //  The model delays commits, so force this change straight to the journal
    item = dbCreate(db, "Reading", DB_PROPS("value", "21"), DB_PARAMS(.delay = DB_NODELAY));
    tnotnull(item);

    jnl = rReadFile("./db/journal-replay.db.jnl", &jnlLen);
    tnotnull(jnl);
    dbClose(db);

    //  Simulate a crash before the save
    unlink("./db/journal-replay.db");
    written = rWriteFile("./db/journal-replay.db.jnl", jnl, jnlLen, 0600);
    tgtll((int64) written, 0);
    rFree(jnl);

    db = dbOpen("./db/journal-replay.db", "./replay.json5", 0);
    tnotnull(db);
    item = dbGet(db, "Reading", NULL, NULL);
    tnotnull(item);
    teqll(dbFieldNumber(item, "value"), 21);

    replayCommits = 0;
    dbAddCallback(db, onReplayCommit, "Reading", NULL, DB_ON_COMMIT);
    //  Outlast the one second model delay
    for (i = 0; i < 7; i++) {
        rSleep(200);
        rRunEvents();
    }
    teqi(replayCommits, 0);

    //  Recovery checkpointed the database and nothing was re-journalled after it
    teqll(rGetFileSize("./db/journal-replay.db.jnl"), fresh);
    dbClose(db);
}

/*
    A journal written by an incompatible release must be rejected
 */
static void testBadJournalVersion()
{
    Db   *db;
    FILE *fp;

    dbReset("./db/journal-version.db");
    fp = openJournal("./db/journal-version.db.jnl", DB_VERSION + 99);
    putRecord(fp, "create", "User", "{username:'x'}");
    fclose(fp);

    db = dbOpen("./db/journal-version.db", "./schema.json", 0);
    tnull(db);
}

/*
    A journal truncated mid record is detected and the readable prefix is kept
 */
static void testTruncatedJournal()
{
    Db      *db;
    CDbItem *item;
    FILE    *fp;

    dbReset("./db/journal-truncated.db");
    fp = openJournal("./db/journal-truncated.db.jnl", DB_VERSION);
    putRecord(fp, "create", "User",
              "{sk:'user#t1',id:'t1',username:'trunc',email:'t@test.com',role:'user',_type:'User'}");
    //  A record size with no record following it
    putLength(fp, 64);
    fclose(fp);

    db = dbOpen("./db/journal-truncated.db", "./schema.json", 0);
    tnotnull(db);
    item = dbGet(db, "User", DB_PROPS("id", "t1"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    A block length beyond the maximum item size is corruption, not the end of the journal.
    The open must fail and leave the journal intact.
 */
static void testOversizedJournalBlock()
{
    Db    *db;
    FILE  *fp;
    int64 size;

    dbReset("./db/journal-oversize.db");
    fp = openJournal("./db/journal-oversize.db.jnl", DB_VERSION);
    putLength(fp, 64);
    putLength(fp, DB_MAX_ITEM + 1);
    fclose(fp);
    size = rGetFileSize("./db/journal-oversize.db.jnl");

    db = dbOpen("./db/journal-oversize.db", "./schema.json", 0);
    tnull(db);
    teqll(rGetFileSize("./db/journal-oversize.db.jnl"), size);
}

/*
    A record size that writeChangeToJournal could never have written is corruption. The open
    must fail and preserve the journal, including the records after the bad one, rather than
    stopping the replay and recreating the journal over them.
 */
static void testOversizedJournalRecord()
{
    Db      *db;
    CDbItem *item;
    FILE    *fp;
    int64   size;

    dbReset("./db/journal-record.db");
    fp = openJournal("./db/journal-record.db.jnl", DB_VERSION);
    putRecord(fp, "create", "User",
              "{sk:'user#r1',id:'r1',username:'first',email:'r1@test.com',role:'user',_type:'User'}");
    //  A record size that cannot be allocated
    putLength(fp, 0xFFFFFFFF);
    putRecord(fp, "create", "User",
              "{sk:'user#r2',id:'r2',username:'second',email:'r2@test.com',role:'user',_type:'User'}");
    fclose(fp);
    size = rGetFileSize("./db/journal-record.db.jnl");

    db = dbOpen("./db/journal-record.db", "./schema.json", 0);
    tnull(db);
    ttrue(rFileExists("./db/journal-record.db.jnl"));
    teqll(rGetFileSize("./db/journal-record.db.jnl"), size);

    //  A record size just beyond the largest that can be written is rejected the same way
    dbReset("./db/journal-bigrecord.db");
    fp = openJournal("./db/journal-bigrecord.db.jnl", DB_VERSION);
    putLength(fp, DB_MAX_RECORD + 1);
    fclose(fp);
    size = rGetFileSize("./db/journal-bigrecord.db.jnl");

    db = dbOpen("./db/journal-bigrecord.db", "./schema.json", 0);
    tnull(db);
    teqll(rGetFileSize("./db/journal-bigrecord.db.jnl"), size);

    //  A record within the limit still replays
    dbReset("./db/journal-maxrecord.db");
    fp = openJournal("./db/journal-maxrecord.db.jnl", DB_VERSION);
    putRecord(fp, "create", "User",
              "{sk:'user#r3',id:'r3',username:'third',email:'r3@test.com',role:'user',_type:'User'}");
    fclose(fp);

    db = dbOpen("./db/journal-maxrecord.db", "./schema.json", 0);
    tnotnull(db);
    item = dbGet(db, "User", DB_PROPS("id", "r3"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    A record holding json that cannot be parsed aborts the replay
 */
static void testUnparsableJournalRecord()
{
    Db   *db;
    FILE *fp;

    dbReset("./db/journal-badjson.db");
    fp = openJournal("./db/journal-badjson.db.jnl", DB_VERSION);
    putRecord(fp, "create", "User", "{not json");
    fclose(fp);

    db = dbOpen("./db/journal-badjson.db", "./schema.json", 0);
    tnull(db);
}

/*
    A record holding no properties aborts the replay
 */
static void testEmptyJournalRecord()
{
    Db    *db;
    FILE  *fp;
    RList *items;

    dbReset("./db/journal-empty.db");
    fp = openJournal("./db/journal-empty.db.jnl", DB_VERSION);
    putRecord(fp, "create", "User", "");
    fclose(fp);

    db = dbOpen("./db/journal-empty.db", "./schema.json", 0);
    tnull(db);

    //  An empty object parses but creates nothing
    dbReset("./db/journal-noprops.db");
    fp = openJournal("./db/journal-noprops.db.jnl", DB_VERSION);
    putRecord(fp, "create", "User", "{}");
    fclose(fp);

    db = dbOpen("./db/journal-noprops.db", "./schema.json", 0);
    tnotnull(db);
    items = dbFind(db, "User", NULL, NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);
    dbClose(db);
}

/*
    An unrecognized command is skipped without aborting the replay
 */
static void testUnknownJournalCommand()
{
    Db    *db;
    FILE  *fp;
    RList *items;

    dbReset("./db/journal-unknown.db");
    fp = openJournal("./db/journal-unknown.db.jnl", DB_VERSION);
    putRecord(fp, "rename", "User", "{sk:'user#u1',id:'u1'}");
    fclose(fp);

    db = dbOpen("./db/journal-unknown.db", "./schema.json", 0);
    tnotnull(db);
    items = dbFind(db, "User", NULL, NULL);
    teqi(rGetListLength(items), 0);
    rFreeList(items);
    dbClose(db);
}

/*
    A version 2 journal, with no magic and host byte order lengths, still replays
 */
static void testLegacyJournalReplays()
{
    Db      *db;
    CDbItem *item;
    FILE    *fp;

    dbReset("./db/journal-legacy.db");
    fp = openLegacyJournal("./db/journal-legacy.db.jnl", "create", "User",
                           "{sk:'user#l1',id:'l1',username:'legacy',email:'l@test.com',role:'user',_type:'User'}");
    fclose(fp);

    db = dbOpen("./db/journal-legacy.db", "./schema.json", 0);
    tnotnull(db);
    item = dbGet(db, "User", DB_PROPS("id", "l1"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "username"), "legacy");
    dbClose(db);

    //  The recovery checkpoint wrote the portable format, which a second open reads
    db = dbOpen("./db/journal-legacy.db", "./schema.json", 0);
    tnotnull(db);
    item = dbGet(db, "User", DB_PROPS("id", "l1"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    A database file written by an incompatible release must be rejected
 */
static void testBadDatabaseVersion()
{
    Db   *db;
    FILE *fp;

    dbReset("./db/journal-dbversion.db");
    fp = openJournal("./db/journal-dbversion.db", DB_VERSION + 99);
    fclose(fp);

    db = dbOpen("./db/journal-dbversion.db", "./schema.json", 0);
    tnull(db);

    //  A database file too short to hold a version is also rejected
    dbReset("./db/journal-short.db");
    fp = fopen("./db/journal-short.db", "wb");
    tnotnull(fp);
    fclose(fp);

    db = dbOpen("./db/journal-short.db", "./schema.json", 0);
    tnull(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testJournalReplay();
    testReplayBypassesCommit();
    testBadJournalVersion();
    testTruncatedJournal();
    testOversizedJournalBlock();
    testOversizedJournalRecord();
    testUnparsableJournalRecord();
    testEmptyJournalRecord();
    testUnknownJournalCommand();
    testLegacyJournalReplays();
    testBadDatabaseVersion();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
