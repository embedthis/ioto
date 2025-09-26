/*
 * DB library Library Source
 */

#include "db.h"

#if ME_COM_DB



/********* Start of file src/dbLib.c ************/

/*
    db.c - In-memory embedded database

    The embedded database is a high performance NoSQL management document database.
    It offers JSON document items with flexible query API with efficient import and
    export of database items. The database uses fast red/black binary search indexes.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/


#include "crypt.h"

#if ME_COM_DB

/************************************ Locals ***********************************/
/*
    API query context
 */
typedef struct Env {
    Db *db;                     /* Database handle */
    RbTree *index;              /* Index to use for this query */
    DbItem search;              /* Key item to search for */
    ssize searchLen;            /* Length of key item */
    DbItem next;                /* Next item to search for */
    DbModel *model;             /* Item schema model */
    Json *props;                /* Prepared properties for the query */
    DbParams *params;           /* Supplied API parameters */
    RList *expiredItems;        /* List of expired items */
    cchar *indexSort;           /* Sort key property name */
    cchar *compare;             /* Compare operation for the query */
    bool mustMatch;             /* Must match properties or where callback */
} Env;

typedef struct DbCallback {
    DbCallbackProc proc;        /* Function callback */
    char *model;                /* Target model - if null, then all models */
    void *arg;                  /* Function argument */
    int events;                 /* Events subscribe to: DB_ON_CHANGE | DB_ON_COMMIT */
} DbCallback;

/**
    Database sync change record. One allocated for each mutation to the database.
    Changes implement a buffer cache for database changes.
 */
typedef struct DbChange {
    Db *db;
    DbModel *model;
    DbParams *params;
    cchar *cmd;
    char *key;
    Time due;                   //  When change is due to be sent
} DbChange;

#define SETUP(db, model, props, params, cmd, env) \
        setup(db, model, props, params ? params : &(DbParams) {}, cmd, env)

#define USER_ALLOC 0x1        /* User allocated json */

/************************************ Forwards *********************************/

static void addContext(Db *db, Json *props);
static DbChange *allocChange(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                             Ticks due);
static DbField *allocField(Db *db, cchar *name, Json *json, int fid);
static DbItem *allocItem(cchar *name, Json *json, char *value);
static DbModel *allocModel(Db *db, cchar *name, cchar *sync, Time delay);
static int applyChange(Db *db, cchar *cmd, cchar *model, cchar *value);
static int applyJournal(Db *db);
static bool checkEnum(DbField *field, cchar *value);
static void clearItem(DbItem *item);
static void commitChange(Db *db);
static int compareItems(cvoid *d1, cvoid *d2, Env *env);
static void change(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd);
static int dberror(Db *db, int code, cchar *fmt, ...);
static void freeChange(Db *db, DbChange *change);
static void freeField(DbField *field);
static void freeModel(DbModel *model);
static void freeItem(Db *db, DbItem *node);
static int flushJournal(Db *db);
static RbTree *getIndex(Db *db, DbParams *params);
static cchar *getIndexName(Db *db, DbParams *params);
static cchar *getIndexHash(Db *db, cchar *index);
static cchar *getIndexSort(Db *db, cchar *index);
static void invokeCallbacks(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                            int event);
static bool matchItem(RbNode *rp, Json *j1, JsonNode *n1, Json *j2, JsonNode *n2, Env *env);
static int mapTypes(Db *db, DbModel *model, Json *props);
static int loadData(Db *db, cchar *path);
static int loadIndexes(Db *db);
static int loadModels(Db *db, Json *json);
static int loadSchema(Db *db, cchar *schema);
static cchar *readBlock(Db *db, FILE *fp, RBuf *buf);
static int readSize(FILE *fp);
static int readItem(FILE *fp, DbItem **item);
static int recreateJournal(Db *db);
static int saveDb(Db *db);
static void selectProperties(Db *db, DbModel *model, Json *props, DbParams *params, cchar *cmd);
static void setDefaults(Db *db, DbModel *model, Json *props);
static void setTemplates(Db *db, DbModel *model, Json *props);
static void setTimestamps(Db *db, DbModel *model, Json *props, cchar *cmd);
static Json *toJson(DbItem *item);
static int writeBlock(Db *db, cchar *buf);
static int writeChangeToJournal(Db *db, DbModel *model, DbItem *item, cchar *cmd);
static int writeItem(FILE *fp, DbItem *item);
static int writeSize(Db *db, int len);

/************************************** Code ***********************************/
/*
    Open database and load data from the given path.
    Schema defines the data model
 */
PUBLIC Db *dbOpen(cchar *path, cchar *schema, int flags)
{
    Db  *db;
    int count;

    if (!path || !*path || !schema || !*schema) {
        return NULL;
    }
    if ((db = rAllocType(Db)) == 0) {
        return 0;
    }
    db->flags = flags;

    if (path) {
        if (flags & DB_OPEN_RESET) {
            dbReset(path);
        }
        db->path = sclone(path);
        db->journalPath = sfmt("%s.jnl", db->path);
        db->maxJournalSize = DB_MAX_LOG_SIZE;
        db->maxJournalAge = DB_MAX_LOG_AGE;
    }
    db->callbacks = rAllocList(0, R_DYNAMIC_VALUE);
    db->context = jsonAlloc();
    db->changes = rAllocHash(0, 0);
    if (loadSchema(db, schema) < 0) {
        rError("db", "%s", db->error);
        dbClose(db);
        return 0;
    }
    if (path) {
        if (loadData(db, path) < 0) {
            rError("db", "%s", db->error);
            dbClose(db);
            return 0;
        }
        //  Recover journal data incase sudden shutdown
        if ((count = applyJournal(db)) < 0) {
            rError("db", "%s", db->error);
            dbClose(db);
            return 0;
        }
        if (count > 0) {
            dbSave(db, NULL);
        }
        if (count >= 0 && !(db->flags & DB_READ_ONLY)) {
            if (recreateJournal(db) < 0) {
                rError("db", "%s", db->error);
                dbClose(db);
                return 0;
            }
        }
    }
    return db;
}

PUBLIC void dbClose(Db *db)
{
    DbModel    *model;
    DbCallback *cb;
    DbChange   *change;
    RName      *np;
    int        ci;

    if (!db) {
        return;
    }
    rStopEvent(db->journalEvent);

    if (!(db->flags & DB_READ_ONLY)) {
        //  Perform a complete save of the in-memory database if the journal has data
        if (db->journalSize) {
            dbSave(db, NULL);
        }
        //  Clean shutdown removes the journal
        if (db->journal) {
            fclose(db->journal);
            db->journal = 0;
            unlink(db->journalPath);
        }
    }
    for (ITERATE_NAMES(db->changes, np)) {
        change = np->value;
        freeChange(db, change);
    }
    for (ITERATE_NAME_DATA(db->models, np, model)) {
        freeModel(model);
    }
    for (ITERATE_ITEMS(db->callbacks, cb, ci)) {
        rFree(cb->model);
    }
    rFreeList(db->callbacks);
    rFreeHash(db->models);
    rFreeHash(db->changes);
    rbFree(db->primary);
    rFree(db->error);
    rFree(db->journalPath);
    rFree(db->path);
    jsonFree(db->schema);
    jsonFree(db->context);
    rFree(db);
}

/*
    Load the database schema of application models, indexes and data fields
 */
