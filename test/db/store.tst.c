/*
    store.c.tst - Unit tests for the on-disk record store and its corruption handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "db.h"

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
    Write a database file header followed by whatever the caller adds
 */
static FILE *openStore(cchar *path, uint16 version)
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

static void putField(FILE *fp, cchar *str)
{
    putLength(fp, (uint32) slen(str));
    fwrite(str, slen(str), 1, fp);
}

/*
    Write a version 2 record, with lengths in the host byte order or the opposite one
 */
static void putLegacyField(FILE *fp, cchar *str, bool swapped)
{
    uint32 length;

    length = (uint32) slen(str);
    if (swapped) {
        length = (length << 24) | ((length & 0xFF00) << 8) | ((length >> 8) & 0xFF00) | (length >> 24);
    }
    fwrite(&length, sizeof(length), 1, fp);
    fwrite(str, slen(str), 1, fp);
}

static FILE *openLegacyStore(cchar *path, bool swapped)
{
    FILE   *fp;
    uint16 version;

    fp = fopen(path, "wb");
    tnotnull(fp);
    version = 2;
    if (swapped) {
        version = (uint16) ((version << 8) | (version >> 8));
    }
    fwrite(&version, sizeof(version), 1, fp);
    return fp;
}

static int countItems(Db *db, cchar *model)
{
    RList *items;
    int   count;

    items = dbFind(db, model, NULL, NULL);
    count = rGetListLength(items);
    rFreeList(items);
    return count;
}

/*
    A populated database survives a save and reload with every field intact
 */
