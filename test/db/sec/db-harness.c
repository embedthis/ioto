/*
    db-harness.c - Database fuzz tests. See README.md for usage. Reuse fixed fixture paths and remove them before and after each call.

    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */

/********************************** Includes **********************************/

#include "harness.h"

#ifndef SEC_LIBFUZZER
#include "fuzzcore/fuzz.c"
#endif

#include <sys/stat.h>

#include "dbLib.c"

/*********************************** Locals ***********************************/

#define DBFUZZ_DIR   ".testme/dbfuzz"
#define DBFUZZ_MAXIN (64 * 1024)

static cchar *SCHEMA_PATH = "../schema.json";

static void initDirs(void)
{
    static int done = 0;

    if (!done) {
        done = 1;
        mkdir(".testme", 0755);
        mkdir(DBFUZZ_DIR, 0755);
    }
}

static void removeDbFiles(cchar *path)
{
    char jpath[512];

    unlink(path);
    snprintf(jpath, sizeof(jpath), "%s.jnl", path);
    unlink(jpath);
}

/*
    NUL-terminate fuzz input for the text APIs.
 */
static char *toCString(cchar *input, size_t len)
{
    char *buf;

    if (len > DBFUZZ_MAXIN) {
        len = DBFUZZ_MAXIN;
    }
    if ((buf = malloc(len + 1)) == 0) {
        return 0;
    }
    if (len > 0) {
        memcpy(buf, input, len);
    }
    buf[len] = '\0';
    return buf;
}

/*
    Bound diagnostic output because fuzz input is not NUL-terminated.
 */
static int clampLen(size_t len)
{
    return (int) (len < 80 ? len : 80);
}

/********************************** Oracles ***********************************/

/*
    Check item round trips and error reporting for parsed JSON properties.
 */
static Db *gPropsDb;

static void initPropsDb(void)
{
    char path[512];

    if (gPropsDb) {
        return;
    }
    initDirs();
    snprintf(path, sizeof(path), "%s/props.db", DBFUZZ_DIR);
    removeDbFiles(path);
    gPropsDb = dbOpen(path, SCHEMA_PATH, DB_OPEN_RESET);
}

static bool propsOracle(cchar *input, size_t len)
{
    Db          *db;
    Json        *props;
    char        *text, *errorMsg;
    char        idBuf[64];
    cchar       *id, *username, *username2, *email, *email2;
    CDbItem     *item, *item2;
    static long counter = 0;
    bool        ok;

    initPropsDb();
    db = gPropsDb;
    if (!db) {
        return true;
    }
    ok = true;
    if ((text = toCString(input, len)) == 0) {
        return true;
    }
    errorMsg = 0;
    props = jsonParseString(text, &errorMsg, 0);
    free(text);
    if (!props) {
        // Only parsed JSON reaches the database API.
        rFree(errorMsg);
        return true;
    }
    // Avoid collisions between repeated inputs.
    snprintf(idBuf, sizeof(idBuf), "-fz%ld", counter++);
    if ((username = jsonGet(props, 0, "username", 0)) != 0) {
        jsonSetFmt(props, 0, "username", "%s%s", username, idBuf);
    }
    item = dbCreate(db, "User", props, DB_PARAMS(.mem = 1, .index = "primary"));
    if (!item) {
        if (!dbGetError(db)) {
            fprintf(stderr,
                    "sec: db props: dbCreate failed with no error message set (input \"%.*s\")\n",
                    clampLen(len), input);
            ok = false;
        }
        jsonFree(props);
        return ok;
    }
    username = dbField(item, "username");
    email = dbField(item, "email");
    id = dbField(item, "id");
    if (!id) {
        fprintf(stderr, "sec: db props: created item has no id (input \"%.*s\")\n",
                clampLen(len), input);
        ok = false;
        jsonFree(props);
        return ok;
    }
    item2 = dbGet(db, "User", DB_PROPS("id", id), DB_PARAMS(.mem = 1, .index = "primary"));
    if (!item2) {
        fprintf(stderr, "sec: db props: created item with id \"%s\" could not be read back "
                "(input \"%.*s\")\n", id, clampLen(len), input);
        ok = false;
    } else {
        username2 = dbField(item2, "username");
        email2 = dbField(item2, "email");
        if (!smatch(username, username2)) {
            fprintf(stderr, "sec: db props: read-back username \"%s\" != created \"%s\" "
                    "(input \"%.*s\")\n", username2, username, clampLen(len), input);
            ok = false;
        }
        if (!smatch(email, email2)) {
            fprintf(stderr, "sec: db props: read-back email \"%s\" != created \"%s\" "
                    "(input \"%.*s\")\n", email2, email, clampLen(len), input);
            ok = false;
        }
    }
    // Keep the in-memory tree bounded across inputs.
    dbRemove(db, "User", DB_PROPS("id", id), DB_PARAMS(.mem = 1, .index = "primary"));
    jsonFree(props);
    return ok;
}