static int loadSchema(Db *db, cchar *schema)
{
    Json     *blend, *json;
    JsonNode *inc;
    cchar    *dir;
    char     dirbuf[ME_MAX_FNAME], *errorMsg, *path, *str;
    int      blendId;

    if (!db || !schema) {
        return R_ERR_BAD_ARGS;
    }
    if ((json = jsonParseFile(schema, &errorMsg, 0)) == 0) {
        dberror(db, R_ERR_CANT_READ, "%s", errorMsg);
        rFree(errorMsg);
        return R_ERR_CANT_READ;
    }
    /*
        Keep (locked) schema to preserve memory that is used in DbModels and DbFields.
        Simplifies memory allocation / freeing.
     */
    db->schema = json;

    blendId = jsonGetId(db->schema, 0, "blend");
    if (blendId >= 0) {
        /*
            Can't iterate blend[] while blending below
         */
        blend = jsonAlloc();
        jsonBlend(blend, 0, 0, db->schema, blendId, 0, 0);

        /*
            Get base directory of the schema. Includes are relative to that directory
         */
        scopy(dirbuf, sizeof(dirbuf), schema);
        dir = rDirname(dirbuf);

        for (ITERATE_JSON(blend, 0, inc, nid)) {
            path = rJoinFile(dir, inc->value);
            if ((json = jsonParseFile(path, &errorMsg, 0)) == 0) {
                dberror(db, R_ERR_CANT_READ, "Cannot parse blended schema %s\n%s", inc->value,
                        errorMsg);
                rFree(errorMsg);
                rFree(path);
                jsonFree(blend);
                jsonFree(db->schema);
                db->schema = 0;
                return R_ERR_CANT_READ;
            }
            if (jsonBlend(db->schema, 0, 0, json, 0, 0, JSON_COMBINE) < 0) {
                dberror(db, R_ERR_CANT_READ, "Cannot blend schema %s\n%s", inc->value, errorMsg);
                rFree(errorMsg);
                jsonFree(json);
                rFree(path);
                jsonFree(blend);
                jsonFree(db->schema);
                db->schema = 0;
                return R_ERR_CANT_READ;
            }
            rFree(path);
        }
        jsonFree(blend);
    }

    //  Trace the resulting schema
    if (rEmitLog("debug", "setup")) {
        str = jsonToString(db->schema, 0, 0, JSON_HUMAN);
        rDebug("db", "%s", str);
        rFree(str);
    }

    db->timestamps = jsonGet(db->schema, 0, "params.timestamps", NULL) ? 1 : 0;
    db->type = jsonGet(db->schema, 0, "params.typeField", "_type");

    if (loadModels(db, db->schema) < 0) {
        return R_ERR_CANT_LOAD;
    }
    if (loadIndexes(db) < 0) {
        return R_ERR_CANT_LOAD;
    }
    jsonLock(db->schema);
    return 0;
}

/*
   Load schema models from the schema json file
 */
static int loadModels(Db *db, Json *json)
{
    JsonNode *node, *fp;
    DbField  *field;
    DbModel  *model;
    cchar    *enable, *hash, *mem, *sync;
    char     key[80];
    int      sid, delay, period;

    if (!db || !json) {
        return R_ERR_BAD_ARGS;
    }
    db->models = rAllocHash(0, 0);

    for (ITERATE_JSON_KEY(json, 0, "models", node, mid)) {
        sid = jsonGetId(json, 0, sfmtbuf(key, sizeof(key), "process.%s", node->name));
        enable = jsonGet(json, sid, "enable", "both");

        if (smatch(enable, "cloud")) {
            continue;
        }
        sync = jsonGet(json, sid, "sync", "none");

        if ((mem = jsonGet(json, sid, "mem", NULL)) != NULL &&
            (smatch(mem, "true") || smatch(mem, "1"))) {
            delay = DB_INMEM;
        } else {
            delay = (int) stoi(jsonGet(json, sid, "delay", "0"));
            //  DEPRECATE DELAY
            period = (int) stoi(jsonGet(json, sid, "period", "-2"));
            if (period > -2) {
                delay = period;
            }
            delay *= TPS;
        }
        model = allocModel(db, node->name, sync, delay);
        hash = getIndexHash(db, "primary");

        for (ITERATE_JSON(json, node, fp, fid)) {
            if (smatch(fp->name, hash)) {
                continue;
            }
            if ((field = allocField(db, fp->name, json, fid)) == 0) {
                return R_ERR_MEMORY;
            }
            if (!model->expiresField) {
                model->expiresField = field->ttl ? field->name : NULL;
            }
            rAddName(model->fields, fp->name, field, R_TEMPORAL_NAME | R_STATIC_VALUE);
        }
        if (model->sync) {
            if (!rLookupName(model->fields, "updated")) {
                rError("db", "Model %s is missing required 'updated' field for sync to cloud",
                       model->name);
            }
        }
    }
    return 0;
}

/*
    Load primary index. Currently only supporting a primary index.
 */
static int loadIndexes(Db *db)
{
    if (!db) {
        return R_ERR_BAD_ARGS;
    }
    db->primary = rbAlloc(0, (RbCompare) compareItems, (RbFree) freeItem, db);
    return 0;
}

/*
    Load database data from the persistent store.
    Should be very fast with sequential reads.
 */
static int loadData(Db *db, cchar *path)
{
    DbItem *item;
    uint16 version;
    FILE   *fp;

    if (!db || !path || !*path) {
        return R_ERR_BAD_ARGS;
    }
    if (rAccessFile(path, R_OK) == 0) {
        if ((fp = fopen(path, "r")) == NULL) {
            return dberror(db, R_ERR_CANT_OPEN, "Cannot open %s", path);
        }
        if (fread(&version, sizeof(version), 1, fp) != 1) {
            fclose(fp);
            return dberror(db, R_ERR_CANT_OPEN, "Cannot read database %s, errno %d", path, errno);
        }
        if (version != DB_VERSION) {
            fclose(fp);
            return dberror(db, R_ERR_CANT_OPEN, "Incorrect database version %d", version);
        }
        while (1) {
            if (readItem(fp, &item) < 0) {
                break;
            }
            if (!item) {
                break;
            }
            rbInsert(db->primary, item);
        }
        fclose(fp);
    }
    return 0;
}

/*
    Save the database to persistent store in binary (non-portable) form.
 */
PUBLIC int dbSave(Db *db, cchar *path)
{
    RbTree *rbt;
    RbNode *rp;
    char   temp[ME_MAX_FNAME];
    uint16 version;
    FILE   *fp;

    if (!db) {
        return R_ERR_BAD_ARGS;
    }
    if (db->flags & DB_READ_ONLY) {
        return 0;
    }
    rbt = db->primary;
    path = path ? path : db->path;
    if (!path) {
        return dberror(db, R_ERR_BAD_ARGS, "No path to save to");
    }

    /*
        Write to temp and then rename incase of an outage while writing
     */
    sfmtbuf(temp, sizeof(temp), "%s.save", db->path);
    if ((fp = fopen(temp, "w")) == NULL) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot open %s", temp);
    }
    version = DB_VERSION;
    if (fwrite(&version, sizeof(version), 1, fp) != 1) {
        fclose(fp);
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write version to database file: %d", errno);
    }
    for (rp = rbFirst(rbt); rp; rp = rbNext(rbt, rp)) {
        if (writeItem(fp, rp->data) < 0) {
            fclose(fp);
            return dberror(db, R_ERR_CANT_WRITE, "Cannot save item");
        }
    }
    fclose(fp);

    if (rename(temp, path) < 0) {
        return dberror(db, R_ERR_CANT_WRITE, "Cannot rename save temp file");
    }
    /*
        If the above fails, the journal will still hold a record of all changes.
     */
    if (path == db->path && recreateJournal(db) < 0) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot recreate journal file");
    }
    return 0;
}

static int saveDb(Db *db)
{
    db->journalEvent = 0;
    return dbSave(db, NULL);
}

/*
    Common setup for an API call. This verifies the API properties and parameters.
    Residuals saved in the API Env.
 */
static int setup(Db *db, cchar *modelName, Json *props, DbParams *params, cchar *cmd, Env *env)
{
    DbModel  *model;
    DbField  *field;
    JsonNode *prop;
    char     *cp;

    if (!db || !cmd || !env || !params) {
        return R_ERR_BAD_ARGS;
    }
    memset(env, 0, sizeof(Env));
    rFree(db->error);
    db->error = 0;
    
    if (!props) {
        props = jsonAlloc();
        jsonSetUserFlags(props, USER_ALLOC);

    } else if (!(props->userFlags & USER_ALLOC)) {
        //  User provided json so clone because the props will be modified
        props = jsonClone(props, 0);
        jsonSetUserFlags(props, USER_ALLOC);
    }
    if (!modelName) {
        modelName = jsonGet(props, 0, db->type, 0);
    }
    env->db = db;
    env->props = props;
    env->params = params;
    env->index = getIndex(db, params);
    env->indexSort = getIndexSort(db, getIndexName(db, params));
    env->mustMatch = env->params->where ? 1 : 0;

    if (modelName) {
        if ((model = rLookupName(db->models, modelName)) == 0) {
            return dberror(db, R_ERR_BAD_ARGS, "Unknown schema model \"%s\"", modelName);
        }
        env->model = model;
        if (model && model->expiresField) {
            env->mustMatch = 1;
        }
        /*
            Validate properties. This ensures only schema properties are accepeted and it validates
               enum values.
            FUTURE: data validations
         */
        for (ITERATE_JSON(props, 0, prop, ppid)) {
            if ((field = rLookupName(model->fields, prop->name)) == 0) {
                if (smatch(prop->name, getIndexHash(db, "primary"))) {
                    //  Ignore cloud side hash
                    continue;
                }
                rTrace("db", "Unknown property \"%s\" in model \"%s\"", prop->name, modelName);
                continue;
            }
            if (field->enums) {
                if (!checkEnum(field, prop->value)) {
                    return dberror(db, R_ERR_BAD_ARGS, "Invalid property \"%s\" value \"%s\"",
                                   prop->name, prop->value);
                }
            }
            if (!smatch(prop->name, env->indexSort)) {
                env->mustMatch = 1;
            }
        }
        //  Add global context properties. These take precedence over API supplied properties.
        addContext(db, props);

        //  On create only, set default property values
        if (smatch(cmd, "create") || (smatch(cmd, "update") && env->params->upsert)) {
            setDefaults(db, model, props);
        }
        if (db->timestamps) {
            if (smatch(cmd, "create") || smatch(cmd, "update")) {
                setTimestamps(db, model, props, cmd);
            }
        }
        //  Compute property values using value templates and all other property values
        setTemplates(db, model, props);

        //  Map data types
        if (mapTypes(db, model, props) < 0) {
            return R_ERR_BAD_ARGS;
        }

        //  Select the properties required for this API
        selectProperties(db, model, props, params, cmd);
    }

    //  Determine the primary index search key value (NOTE: the item.key is the key value,
    // item.value is the record value)
    env->search.key = (char*) jsonGet(env->props, 0, env->indexSort, 0);

    if (smatch(cmd, "find")) {
        if (env->params->next) {
            env->next.key = (char*) env->params->next;
        }

    } else if (jsonGet(env->props, 0, env->indexSort, 0) == 0) {
        //  Check we have a key for non-find operations
        return dberror(db, R_ERR_BAD_ARGS, "Missing sort key in properties\n%s",
                       jsonString(env->props, JSON_HUMAN));
    }

    /*
        If doing find or remove with limit, and the value template is unresolved,
        strip the variables and use a beginsWith search.
        Note: Unresolved tokens are preserved in the search key.
     */
    if ((cp = scontains(env->search.key, "${")) != 0) {
        if (smatch(cmd, "find") || (smatch(cmd, "remove") && env->params->limit)) {
            *cp = '\0';
            env->compare = "begins";
        } else {
            return dberror(db, R_ERR_BAD_ARGS, "Incomplete sort key in properties: %s\n%s",
                           env->search.key, jsonString(env->props, JSON_HUMAN));
        }
    }
    //  Must be after find trims templates
    env->searchLen = slen(env->search.key);

    if (env->params->log) {
        rInfo("db", "Command: \"%s\" Properties:\n%s", cmd, jsonString(env->props, JSON_HUMAN));
    }
    return 0;
}

static void freeEnv(Env *env)
{
    RbNode *rp;
    int    next;

    if (!env) {
        return;
    }
    if (env->props && (env->props->userFlags & USER_ALLOC)) {
        jsonFree(env->props);
        env->props = 0;
    }
    if (env->expiredItems) {
        for (ITERATE_ITEMS(env->expiredItems, rp, next)) {
            if (env->params->log) {
                rInfo("db", "Remove expired item:\n%s", jsonString(toJson(rp->data), JSON_HUMAN));
            }
            rbRemove(env->index, rp, 0);
        }
        rClearList(env->expiredItems);
    }
}

/*
    Create a database item of the given model type.
    Props contains the item properties. Params contains request modifiers.
 */
PUBLIC const DbItem *dbCreate(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env     env;
    DbItem  *item;
    DbField *field;
    RName   *np;

    if (!db) {
        return NULL;
    }
    if (SETUP(db, modelName, props, params, "create", &env) < 0) {
        return 0;
    }
    if (!env.params->upsert && rbLookup(env.index, &env.search, &env)) {
        dberror(db, R_ERR_CANT_CREATE, "Cannot create, item already exists");
        freeEnv(&env);
        return 0;
    }
    /*
        Ensure all required properties are provided
     */
    for (ITERATE_NAMES(env.model->fields, np)) {
        field = np->value;
        if (field->required && jsonGet(env.props, 0, np->name, 0) == 0) {
            dberror(db, R_ERR_BAD_STATE, "Missing required property \"%s\"", np->name);
            freeEnv(&env);
            return 0;
        }
    }
    //  AllocItem will assume ownership of the env.props JSON
    item = allocItem(env.search.key, env.props, 0);
    env.props = 0;

    rbInsert(env.index, item);
    change(db, env.model, item, params, env.params->upsert ? "upsert" : "create");

    if (env.params->log) {
        rInfo("db", "Create result:\n%s", jsonString(item->json, JSON_HUMAN));
    }
    // XXXX
    freeEnv(&env);
    return item;
}

/*
    Get a database item of the given model type.
    Props contains item properties to search for. This should include the item key (or key
       ingredients).
    Params contains request modifiers.
 */
PUBLIC const DbItem *dbGet(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "get", &env) < 0) {
        return 0;
    }
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env)) {
            if (env.params->log) {
                dbPrintItem(item);
            }
            freeEnv(&env);
            return item;
        }
    }
    freeEnv(&env);
    return 0;
}

/*
    Get a field from an item. This returns a direct (const) reference into the data.
 */
PUBLIC cchar *dbGetField(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db || !modelName || !fieldName) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "get", &env) < 0) {
        return 0;
    }
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env)) {
            // XXX
            freeEnv(&env);
            return jsonGet(toJson(item), 0, fieldName, 0);
        }
    }
    freeEnv(&env);
    return 0;
}

PUBLIC bool dbGetBool(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params, bool defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    if (value) {
        return smatch(value, "true");
    }
    return defaultValue;
}

PUBLIC Time dbGetDate(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params, Time defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    if (value) {
        return rParseIsoDate(value);
    }
    return defaultValue;
}

PUBLIC double dbGetDouble(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params,
                          double defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    if (value) {
        return stod(value);
    }
    return defaultValue;
}

PUBLIC int64 dbGetNum(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params, int64 defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    if (value) {
        return stoi(value);
    }
    return defaultValue;
}

PUBLIC cchar *dbGetString(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params,
                          cchar *defaultValue)
{
    return dbGetField(db, modelName, fieldName, props, params);
}

/*
    Find matching items of the given model type.
    Props contains item properties to search for. This should include the item key (or key
       ingredients).
    Params contains request modifiers.
 */
PUBLIC RList *dbFind(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;
    RList  *list;
    int    count, limit;

    if (!db) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "find", &env) < 0) {
        return 0;
    }
    limit = env.params->limit ? env.params->limit : MAXINT;
    if ((list = rAllocList(0, 0)) == 0) {
        freeEnv(&env);
        return 0;
    }
    count = 0;

    if (env.params->next) {
        //  Lookup the exact last item and then step forward and match with the search key
        if ((rp = rbLookupFirst(env.index, &env.next, &env)) != 0) {
            if (env.search.key) {
                rp = rbLookupNext(env.index, rp, &env.search, &env);
            } else {
                rp = rbNext(env.index, rp);
            }
        }
    } else if (env.search.key) {
        rp = rbLookupFirst(env.index, &env.search, &env);
    } else {
        rp = rbFirst(db->primary);
    }
    while (rp) {
        item = rp->data;
        if (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env)) {
            rPushItem(list, item);
            if (++count >= limit) {
                break;
            }
        }
        if (env.search.key) {
            rp = rbLookupNext(env.index, rp, &env.search, &env);
        } else {
            rp = rbNext(db->primary, rp);
        }
    }
    if (env.params->log) {
        dbPrintList(list);
    }
    // XXX
    freeEnv(&env);
    return list;
}

/*
    Find one database item.
 */
PUBLIC const DbItem *dbFindOne(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "find", &env) < 0) {
        return 0;
    }
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env)) {
            freeEnv(&env);
            return item;
        }
    }
    freeEnv(&env);
    return 0;
}

/*
    Remove and item.
    If limit > 0, then can do remove without specifying SK and will remove one or more matching
       items
 */
PUBLIC int dbRemove(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *next, *rp;
    DbItem *item, *search;
    int    count, limit;

    if (!db || !props) {
        return R_ERR_BAD_ARGS;
    }
    if (SETUP(db, modelName, props, params, "remove", &env) < 0) {
        return db->code;
    }
    limit = env.params->limit ? env.params->limit : 1;
    search = &env.search;

    for (count = 0, rp = rbLookupFirst(env.index, search, &env); rp; rp = next) {
        next = rbLookupNext(env.index, rp, search, &env);
        item = rp->data;
        if (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env)) {
            change(db, env.model, item, params, "remove");
            rbRemove(env.index, rp, 0);
            if (++count >= limit) {
                break;
            }
        }
    }
    freeEnv(&env);
    if (count) {
        flushJournal(db);
    }
    return count;
}

/*
    Must be manually invoked by the user to remove expired items.
 */
PUBLIC int dbRemoveExpired(Db *db, bool notify)
{
    Env     env;
    RbNode  *rp, *next;
    DbModel *model;
    DbItem  *item;
    RName   *np;
    Json    *json, *allocJson;
    cchar   *expires;
    char    *now;
    int     count;

    if (!db) {
        return R_ERR_BAD_ARGS;
    }
    now = rGetIsoDate(rGetTime());
    db->servicing = 1;
    count = 0;

    for (ITERATE_NAME_DATA(db->models, np, model)) {
        if (!model->expiresField) continue;
        if (SETUP(db, model->name, NULL, NULL, "find", &env) < 0) {
            return 0;
        }
again:
        for (rp = rbLookupFirst(env.index, &env.search, &env); rp; rp = next) {
            next = rbLookupNext(env.index, rp, &env.search, &env);
            item = rp->data;
            //  Do this rather than using dbJson so we don't convert nodes to json unnecessarily
            allocJson = 0;
            json = item->json ? item->json : (allocJson = jsonParse(item->value, 0));
            expires = jsonGet(json, 0, model->expiresField, 0);
            if (expires && scmp(expires, now) <= 0) {
                if (notify) {
                    change(db, model, item, NULL, "remove");
                }
                if (rEmitLog("trace", "db")) {
                    rTrace("db", "Remove expired item:\n%s", jsonString(json, JSON_HUMAN));
                }
                rbRemove(db->primary, rp, 0);
                count++;
                if (allocJson) {
                    jsonFree(allocJson);
                }
                goto again;
            }
            if (allocJson) {
                jsonFree(allocJson);
            }
        }
        freeEnv(&env);
    }
    db->servicing = 0;

    if (flushJournal(db) < 0) {
        return R_ERR_CANT_WRITE;
    }
    rFree(now);
    return count;
}

/*
    Convert all json back to a string rep
 */
PUBLIC void dbCompact(Db *db)
{
    RbNode *rp;
    DbItem *item;

    for (rp = rbFirst(db->primary); rp; rp = rbNext(db->primary, rp)) {
        item = rp->data;
        if (item->json) {
            item->value = jsonToString(item->json, 0, 0, 0);
            item->allocatedValue = 1;
            jsonFree(item->json);
        }
    }
}

/*
    Update a field in an item.
    Warning: caller must provide props to uniquely identify the item
    Value may be null to remove the property.
 */
PUBLIC const DbItem *dbSetField(Db *db, cchar *modelName, cchar *fieldName, cchar *value,
                                Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db || !modelName || !fieldName) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "update", &env) < 0) {
        return 0;
    }
    item = 0;

    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env)) {
            break;
        }
        item = 0;
    }
    if (!item) {
        if (env.params->upsert) {
            item = allocItem(env.search.key, env.props, 0);
            rbInsert(env.index, item);
        } else {
            dberror(db, R_ERR_NOT_READY, "Cannot set field, item does not exist");
            freeEnv(&env);
            return 0;
        }
    }
    if (value == NULL) {
        jsonRemove(toJson(item), 0, fieldName);
    } else {
        jsonSet(toJson(item), 0, fieldName, value, 0);
    }
    change(db, env.model, item, params, env.params->upsert ? "upsert" : "update");
    //XXX
    freeEnv(&env);
    return item;
}

PUBLIC const DbItem *dbSetBool(Db *db, cchar *modelName, cchar *fieldName, bool value, Json *props, DbParams *params)
{
    return dbSetField(db, modelName, fieldName, value ? "true" : "false", props, params);
}

PUBLIC const DbItem *dbSetDouble(Db *db, cchar *modelName, cchar *fieldName, double value, Json *props,
                                 DbParams *params)
{
    char buf[32];

    rSnprintf(buf, sizeof(buf), "%f", value);
    return dbSetField(db, modelName, fieldName, buf, props, params);
}

PUBLIC const DbItem *dbSetDate(Db *db, cchar *modelName, cchar *fieldName, Time when, Json *props, DbParams *params)
{
    char         *value;
    const DbItem *item;

    value = rGetIsoDate(when);
    item = dbSetField(db, modelName, fieldName, value, props, params);
    return item;
}

PUBLIC const DbItem *dbSetNum(Db *db, cchar *modelName, cchar *fieldName, int64 value, Json *props, DbParams *params)
{
    char buf[32];

    return dbSetField(db, modelName, fieldName, sitosbuf(buf, sizeof(buf), value, 10), props, params);
}

PUBLIC const DbItem *dbSetString(Db *db, cchar *modelName, cchar *fieldName, cchar *value, Json *props,
                                 DbParams *params)
{
    return dbSetField(db, modelName, fieldName, value, props, params);
}


/*
    Update an item
 */
PUBLIC const DbItem *dbUpdate(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db) {
        return NULL;
    }
    if (!props) {
        dberror(db, R_ERR_BAD_ARGS, "Cannot update, bad properties");
        return 0;
    }
    if (SETUP(db, modelName, props, params, "update", &env) < 0) {
        return 0;
    }
    item = 0;
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env)) {
            break;
        }
    }
    if (item) {
        if (env.params->upsert) {
            env.props->userFlags &= ~USER_ALLOC;
            clearItem(item);
            item->json = env.props;
        } else {
            //  Preserve existing properties that are not being updated
            jsonBlend(toJson(item), 0, 0, env.props, 0, 0, JSON_REMOVE_UNDEF);
        }

    } else {
        if (!env.params->upsert) {
            dberror(db, R_ERR_CANT_FIND, "Cannot update, item does not exist");
            freeEnv(&env);
            return 0;
        }
        if ((item = allocItem(env.search.key, env.props, 0)) != 0) {
            rbInsert(env.index, item);
        }
    }
    change(db, env.model, item, params, env.params->upsert ? "upsert" : "update");
    // XXX
    freeEnv(&env);
    return item;
}

PUBLIC cchar *dbField(const DbItem *item, cchar *fieldName)
{
    if (!item || !fieldName) {
        return 0;
    }
    return jsonGet(toJson((DbItem*) item), 0, fieldName, 0);
}

PUBLIC double dbFieldDouble(const DbItem *item, cchar *fieldName)
{
    return stof(dbField(item, fieldName));
}

PUBLIC int64 dbFieldNumber(const DbItem *item, cchar *fieldName)
{
    return stoi(dbField(item, fieldName));
}

PUBLIC bool dbFieldBool(const DbItem *item, cchar *fieldName)
{
    cchar *value;

    value = dbField(item, fieldName);
    return (smatch(value, "true") || smatch(value, "1")) ? 1 : 0;
}

PUBLIC Time dbFieldDate(const DbItem *item, cchar *fieldName)
{
    cchar *when;

    if ((when = dbField(item, fieldName)) == NULL) {
        return 0;
    }
    return rParseIsoDate(when);
}

PUBLIC char *dbListToString(RList *items)
{
    const DbItem *item;
    RBuf         *buf;
    int          index;

    if (!items) {
        return NULL;
    }
    buf = rAllocBuf(0);
    rPutCharToBuf(buf, '[');
    for (ITERATE_ITEMS(items, item, index)) {
        rPutStringToBuf(buf, jsonToString(toJson((DbItem*) item), 0, 0, 0));
        rPutCharToBuf(buf, ',');
    }
    rAdjustBufEnd(buf, -1);
    rPutCharToBuf(buf, ']');
    return rBufToStringAndFree(buf);
}

static void printItem(DbItem *item)
{
    char *value;

    if (!item) return;
    value = jsonToString(toJson(item), 0, 0, JSON_HUMAN);
    value[slen(value) - 1] = '\0';
    rPrintf("%s", value);
    rFree(value);
}

PUBLIC void dbPrintTree(Db *db)
{
    rbPrint(db->primary, (void*) printItem);
}

PUBLIC void dbPrintItem(const DbItem *item)
{
    char *value;

    if (item) {
        value = jsonToString(toJson((DbItem*) item), 0, 0, JSON_HUMAN);
        rPrintf("%s: %s\n", item->key, value);
        rFree(value);
    } else {
        rPrintf("Item not defined\n");
    }
}

PUBLIC void dbPrintList(RList *list)
{
    const DbItem *item;
    char         *value;
    int          index;

    for (ITERATE_ITEMS(list, item, index)) {
        value = jsonToString(toJson((DbItem*) item), 0, 0, JSON_HUMAN);
        rPrintf("    %s: %s\n", item->key, value);
        rFree(value);
    }

}

PUBLIC void dbPrintProperties(Json *props)
{
    rPrintf("Properties\n%s\n", jsonToString(props, 0, 0, JSON_HUMAN));
}

PUBLIC void dbPrint(Db *db)
{
    RbNode *rp;

    for (rp = rbFirst(db->primary); rp; rp = rbNext(db->primary, rp)) {
        dbPrintItem(rp->data);
    }
}

/*
    This will convert an item value to JSON for queries and will create item.json and zero item.value
 */
static Json *toJson(DbItem *item)
{
    if (!item) {
        return 0;
    }
    if (!item->json) {
        item->json = jsonParse(item->value, 0);
        if (item->allocatedValue) {
            rFree(item->value);
        }
        item->value = 0;
    }
    return item->json;
}

/*
    Public function to get the JSON object for an item. Because this returns a reference to
    the internal data, we return a const pointer.
    Force a const DbItem so callers don't need to cast returns from dbGet etc.
    Caller must not modify or free the return result.
 */
PUBLIC const Json *dbJson(const DbItem *citem)
{
    return toJson((DbItem*) citem);
}

PUBLIC cchar *dbString(const DbItem *citem, int flags)
{
    DbItem *item;

    item = (DbItem*) citem;
    if (item->json) {
        if (item->allocatedValue) {
            rFree(item->value);
        }
        item->value = jsonToString(item->json, 0, NULL, flags);
        item->allocatedValue = 1;
    }
    return item->value;
}

static int getTypeFromSchema(cchar *type)
{
    if (smatch(type, "array")) {
        return JSON_ARRAY;
    } else if (smatch(type, "object")) {
        return JSON_OBJECT;
    } else if (smatch(type, "string")) {
        return JSON_STRING;
    } else {
        return JSON_PRIMITIVE;
    }
}

/*
    For create() set the default properties or generated values
 */
static void setDefaults(Db *db, DbModel *model, Json *props)
{
    RName   *np;
    DbField *field;
    char    *value;
    ssize   size;

    if (!db || !model || !props) {
        return;
    }
    for (ITERATE_NAME_DATA(model->fields, np, field)) {
        if (field->def || field->generate) {
            if (jsonGet(props, 0, np->name, 0) != 0) {
                continue;
            }
            if (field->def) {
                jsonSet(props, 0, np->name, field->def, getTypeFromSchema(field->type));

            } else if (field->generate) {
                if (smatch(field->generate, "ulid")) {
                    value = dbGetULID(rGetTime());
                    jsonSet(props, 0, np->name, (void*) value, JSON_STRING);
                    rFree(value);
                } else if (smatch(field->generate, "uid")) {
                    value = dbGetUID(10);
                    jsonSet(props, 0, np->name, (void*) value, JSON_STRING);
                    rFree(value);
                } else if (sstarts(field->generate, "uid(")) {
                    size = stoi(&field->generate[4]);
                    value = dbGetUID(size);
                    jsonSet(props, 0, np->name, (void*) value, JSON_STRING);
                    rFree(value);
                }
            }
        }
    }
}

static void setTimestamps(Db *db, DbModel *model, Json *props, cchar *cmd)
{
    RName   *np;
    DbField *field;
    char    *value;

    if (!db || !model || !props) {
        return;
    }
    for (ITERATE_NAME_DATA(model->fields, np, field)) {
        if ((smatch(np->name, "created") && smatch(cmd, "create")) ||
            smatch(np->name, "updated") || smatch(np->name, "remove")) {
            value = rGetIsoDate(rGetTime());
            jsonSet(props, 0, np->name, (void*) value, 0);
            rFree(value);
        }
    }
}

/*
    Set the value of fields using value templates
 */
static void setTemplates(Db *db, DbModel *model, Json *props)
{
    RName   *np;
    DbField *field;
    char    *value;

    if (!db || !model || !props) {
        return;
    }
    for (ITERATE_NAME_DATA(model->fields, np, field)) {
        if (field->value) {
            if (jsonGet(props, 0, np->name, 0) == 0) {
                value = jsonTemplate(props, field->value, 1);
                jsonSet(props, 0, np->name, value, 0);
                rFree(value);
            }
        }
    }
}

/*
    Map and validate data types
 */
static int mapTypes(Db *db, DbModel *model, Json *props)
{
    DbField  *field;
    JsonNode *prop;
    cchar    *value;

    if (!db || !model || !props) {
        return R_ERR_BAD_ARGS;
    }
again:
    for (ITERATE_JSON(props, 0, prop, ppid)) {
        field = rLookupName(model->fields, prop->name);
        if (!field) {
            //  Unknown field. Maybe context that does not apply for this model
            jsonRemove(props, ppid, 0);
            //  Can't iterate and mutate
            goto again;
        }
        if (prop->type == JSON_PRIMITIVE && smatch(prop->value, "undefined")) {
            continue;
        }
        if (smatch(field->type, "date")) {
            if (sfnumber(prop->value)) {
                jsonSetNodeValue(prop, rGetIsoDate(stoi(prop->value)), JSON_STRING, JSON_PASS_VALUE);

            } else if (!sends(prop->value, "Z")) {
                return dberror(db, R_ERR_BAD_ARGS, "Invalid date in property \"%s\": %s",
                               prop->name, prop->value);
            }

        } else if (smatch(field->type, "boolean")) {
            if (smatch(prop->value, "true") || smatch(prop->value, "1")) {
                value = "true";
            } else if (smatch(prop->value, "false") || smatch(prop->value, "0")) {
                value = "false";
            } else {
                return dberror(db, R_ERR_BAD_ARGS, "Invalid boolean in property \"%s\": %s",
                               prop->name, prop->value);
            }
            jsonSetNodeValue(prop, value, JSON_PRIMITIVE, 0);

        } else if (smatch(field->type, "number")) {
            if (!sfnumber(prop->value)) {
                return dberror(db, R_ERR_BAD_ARGS, "Invalid numeric in property \"%s\": %s",
                               prop->name, prop->value);
            }
            if (prop->type != JSON_PRIMITIVE) {
                jsonSetNodeType(prop, JSON_PRIMITIVE);
            }

        } else if (smatch(field->type, "string")) {
            if (prop->type != JSON_STRING) {
                jsonSetNodeType(prop, JSON_STRING);
            }
        }
    }
    return 0;
}

/*
    Select the properties required for the API
 */
static void selectProperties(Db *db, DbModel *model, Json *props, DbParams *params, cchar *cmd)
{
    JsonNode *prop;
    DbField  *field;

    for (ITERATE_JSON(props, 0, prop, ppid)) {
        field = rLookupName(model->fields, prop->name);
        if (!field) {
            jsonRemove(props, ppid, 0);
        }
    }
    if (smatch(cmd, "create") || (smatch(cmd, "update") && params->upsert)) {
        jsonSet(props, 0, db->type, (char*) model->name, 0);
    }
}

/*
    Match an item in n1 against a target in n2.
    Env provides the comparison options
 */
static bool matchItem(RbNode *rp, Json *j1, JsonNode *n1, Json *j2, JsonNode *n2, Env *env)
{
    JsonNode *c1, *c2;
    cchar    *expires;
    char     *now;
    int      nid, rc;

    if (j2->count == 0) {
        return 0;
    }
    if (n2 == 0) {
        n2 = jsonGetNode(j2, 0, 0);
    }
    if (j1->count > 0) {
        if (n1 == 0) {
            n1 = jsonGetNode(j1, 0, 0);
        }
        /*
            Match given properties
         */
        for (ITERATE_JSON(j1, n1, c1, cid)) {
            if (smatch(c1->name, env->indexSort)) {
                //  Already been done via the lookup
                continue;
            }
            c2 = jsonGetNode(j2, jsonGetNodeId(j2, n2), c1->name);
            if (!c2) return 0;

            rc = scmp(c1->value, c2->value);
            if (rc != 0) return 0;

            if (c1->type == JSON_OBJECT && c2->type == JSON_OBJECT) {
                if (!matchItem(rp, j1, c1, j2, c2, env)) {
                    return 0;
                }
            }
            if (c1->type == JSON_ARRAY && c2->type == JSON_ARRAY) {
                if (!matchItem(rp, j1, c1, j2, c2, env)) {
                    return 0;
                }
            }
        }
    }
    /*
        Invoke where expression callbacks
     */
    rc = 1;
    if (env->params->where) {
        nid = jsonGetNodeId(j2, n2);
        rc = (env->params->where)(j2, nid, env->params->arg);
    }
    if (rc && env->model->expiresField) {
        expires = jsonGet(j2, 0, env->model->expiresField, 0);
        now = rGetIsoDate(rGetTime());
        if (expires && scmp(expires, now) <= 0) {
            //  Add item to expired list to cleanup in freeEnv
            if (!env->expiredItems) {
                env->expiredItems = rAllocList(0, 0);
            }
            rAddItem(env->expiredItems, rp);
            rc = 0;
        }
        rFree(now);
    }
    return rc;
}

static DbModel *allocModel(Db *db, cchar *name, cchar *sync, Time delay)
{
    DbModel *model;
    DbField *field;

    if (!db || !name) {
        return NULL;
    }
    if ((model = rAllocType(DbModel)) == 0) {
        return 0;
    }
    model->name = name;
    model->sync = (smatch(sync, "both") || smatch(sync, "up") || smatch(sync, "down")) ? 1 : 0;
    model->delay = delay;
    model->fields = rAllocHash(0, 0);

    /*
        Add type field to the model
     */
    field = rAllocType(DbField);
    field->name = sclone(name);
    field->hidden = 1;
    rAddName(model->fields, db->type, field, R_TEMPORAL_NAME | R_STATIC_VALUE);

    rAddName(db->models, model->name, model, 0);
    return model;
}

static void freeModel(DbModel *model)
{
    RName   *fp;
    DbField *field;

    if (model) {
        for (ITERATE_NAME_DATA(model->fields, fp, field)) {
            freeField(field);
        }
        rFreeHash(model->fields);
        rFree(model);
    }
}

PUBLIC DbModel *dbGetModel(Db *db, cchar *name)
{
    return rLookupName(db->models, name);
}

PUBLIC DbModel *dbGetItemModel(Db *db, CDbItem *item)
{
    cchar *modelName;

    modelName = dbField(item, db->type);
    return rLookupName(db->models, modelName);
}

/*
    Allocate a model field from the OneTable schema
 */
static DbField *allocField(Db *db, cchar *name, Json *json, int fid)
{
    DbField *field;

    if (!name || !json || fid < 0) {
        return NULL;
    }
    if ((field = rAllocType(DbField)) == 0) {
        return 0;
    }
    //  Memory is preserved in the schema - so can use jsonGet references
    field->name = sclone(name);
    field->def = jsonGet(json, fid, "default", 0);
    field->generate = jsonGet(json, fid, "generate", 0);
    field->hidden = jsonGetBool(json, fid, "hidden", smatch(name, "pk") || smatch(name, "sk"));
    field->required = jsonGetBool(json, fid, "required", 0);
    field->type = jsonGet(json, fid, "type", 0);
    field->value = jsonGet(json, fid, "value", 0);
    field->ttl = jsonGetBool(json, fid, "ttl", 0);

    if (jsonGetNode(json, fid, "enum") != 0) {
        field->enums = jsonToString(json, fid, "enum", 0);
    }
#if KEEP
    field->crypt = jsonGetBool(json, fid, "crypt", 0);
    field->map = jsonGet(json, fid, "map", 0);
    field->unique = jsonGetBool(json, fid, "unique", 0);
    field->validate = jsonGet(json, fid, "validate", 0);
#endif
    return field;
}

static void freeField(DbField *field)
{
    if (field) {
        rFree(field->enums);
        rFree(field->name);
        rFree(field);
    }
}

/*
    Assume ownership of the value and the json.
    The "json" and "value" may be null.
    If both are set, "json" takes precedence and may be more current.
 */
static DbItem *allocItem(cchar *key, Json *json, char *value)
{
    DbItem *item;

    if ((item = rAllocType(DbItem)) == 0) {
        return 0;
    }
    if (json) {
        if (json->userFlags & USER_ALLOC) {
            //  Cleanup before insertion into the RB tree
            json->userFlags &= ~USER_ALLOC;
        } else {
            /*
                Not allocated as part of the DB_PROPS() API in dbCreate(), so we must clone
                here as the user owns these props.
             */
            json = jsonClone(json, 0);
        }
        item->json = json;
    } else {
        item->value = value;
        item->allocatedValue = 1;
    }
    item->key = sclone(key);
    item->allocatedName = 1;
    return item;
}

static void freeItem(Db *db, DbItem *item)
{
    if (!item) return;

    if (item->allocatedName) {
        rFree(item->key);
        item->key = 0;
    }
    clearItem(item);
    free(item);
}

static void clearItem(DbItem *item)
{
    if (item) {
        if (item->allocatedValue) {
            rFree(item->value);
            item->value = 0;
        }
        jsonFree(item->json);
        item->json = 0;
    }
}

/*
    Get the name of the index to use from the params.
 */
static cchar *getIndexName(Db *db, DbParams *params)
{
    return (params && params->index) ? params->index : "primary";
}

/*
    Get the index to use for this API call. Currently only supports a primary index.
    Defaults to the primary if params is null.
 */
static RbTree *getIndex(Db *db, DbParams *params)
{
    return db->primary;
}

/*
    Get the hash key index field name
 */
static cchar *getIndexHash(Db *db, cchar *index)
{
    char key[160];

    sfmtbuf(key, sizeof(key), "indexes.%s.hash", index);
    return jsonGet(db->schema, 0, key, 0);
}

/*
    Get the sort key index field name
 */
static cchar *getIndexSort(Db *db, cchar *index)
{
    char key[160];

    sfmtbuf(key, sizeof(key), "indexes.%s.sort", index);
    return jsonGet(db->schema, 0, key, 0);
}

PUBLIC cchar *dbGetSortKey(Db *db)
{
    return getIndexSort(db, getIndexName(db, NULL));
}

PUBLIC Json *dbStringToJson(cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    Json    *json;

    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    json = jsonParseKeep(buf, 0);
    if (json) {
        json->userFlags |= USER_ALLOC;
    }
    va_end(ap);
    return json;
}

/*
    Convert an array of properties into a json object to be used as props.
 */
PUBLIC Json *dbPropsToJson(cchar *props[])
{
    Json *json;
    int  i;

    json = jsonAlloc();
    for (i = 0; props[i]; i += 2) {
        if (props[i] && props[i + 1]) {
            jsonSet(json, 0, props[i], (void*) props[i + 1], 0);
        }
    }
    json->userFlags |= USER_ALLOC;
    return json;
}

/*
    Read an item from the on-disk database store (not the journal)
 */
static int readItem(FILE *fp, DbItem **item)
{
    ssize length;
    char  key[DB_MAX_KEY];
    char  *data;

    if (!fp || !item) {
        return R_ERR_BAD_ARGS;
    }
    *item = 0;
    if (fread(&length, sizeof(length), 1, fp) != 1) {
        return 0;
    }
    if (length > (DB_MAX_KEY - 1)) {
        return R_ERR_BAD_STATE;
    }
    if (fread(key, length, 1, fp) != 1) {
        return R_ERR_CANT_READ;
    }
    key[length] = '\0';

    if (fread(&length, sizeof(length), 1, fp) != 1) {
        return R_ERR_CANT_READ;
    }
    if (length > DB_MAX_ITEM) {
        return R_ERR_BAD_STATE;
    }
    if ((data = rAlloc(length + 1)) == 0) {
        return R_ERR_MEMORY;
    }
    if (fread(data, length, 1, fp) != 1) {
        return R_ERR_CANT_READ;
    }
    data[length] = '\0';
    *item = allocItem(key, 0, data);
    return 0;
}

/*
    Persist an item to the on-disk store
 */
static int writeItem(FILE *fp, DbItem *item)
{
    char  *value;
    ssize length;

    length = slen(item->key);
    if (fwrite(&length, sizeof(length), 1, fp) != 1) {
        return R_ERR_CANT_WRITE;
    }
    if (fwrite(item->key, length, 1, fp) != 1) {
        return R_ERR_CANT_WRITE;
    }
    value = item->json ? jsonToString(item->json, 0, 0, 0) : item->value;

    length = slen(value);
    if (fwrite(&length, sizeof(length), 1, fp) != 1) {
        return R_ERR_CANT_WRITE;
    }
    if (fwrite(value, length, 1, fp) != 1) {
        return R_ERR_CANT_WRITE;
    }
    if (value != item->value) {
        rFree(value);
    }
    return 0;
}

/*
    Get the persistency delay. API params override model delay.
 */
static Ticks getDelay(Db *db, DbModel *model, DbParams *params)
{
    Ticks delay;

    delay = model->delay;
    if (params) {
        if (params->mem) {
            delay = DB_INMEM;
        } else if (params->delay) {
            delay = params->delay == DB_NODELAY ? 0 : params->delay;
        }
    }
    return delay;
}

/*
    React to a change. This handles persistency to the journal including delayed commits.
 */
static void change(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd)
{
    DbChange *change;
    Ticks    delay, when;
    int      events;

    delay = getDelay(db, model, params);
    if (delay > 0) {
        /*
            Create a change record and schedule.
         */
        when = rGetTicks() + delay;
        change = rLookupName(db->changes, item->key);
        if (!change) {
            if ((change = allocChange(db, model, item, params, cmd, when)) == 0) {
                return;
            }
        } else if (when < change->due) {
            change->due = when;
        }
        item->delayed = 1;
        if (change->due < db->due || !db->due) {
            db->due = change->due;
            if (db->commitEvent) {
                rStopEvent(db->commitEvent);
            }
            db->commitEvent = rStartEvent((REventProc) commitChange, db, delay);
        }
        events = DB_ON_CHANGE;

    } else {
        if (delay != DB_INMEM && db->journal) {
            /*
                No delay, write to the journal unless in-mem
             */
            if (writeChangeToJournal(db, model, item, cmd) == 0) {
                if (item->delayed) {
                    rRemoveName(db->changes, item->key);
                    item->delayed = 0;
                }
            }
        }
        events = DB_ON_CHANGE | DB_ON_COMMIT;
    }
    invokeCallbacks(db, model, item, params, cmd, events);
}

/*
    This commits due changes to the journal
 */
static void commitChange(Db *db)
{
    DbChange *change;
    DbItem   search, *item;
    RbNode   *rp;
    RName    *np;
    Time     nextDue, now;

    now = rGetTicks();
    nextDue = MAXINT64;

    for (ITERATE_NAME_DATA(db->changes, np, change)) {
        if (change->due <= now) {
            search.key = change->key;
            if ((rp = rbLookup(db->primary, &search, NULL)) != 0) {
                item = rp->data;
                if (writeChangeToJournal(change->db, change->model, item, change->cmd) != 0) {
                    // continue
                }
                invokeCallbacks(change->db, change->model, item, change->params, change->cmd,
                                DB_ON_COMMIT);
                freeChange(db, change);
            }
        } else {
            nextDue = min(nextDue, change->due);
        }
    }
    if (nextDue == MAXINT64) {
        db->due = 0;
        db->commitEvent = 0;
    } else {
        db->due = nextDue;
        db->commitEvent = rStartEvent((REventProc) commitChange, db, nextDue - now);
    }
}

/*
    Write a changed item to the journal and handle journal resets
    Errors are handled by low level I/O routines setting db->journalError
 */
static int writeChangeToJournal(Db *db, DbModel *model, DbItem *item, cchar *cmd)
{
    char *value;
    int  bufsize;

    //  Journal will be unset when booting and applying prior journal
    if (!db->journal) return 0;

    /*
        Compute the size of the change record as three null terminated strings.
        The string lengths are not read into the buffer.
     */
    value = item->json ? jsonToString(item->json, 0, 0, 0) : item->value;
    bufsize = (int) (slen(cmd) + slen(model->name) + slen(value) + 3);
    item->delayed = 0;

    //  These set journalError on errors - handled below
    writeSize(db, bufsize);
    writeBlock(db, cmd);
    writeBlock(db, model->name);
    writeBlock(db, value);

    if (value != item->value) {
        rFree(value);
    }
    if (fflush(db->journal) < 0 || rFlushFile(fileno(db->journal)) < 0) {
        dberror(db, R_ERR_CANT_WRITE, "Cannot flush journal: %d", errno);
        db->journalError = 1;
    }
    return flushJournal(db);
}

static int flushJournal(Db *db)
{
    /*
        Save the journal to the database if it is full or if there is an error.
     */
    if (db->journalError || db->journalSize >= db->maxJournalSize ||
        (rGetTicks() - db->journalCreated) >= db->maxJournalAge) {
        if (db->servicing) {
            db->needSave = 1;
        } else if (dbSave(db, NULL) < 0) {
            return R_ERR_CANT_WRITE;
        }

    } else if (!db->journalEvent) {
        db->journalEvent = rStartEvent((REventProc) saveDb, db, db->maxJournalAge);
    }
    return db->journalError ? R_ERR_CANT_WRITE : 0;
}

static int writeSize(Db *db, int len)
{
    if (fwrite(&len, sizeof(len), 1, db->journal) != 1) {
        db->journalError = 1;
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write to db journal file");
    }
    return 0;
}

/*
    Write the string including the trailing null
 */
static int writeBlock(Db *db, cchar *buf)
{
    int len;

    len = (int) slen(buf) + 1;
    if (fwrite(&len, sizeof(len), 1, db->journal) != 1) {
        db->journalError = 1;
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write to db journal file");
    }
    if (fwrite(buf, len, 1, db->journal) != 1) {
        db->journalError = 1;
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write to db journal file");
    }
    db->journalSize += sizeof(len) + len;
    return 0;
}

/*
    Recreate the journal. Will close journal if already open.
 */
static int recreateJournal(Db *db)
{
    uint16 version;

    if (db->journal) {
        fclose(db->journal);
    }
    if ((db->journal = fopen(db->journalPath, "w")) == NULL) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot open database journal %s, errno %d",
                       db->journalPath, errno);
    }
    version = DB_VERSION;
    if (fwrite(&version, sizeof(version), 1, db->journal) != 1) {
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write version to db journal file");
    }
    db->journalCreated = rGetTicks();
    db->journalSize = 0;
    db->journalError = 0;
    return 0;
}

/*
    Apply the journal of changes to the database state
    Return 1 if journal data was applied, 0 if not, negative for errors.
 */
static int applyJournal(Db *db)
{
    struct stat sbuf;
    FILE        *fp;
    RBuf        *buf;
    cchar       *cmd, *model, *value;
    uint16      version;
    int         bufsize, rc;

    if (stat(db->journalPath, &sbuf) < 0 || sbuf.st_size == 0) {
        return 0;
    }
    if ((fp = fopen(db->journalPath, "r")) == NULL) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot open database journal %s, errno %d", db->journalPath, errno);
    }
    if (fread(&version, sizeof(version), 1, fp) != 1) {
        fclose(fp);
        return dberror(db, R_ERR_CANT_OPEN, "Cannot read database journal %s, errno %d", db->journalPath, errno);
    }
    if (version != DB_VERSION) {
        fclose(fp);
        return dberror(db, R_ERR_CANT_OPEN, "Incorrect database journal version %d", version);
    }
    buf = rAllocBuf(ME_BUFSIZE);
    rc = 0;
    while ((bufsize = readSize(fp)) != 0) {
        if (rGrowBuf(buf, bufsize) < 0) {
            break;
        }
        cmd = readBlock(db, fp, buf);
        model = readBlock(db, fp, buf);
        value = readBlock(db, fp, buf);
        if (!cmd || !model || !value) {
            break;
        }
        if (applyChange(db, cmd, model, value) < 0) {
            rc = R_ERR_CANT_READ;
            break;
        }
        rc++;
        rFlushBuf(buf);
    }
    rFreeBuf(buf);
    fclose(fp);
    return rc;
}

static int readSize(FILE *fp)
{
    int len;

    if (fread(&len, sizeof(len), 1, fp) != 1) {
        //  EOF
        return 0;
    }
    return len;
}

static cchar *readBlock(Db *db, FILE *fp, RBuf *buf)
{
    cchar *result;
    int   len;

    //  The length includes a trailing null. Don't read the length into the buffer.
    if (fread(&len, sizeof(len), 1, fp) != 1) {
        dberror(db, R_ERR_CANT_READ, "Corrupt database journal");
        return 0;
    }
    if (len < 0 || len > DB_MAX_ITEM) {
        dberror(db, R_ERR_CANT_READ, "Corrupt database journal");
        return 0;
    }
    if (fread(rGetBufStart(buf), len, 1, fp) != 1) {
        dberror(db, R_ERR_CANT_READ, "Corrupt database journal");
        return 0;
    }
    result = rGetBufStart(buf);
    rAdjustBufEnd(buf, len);
    rAdjustBufStart(buf, rGetBufLength(buf));
    return result;
}

static int applyChange(Db *db, cchar *cmd, cchar *model, cchar *value)
{
    Json *json;

    if ((json = jsonParse(value, 0)) == 0) {
        return dberror(db, R_ERR_CANT_READ, "Cannot parse json from journal file");
    }
    if (json->count == 0) {
        jsonFree(json);
        return dberror(db, R_ERR_CANT_READ, "Empty json from journal file");
    }
    if (sends(cmd, "create")) {
        dbCreate(db, model, json, DB_PARAMS(.bypass = 1));

    } else if (sends(cmd, "remove")) {
        dbRemove(db, model, json, DB_PARAMS(.bypass = 1));

    } else if (sends(cmd, "update")) {
        dbUpdate(db, model, json, DB_PARAMS(.bypass = 1));

    } else if (sends(cmd, "upsert")) {
        dbUpdate(db, model, json, DB_PARAMS(.bypass = 1, .upsert = 1));
    }
    jsonFree(json);
    return 0;
}

PUBLIC void dbAddContext(Db *db, cchar *name, cchar *value)
{
    jsonSet(db->context, 0, name, value, 0);
}

PUBLIC void dbAddCallback(Db *db, DbCallbackProc proc, cchar *model, void *arg, int events)
{
    DbCallback *cb;

    if ((cb = rAllocType(DbCallback)) == 0) {
        rError("db", "Cannot add callback");
        return;
    }
    cb->proc = proc;
    cb->arg = arg;
    cb->model = model ? sclone(model) : NULL;
    cb->events = events;
    rAddItem(db->callbacks, cb);
}

PUBLIC void dbRemoveCallback(Db *db, DbCallbackProc proc, cchar *model, void *arg)
{
    DbCallback *cb;
    int        ci;

    for (ITERATE_ITEMS(db->callbacks, cb, ci)) {
        if (cb->proc == proc && cb->arg == arg && model == cb->model) {
            rFree(cb->model);
            rRemoveItem(db->callbacks, cb);
            break;
        }
    }
}

static void invokeCallbacks(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                            int event)
{
    DbCallback *cb;
    int        ci;

    for (ITERATE_ITEMS(db->callbacks, cb, ci)) {
        if (!cb->model || smatch(cb->model, model->name)) {
            if (cb->events & event) {
                cb->proc(cb->arg, db, model, item, params, cmd, event);
            }
        }
    }
}

PUBLIC void dbSetJournalParams(Db *db, Ticks delay, ssize maxSize)
{
    db->maxJournalAge = delay;
    db->maxJournalSize = maxSize;
}

static void addContext(Db *db, Json *props)
{
    jsonBlend(props, 0, 0, db->context, 0, 0, 0);
}

/*
    Compare the sort key of an item.
    This is the Red/black index comparision callback.
    Env contains the API environment - (beginsWith).
 */
static int compareItems(cvoid *n1, cvoid *n2, Env *env)
{
    DbItem *d1, *d2;

    if (!n1 || !n2) {
        return R_ERR_BAD_ARGS;
    }
    d1 = (DbItem*) n1;
    d2 = (DbItem*) n2;

    if (d1->key == NULL) {
        if (d2->key == NULL) {
            return 0;
        } else {
            return -1;
        }
    } else if (d2->key == NULL) {
        return 1;
    }
    if (env && env->compare && smatch(env->compare, "begins")) {
        //  The d1->key length was precomputed in searchLen
        return strncmp(d1->key, d2->key, env->searchLen);
    }
    return strcmp(d1->key, d2->key);
}

PUBLIC cchar *dbGetError(Db *db)
{
    if (!db) {
        return NULL;
    }
    return db->error;
}

/*
    Check a value is one of the valid enum values
 */
static bool checkEnum(DbField *field, cchar *value)
{
    cchar *cp;

    if (!field || !value) {
        return 0;
    }
    if ((cp = scontains(field->enums, value)) == 0) {
        return 0;
    }
    if ((cp[-1] != '"' && cp[-1] != '\'') || (cp[slen(value)] != '"' && cp[slen(value)] != '\'')) {
        return 0;
    }
    return 1;
}

static cchar *LETTERS = "0123456789ABCDEFGHJKMNPQRSTVWXYZZ";

/*
    Generate a ULID identifier. Universal sortable ID.
 */
PUBLIC char *dbGetULID(Time when)
{
    char bytes[16 + 1], time[10 + 1];
    Time mark;
    int  i, index, length, mod;

    length = (int) slen(LETTERS) - 1;
    if (cryptGetRandomBytes((uchar*) bytes, sizeof(bytes), 1) < 0) {
        return 0;
    }
    for (i = 0; i < sizeof(bytes) - 1; i++) {
        index = (int) floor((uchar) bytes[i] * length / 0xFF);
        bytes[i] = LETTERS[index];
    }
    bytes[sizeof(bytes) - 1] = '\0';

    mark = when;
    for (i = sizeof(time) - 2; i >= 0; i--) {
        mod = (int) (mark % length);
        time[i] = LETTERS[mod];
        mark = (mark - mod) / length;
    }
    time[sizeof(time) - 1] = '\0';
    return sjoin(time, bytes, 0);
}

PUBLIC char *dbGetUID(ssize size)
{
    return cryptID(size);
}

/*
    Load data from a JSON file -- Useful for development and initial migrations
 */
PUBLIC int dbLoadData(Db *db, cchar *path)
{
    Json *json;
    char *errorMsg;
    int  rc;

    if ((json = jsonParseFile(path, &errorMsg, 0)) == 0) {
        dberror(db, R_ERR_CANT_READ, "%s", errorMsg);
        rFree(errorMsg);
        return R_ERR_CANT_READ;
    }
    rc = dbLoadDataItems(db, json, NULL);
    jsonFree(json);
    return rc;
}

PUBLIC int dbLoadDataItems(Db *db, Json *json, JsonNode *parent)
{
    Json     *props;
    JsonNode *item, *model;
    char     *str;

    for (ITERATE_JSON(json, parent, model, mid)) {
        for (ITERATE_JSON(json, model, item, id)) {
            str = jsonToString(json, id, 0, 0);
            props = jsonParse(str, 0);
            if (!dbFindOne(db, model->name, props, NULL)) {
                if (dbCreate(db, model->name, props, NULL) == 0) {
                    return dberror(db, R_ERR_CANT_COMPLETE, "Cannot create item for %s. %s", str, db->error);
                }
            }
            jsonFree(props);
            rFree(str);
        }
    }
    return 0;
}

PUBLIC void dbReset(cchar *path)
{
    char pbuf[ME_MAX_FNAME];

    unlink(path);
    unlink(sfmtbuf(pbuf, sizeof(pbuf), "%s.jnl", path));
}

PUBLIC cchar *dbType(Db *db)
{
    return db->type;
}

PUBLIC cchar *dbNext(Db *db, RList *list)
{
    if (list->length == 0) {
        return 0;
    }
    return dbField(list->items[list->length - 1], "sk");
}

static DbChange *allocChange(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                             Ticks due)
{
    DbChange *change;

    if ((change = rAllocType(DbChange)) == 0) {
        return 0;
    }
    change->db = db;
    change->model = model;
    change->params = params;
    change->cmd = cmd;
    change->due = due;
    change->key = sclone(item->key);
    rAddName(db->changes, item->key, change, 0);
    return change;
}

static void freeChange(Db *db, DbChange *change)
{
    rRemoveName(db->changes, change->key);
    rFree(change->key);
    rFree(change);
}

/*
    Set and log an error. Emits a message at the "trace" level.
 */
static int dberror(Db *db, int code, cchar *fmt, ...)
{
    va_list ap;
    char    *msg;

    if (!db || !code || !fmt) {
        return R_ERR_BAD_ARGS;
    }
    va_start(ap, fmt);
    msg = sfmtv(fmt, ap);
    db->code = code;

    rFree(db->error);
    db->error = msg;

    rTrace("db", "%s", msg);
    va_end(ap);
    return code;
}
#endif /* ME_COM_DB */

#else
void dummyDb(){}
#endif /* ME_COM_DB */