static void testRoundTrip()
{
    Db      *db;
    CDbItem *item;
    char    ibuf[32], nbuf[32];
    int     i, rc;

    db = dbOpen("./db/store-round.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    for (i = 0; i < 50; i++) {
        item = dbCreate(db, "Port", DB_PROPS(
            "id", SFMT(ibuf, "p%03d", i),
            "name", SFMT(nbuf, "eth%d", i),
            "speed", "1000"
        ), NULL);
        tnotnull(item);
    }
    rc = dbSave(db, NULL);
    teqi(rc, 0);
    dbClose(db);

    db = dbOpen("./db/store-round.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 50);

    item = dbGet(db, "Port", DB_PROPS("id", "p049"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "eth49");
    teqll(dbFieldNumber(item, "speed"), 1000);
    tmatch(dbField(item, "duplex"), "full");

    dbClose(db);
}

/*
    A record whose key length exceeds the maximum stops the load at that point
 */
static void testOversizedKey()
{
    Db     *db;
    FILE   *fp;

    dbReset("./db/store-bigkey.db");
    fp = openStore("./db/store-bigkey.db", DB_VERSION);
    putField(fp, "port#p1");
    putField(fp, "{sk:'port#p1',id:'p1',name:'eth0',_type:'Port'}");
    putLength(fp, DB_MAX_KEY);
    fclose(fp);

    db = dbOpen("./db/store-bigkey.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 1);
    dbClose(db);
}

/*
    A record truncated after its key length stops the load
 */
static void testTruncatedKey()
{
    Db     *db;
    FILE   *fp;

    dbReset("./db/store-shortkey.db");
    fp = openStore("./db/store-shortkey.db", DB_VERSION);
    putField(fp, "port#p1");
    putField(fp, "{sk:'port#p1',id:'p1',name:'eth0',_type:'Port'}");
    putLength(fp, 32);
    fwrite("short", 5, 1, fp);
    fclose(fp);

    db = dbOpen("./db/store-shortkey.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 1);
    dbClose(db);
}

/*
    A record whose value length exceeds the maximum stops the load
 */
static void testOversizedValue()
{
    Db     *db;
    FILE   *fp;

    dbReset("./db/store-bigvalue.db");
    fp = openStore("./db/store-bigvalue.db", DB_VERSION);
    putField(fp, "port#p1");
    putField(fp, "{sk:'port#p1',id:'p1',name:'eth0',_type:'Port'}");
    putField(fp, "port#p2");
    putLength(fp, DB_MAX_ITEM + 1);
    fclose(fp);

    db = dbOpen("./db/store-bigvalue.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 1);
    dbClose(db);
}

/*
    A record truncated inside its value stops the load
 */
static void testTruncatedValue()
{
    Db     *db;
    FILE   *fp;

    dbReset("./db/store-shortvalue.db");
    fp = openStore("./db/store-shortvalue.db", DB_VERSION);
    putField(fp, "port#p1");
    putField(fp, "{sk:'port#p1',id:'p1',name:'eth0',_type:'Port'}");
    putField(fp, "port#p2");
    putLength(fp, 128);
    fwrite("{sk:'port#p2'", 13, 1, fp);
    fclose(fp);

    db = dbOpen("./db/store-shortvalue.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 1);
    dbClose(db);
}

/*
    A database that cannot create its journal does not open
 */
static void testUnwritableLocation()
{
    Db *db;

    db = dbOpen("./db/no-such-dir/store.db", "./schema.json", 0);
    tnull(db);
}

/*
    A save whose temporary file cannot be created reports an error and keeps the data
 */
static void testUnwritableSaveFile()
{
    Db      *db;
    CDbItem *item;
    int     rc;

    dbReset("./db/store-blocked.db");
    rmdir("./db/store-blocked.db.save");

    db = dbOpen("./db/store-blocked.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "User", DB_PROPS(
        "username", "blocked",
        "email", "blocked@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);

    //  A directory in place of the temporary file makes the save fail to open it
    teqi(mkdir("./db/store-blocked.db.save", 0755), 0);

    rc = dbSave(db, NULL);
    tltei(rc, 0);
    tcontains(dbGetError(db), "Cannot open");

    //  The in-memory data is untouched
    item = dbFindOne(db, "User", DB_PROPS("username", "blocked"), NULL);
    tnotnull(item);

    rmdir("./db/store-blocked.db.save");
    dbClose(db);
    rmdir("./db/store-blocked.db.save");
}

/*
    A save that cannot restart the journal reports an error
 */
static void testUnwritableJournal()
{
    Db      *db;
    CDbItem *item;
    int     rc;

    dbReset("./db/store-nojnl.db");
    rmdir("./db/store-nojnl.db.jnl");

    db = dbOpen("./db/store-nojnl.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "User", DB_PROPS(
        "username", "nojnl",
        "email", "nojnl@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);

    /*
        Replace the journal with a directory so it cannot be recreated after the save. Windows
        refuses to unlink a file that is still open, and the database holds the journal, so the
        fixture cannot be built there. Report that rather than assert against it: the behaviour
        under test is the save's error path, not the platform's unlink semantics.
     */
    if (unlink("./db/store-nojnl.db.jnl") < 0 && rGetOsError() != ENOENT) {
        tinfo("Skipped: Cannot replace an open journal on this platform");
        dbClose(db);
        return;
    }
    teqi(mkdir("./db/store-nojnl.db.jnl", 0755), 0);

    rc = dbSave(db, NULL);
    tltei(rc, 0);
    tcontains(dbGetError(db), "journal");

    //  The database itself was written before the journal failed
    tgtll(rGetFileSize("./db/store-nojnl.db"), 0);

    rmdir("./db/store-nojnl.db.jnl");
    dbClose(db);
    rmdir("./db/store-nojnl.db.jnl");
}

/*
    A journal that cannot be read is reported rather than silently skipped
 */
static void testUnreadableJournal()
{
    Db   *db;
    FILE *fp;

    dbReset("./db/store-badjnl.db");
    unlink("./db/store-badjnl.db.jnl/filler");
    rmdir("./db/store-badjnl.db.jnl");

    //  A directory has a non zero size, so the journal looks present but cannot be read
    teqi(mkdir("./db/store-badjnl.db.jnl", 0755), 0);
    fp = fopen("./db/store-badjnl.db.jnl/filler", "wb");
    tnotnull(fp);
    fwrite("filler", 6, 1, fp);
    fclose(fp);

    db = dbOpen("./db/store-badjnl.db", "./schema.json", 0);
    tnull(db);

    unlink("./db/store-badjnl.db.jnl/filler");
    rmdir("./db/store-badjnl.db.jnl");
}

/*
    A failed save must not destroy the journal. The journal is the only record of the
    changes the save was supposed to checkpoint.
 */
static void testFailedSaveRetainsJournal()
{
    Db      *db;
    CDbItem *item;
    ssize   before;

    dbReset("./db/store-retain.db");
    rmdir("./db/store-retain.db.save");

    db = dbOpen("./db/store-retain.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    item = dbCreate(db, "User", DB_PROPS(
        "username", "retained",
        "email", "retained@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);

    before = rGetFileSize("./db/store-retain.db.jnl");
    tgtll(before, 0);

    //  Block the save, then close. The journal must survive.
    teqi(mkdir("./db/store-retain.db.save", 0755), 0);
    dbClose(db);
    tgtll(rGetFileSize("./db/store-retain.db.jnl"), 0);

    //  With the save unblocked, reopening replays the retained journal
    teqi(rmdir("./db/store-retain.db.save"), 0);
    db = dbOpen("./db/store-retain.db", "./schema.json", 0);
    tnotnull(db);
    item = dbFindOne(db, "User", DB_PROPS("username", "retained"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    A save that fails while checkpointing a replayed journal must keep that journal too
 */
static void testFailedReplaySaveRetainsJournal()
{
    Db      *db;
    CDbItem *item;
    char    *jnl;
    size_t  len;

    dbReset("./db/store-replay.db");
    rmdir("./db/store-replay.db.save");

    db = dbOpen("./db/store-replay.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);
    tnotnull(dbCreate(db, "User", DB_PROPS(
        "username", "replayed", "email", "r@test.com", "role", "user"), NULL));
    jnl = rReadFile("./db/store-replay.db.jnl", &len);
    tnotnull(jnl);
    dbClose(db);

    //  Simulate a crash, then block the checkpoint save that the replay triggers
    unlink("./db/store-replay.db");
    tgtll((int64) rWriteFile("./db/store-replay.db.jnl", jnl, len, 0600), 0);
    rFree(jnl);
    teqi(mkdir("./db/store-replay.db.save", 0755), 0);

    db = dbOpen("./db/store-replay.db", "./schema.json", 0);
    tnotnull(db);
    item = dbFindOne(db, "User", DB_PROPS("username", "replayed"), NULL);
    tnotnull(item);

    //  The journal still holds the only copy, so it must not have been truncated
    tgtll(rGetFileSize("./db/store-replay.db.jnl"), 0);
    dbClose(db);
    tgtll(rGetFileSize("./db/store-replay.db.jnl"), 0);

    teqi(rmdir("./db/store-replay.db.save"), 0);
    db = dbOpen("./db/store-replay.db", "./schema.json", 0);
    tnotnull(db);
    item = dbFindOne(db, "User", DB_PROPS("username", "replayed"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    A sort key too long to be read back is refused before it enters the tree
 */
static void testOversizeKeyRejected()
{
    Db      *db;
    CDbItem *item;
    char    *big;
    int     rc;

    db = dbOpen("./db/store-bigkey-api.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    big = rAlloc(DB_MAX_KEY + 100);
    memset(big, 'k', DB_MAX_KEY + 99);
    big[DB_MAX_KEY + 99] = '\0';

    tnotnull(dbCreate(db, "Port", DB_PROPS("id", "small", "name", "eth0"), NULL));

    item = dbCreate(db, "Port", DB_PROPS("id", big, "name", "huge"), NULL);
    tnull(item);
    tcontains(dbGetError(db), "Sort key exceeds");
    rFree(big);

    //  The database is still savable and the good item survives
    rc = dbSave(db, NULL);
    teqi(rc, 0);
    teqi(countItems(db, "Port"), 1);
    dbClose(db);

    db = dbOpen("./db/store-bigkey-api.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 1);
    dbClose(db);
}

/*
    A write that would make an item too large to store or journal is refused at the API and
    leaves both the database and the item untouched
 */
static void testOversizeValueRejected()
{
    Db      *db;
    CDbItem *item;
    char    *big, *jnl;
    size_t  len;

    db = dbOpen("./db/store-bigvalue-api.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    big = rAlloc(DB_MAX_ITEM + 1000);
    memset(big, 'v', DB_MAX_ITEM + 999);
    big[DB_MAX_ITEM + 999] = '\0';

    item = dbCreate(db, "User", DB_PROPS("id", "aaa", "username", "first", "email", "f@test.com"), NULL);
    tnotnull(item);

    //  An oversize create is refused
    item = dbCreate(db, "User", DB_PROPS("id", "mmm", "username", "huge", "email", big), NULL);
    tnull(item);
    tcontains(dbGetError(db), "exceeds the maximum size");
    item = dbGet(db, "User", DB_PROPS("id", "mmm"), NULL);
    tnull(item);

    //  An oversize update and field set are refused and the item keeps its old values
    item = dbUpdate(db, "User", DB_PROPS("id", "aaa", "email", big), NULL);
    tnull(item);
    tcontains(dbGetError(db), "exceeds the maximum size");
    item = dbSetField(db, "User", "email", big, DB_PROPS("id", "aaa"), NULL);
    tnull(item);
    tcontains(dbGetError(db), "exceeds the maximum size");
    item = dbGet(db, "User", DB_PROPS("id", "aaa"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "email"), "f@test.com");

    //  An oversize upsert of a new item is refused
    item = dbUpdate(db, "User", DB_PROPS("id", "nnn", "username", "new", "email", big), DB_PARAMS(.upsert = 1));
    tnull(item);
    item = dbGet(db, "User", DB_PROPS("id", "nnn"), NULL);
    tnull(item);

    item = dbCreate(db, "User", DB_PROPS("id", "zzz", "username", "last", "email", "l@test.com"), NULL);
    tnotnull(item);
    rFree(big);

    //  A valid update after the refusals still journals and the save is clean
    item = dbUpdate(db, "User", DB_PROPS("id", "aaa", "role", "user"), NULL);
    tnotnull(item);
    jnl = rReadFile("./db/store-bigvalue-api.db.jnl", &len);
    tnotnull(jnl);
    teqi(dbSave(db, NULL), 0);
    tnull(dbGetError(db));
    dbClose(db);

    //  Simulate a crash so recovery comes entirely from the journal
    unlink("./db/store-bigvalue-api.db");
    tgtll((int64) rWriteFile("./db/store-bigvalue-api.db.jnl", jnl, len, 0600), 0);
    rFree(jnl);

    db = dbOpen("./db/store-bigvalue-api.db", "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "User"), 2);
    item = dbGet(db, "User", DB_PROPS("id", "aaa"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "role"), "user");
    item = dbFindOne(db, "User", DB_PROPS("username", "last"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    The journal age timer saves the database in the background
 */
static void testJournalAgeSave()
{
    Db      *db;
    CDbItem *item;
    ssize   before;

    db = dbOpen("./db/store-age.db", "./schema.json", DB_OPEN_RESET);
    tnotnull(db);

    //  A short age with a large size so the timer, not the size, triggers the save
    dbSetJournalParams(db, 200, 1024 * 1024);

    item = dbCreate(db, "User", DB_PROPS(
        "username", "aged",
        "email", "aged@test.com",
        "role", "user"
    ), NULL);
    tnotnull(item);
    before = rGetFileSize("./db/store-age.db");

    rSleep(300);
    rRunEvents();

    //  The scheduled save has written the database
    tgtll(rGetFileSize("./db/store-age.db"), before);
    dbClose(db);

    db = dbOpen("./db/store-age.db", "./schema.json", 0);
    tnotnull(db);
    item = dbFindOne(db, "User", DB_PROPS("username", "aged"), NULL);
    tnotnull(item);
    dbClose(db);
}

/*
    Load a version 2 database, which has no magic and host byte order lengths, and check that a
    save rewrites it in the portable format
 */
static void loadLegacyStore(cchar *path, bool swapped)
{
    Db      *db;
    CDbItem *item;
    FILE    *fp;
    uchar   header[4];
    int     count;

    dbReset(path);
    fp = openLegacyStore(path, swapped);
    putLegacyField(fp, "port#p1", swapped);
    putLegacyField(fp, "{sk:'port#p1',id:'p1',name:'eth0',_type:'Port'}", swapped);
    putLegacyField(fp, "port#p2", swapped);
    putLegacyField(fp, "{sk:'port#p2',id:'p2',name:'eth1',_type:'Port'}", swapped);
    fclose(fp);

    db = dbOpen(path, "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 2);
    item = dbGet(db, "Port", DB_PROPS("id", "p2"), NULL);
    tnotnull(item);
    tmatch(dbField(item, "name"), "eth1");
    teqi(dbSave(db, NULL), 0);
    dbClose(db);

    fp = fopen(path, "rb");
    tnotnull(fp);
    count = (int) fread(header, sizeof(header), 1, fp);
    fclose(fp);
    teqi(count, 1);
    teqi(header[0], 'D');
    teqi(header[1], 'B');
    teqi(header[2], DB_VERSION);

    db = dbOpen(path, "./schema.json", 0);
    tnotnull(db);
    teqi(countItems(db, "Port"), 2);
    dbClose(db);
}

static void testLegacyStoreLoads()
{
    loadLegacyStore("./db/store-legacy.db", 0);
}

/*
    A version 2 database written by a host of the opposite byte order is read with its lengths swapped
 */
static void testSwappedLegacyStoreLoads()
{
    loadLegacyStore("./db/store-legacy-swapped.db", 1);
}

/*
    A file that is not a database is rejected by its header
 */
static void testGarbageStoreRejected()
{
    Db   *db;
    FILE *fp;

    dbReset("./db/store-garbage.db");
    fp = fopen("./db/store-garbage.db", "wb");
    tnotnull(fp);
    fwrite("This is not a database file\n", 28, 1, fp);
    fclose(fp);

    db = dbOpen("./db/store-garbage.db", "./schema.json", 0);
    tnull(db);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testRoundTrip();
    testOversizedKey();
    testTruncatedKey();
    testOversizedValue();
    testTruncatedValue();
    testUnwritableLocation();
    testUnwritableSaveFile();
    testUnwritableJournal();
    testUnreadableJournal();
    testFailedSaveRetainsJournal();
    testFailedReplaySaveRetainsJournal();
    testOversizeKeyRejected();
    testOversizeValueRejected();
    testLegacyStoreLoads();
    testSwappedLegacyStoreLoads();
    testGarbageStoreRejected();
    testJournalAgeSave();

    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