/*
    Check pagination limits against a fixed dataset.
 */
#define DBFUZZ_QUERY_ITEMS 5

static Db *gQueryDb;

static void initQueryDb(void)
{
    char  path[512];
    char  id[32];
    int   i;

    if (gQueryDb) {
        return;
    }
    initDirs();
    snprintf(path, sizeof(path), "%s/query.db", DBFUZZ_DIR);
    removeDbFiles(path);
    gQueryDb = dbOpen(path, SCHEMA_PATH, DB_OPEN_RESET);
    if (!gQueryDb) {
        return;
    }
    for (i = 0; i < DBFUZZ_QUERY_ITEMS; i++) {
        snprintf(id, sizeof(id), "q%d", i);
        dbCreate(gQueryDb, "Event", DB_PROPS(
                     "id", id,
                     "message", "fuzz seed event",
                     "source", "harness",
                     "severity", "info",
                     "subject", "fuzz"), DB_PARAMS(.mem = 1, .index = "primary"));
    }
}

static bool queryOracle(cchar *input, size_t len)
{
    Db     *db;
    RList  *list;
    char   *nextTok;
    int32  rawLimit;
    int    limit;
    bool   ok;

    initQueryDb();
    db = gQueryDb;
    if (!db) {
        return true;
    }
    ok = true;
    rawLimit = 0;
    if (len >= 4) {
        memcpy(&rawLimit, input, 4);
    }
    limit = (int) (rawLimit % 20);

    nextTok = len > 4 ? toCString(input + 4, len - 4) : 0;

    list = dbFind(db, "Event", NULL,
                   DB_PARAMS(.mem = 1, .index = "primary", .limit = limit, .next = nextTok));
    if (list) {
        if (list->length > DBFUZZ_QUERY_ITEMS) {
            fprintf(stderr,
                    "sec: db query: dbFind returned %d items but only %d exist "
                    "(limit %d, input \"%.*s\")\n",
                    list->length, DBFUZZ_QUERY_ITEMS, limit, clampLen(len), input);
            ok = false;
        }
        if (limit > 0 && list->length > limit) {
            fprintf(stderr,
                    "sec: db query: dbFind returned %d items but limit was %d "
                    "(input \"%.*s\")\n", list->length, limit, clampLen(len), input);
            ok = false;
        }
        rFreeList(list);
    }
    if (nextTok) {
        free(nextTok);
    }
    return ok;
}

/*
    Check journal replay with arbitrary file contents.
 */
static bool journalOracle(cchar *input, size_t len)
{
    char path[512], jpath[512];
    FILE *fp;
    Db   *db;

    initDirs();
    snprintf(path, sizeof(path), "%s/journal.db", DBFUZZ_DIR);
    snprintf(jpath, sizeof(jpath), "%s.jnl", path);
    removeDbFiles(path);

    if ((fp = fopen(jpath, "wb")) == 0) {
        return true;
    }
    if (len > 0) {
        fwrite(input, 1, len, fp);
    }
    fclose(fp);

    // DB_OPEN_RESET would delete the journal fixture.
    db = dbOpen(path, SCHEMA_PATH, 0);
    if (db) {
        dbClose(db);
    }
    removeDbFiles(path);
    return true;
}

/*
    Check schema loading with arbitrary file contents.
 */
static bool schemaOracle(cchar *input, size_t len)
{
    char  path[512], schemaPath[512];
    char  *text;
    FILE  *fp;
    Db    *db;
    bool  ok;

    initDirs();
    snprintf(path, sizeof(path), "%s/schema.db", DBFUZZ_DIR);
    snprintf(schemaPath, sizeof(schemaPath), "%s/schema-fuzz.json", DBFUZZ_DIR);
    removeDbFiles(path);

    ok = true;
    if ((text = toCString(input, len)) == 0) {
        return true;
    }
    if ((fp = fopen(schemaPath, "wb")) == 0) {
        free(text);
        return true;
    }
    fwrite(text, 1, strlen(text), fp);
    fclose(fp);

    db = dbOpen(path, schemaPath, DB_OPEN_RESET);
    if (db) {
        dbClose(db);
    }
    unlink(schemaPath);
    removeDbFiles(path);
    free(text);
    return ok;
}

#if defined(DB_ORACLE_QUERY)
    #define ORACLE      queryOracle
    #define ORACLE_NAME "query"
#elif defined(DB_ORACLE_JOURNAL)
    #define ORACLE      journalOracle
    #define ORACLE_NAME "journal"
#elif defined(DB_ORACLE_SCHEMA)
    #define ORACLE      schemaOracle
    #define ORACLE_NAME "schema"
#else
    #define ORACLE      propsOracle
    #define ORACLE_NAME "props"
#endif

/****************************** libFuzzer entry *******************************/

#ifdef SEC_LIBFUZZER
SEC_LIBFUZZER_ENTRY(ORACLE)

#else

/*********************************** main *************************************/

typedef struct {
    cchar *data;
    size_t len;
} Seed;

#define SEED(x) { x, sizeof(x) - 1 }

#if defined(DB_ORACLE_QUERY)
    static const Seed seeds[] = {
        SEED("\x00\x00\x00\x00"),                       //  limit 0, no next token
        SEED("\x01\x00\x00\x00"),                       //  limit 1
        SEED("\xff\xff\xff\xff"),                       //  limit -1
        SEED("\x64\x00\x00\x00"),                       //  limit 100 (more than exist)
        SEED("\x02\x00\x00\x00" "event#q1"),            //  limit 2, plausible next token
        SEED("\x02\x00\x00\x00" "\x00\x01\x02garbage"), //  limit 2, binary-garbage next token
        SEED(""),
    };
#elif defined(DB_ORACLE_JOURNAL)
    static const Seed seeds[] = {
        SEED(""),                                                    //  empty journal
        SEED("DB\x03\x00"),                                          //  header only, no records
        SEED("DB\xff\xff"),                                          //  bad version
        SEED("\x02\x00"),                                            //  legacy native version
        SEED("DB\x03\x00" "\xff\xff\xff\xff"),                       //  record size claims 4GB
        SEED("DB\x03\x00" "\x00\x00\x00\x00"),                       //  record size 0 (invalid)
        SEED("garbage, not a journal at all"),
    };
#elif defined(DB_ORACLE_SCHEMA)
    static const Seed seeds[] = {
        SEED("{}"),
        SEED("{ models: { Item: { id: { type: 'string', required: true } } } }"),
        SEED("{ models: { Bad: { f: { type: 'string', validate: '.*' } } } }"),
        SEED("{ models: { Bad: { f: { type: 'string', crypt: true } } } }"),
        SEED("{ models: { Ok: { f: { type: 'string', map: 'cloudName' } } } }"),
        SEED("{ indexes: { primary: { sort: 'sk' } }, models: {} }"),
        SEED("not json at all"),
        SEED("{ models: "),
        SEED(""),
    };
#else
    static const Seed seeds[] = {
        SEED("{}"),
        SEED("{\"username\":\"admin\",\"email\":\"a@example.com\",\"role\":\"admin\"}"),
        SEED("{\"username\":\"admin\",\"email\":\"a@example.com\",\"role\":\"nosuchrole\"}"),
        SEED("{\"email\":\"a@example.com\"}"),
        SEED("{\"username\":\"\",\"email\":\"\"}"),
        SEED("{\"username\":\"admin\",\"email\":\"a@example.com\",\"role\":\"admin\",\"id\":\"x\"}"),
        SEED("[1,2,3]"),
        SEED("not json"),
        SEED(""),
    };
#endif

int main(int argc, char **argv)
{
    FuzzConfig config;
    FuzzRunner *runner;
    cchar      *replay;
    int        crashes, leaked;
    size_t     s;

    (void) argc;
    (void) argv;

    rInit(0, 0);

    memset(&config, 0, sizeof(config));
    config.stop = false;
    config.verbose = false;
    // fuzzInit creates only one directory level.
    config.crashDir = "crashes";

    runner = fuzzInit(&config);
    fuzzSetOracle(runner, ORACLE);

    for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
        fuzzAddCorpus(runner, seeds[s].data, seeds[s].len);
    }
    // fuzzLoadDir does not recurse.
    fuzzLoadDir(runner, "corpus/" ORACLE_NAME);

    replay = getenv("FUZZ_REPLAY");
    if (replay) {
        printf("Replay mode: %s\n", replay);
        fuzzClearCorpus(runner);
        fuzzLoadArtifact(runner, replay);
    }
    printf("Oracle: %s\n", ORACLE_NAME);

    crashes = fuzzRun(runner);
    fuzzReport(runner);
    fuzzFree(runner);

    leaked = fuzzLeakCheck();

    return (crashes > 0 || leaked > 0) ? 1 : 0;
}

#endif /* SEC_LIBFUZZER */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */
