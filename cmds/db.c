/*
    db.c -- Database management

    Usage:
        db [options] database
        db database model
        db database model [prop:value, ...]
        db database model [prop:value] prop=value...
 */

/********************************* Includes ***********************************/

#define ME_COM_DB 1
#define ME_COM_R  1

#include    "r.h"
#include    "db.h"

/********************************** Locals ************************************/

static RList *fields = NULL;
static bool  show = 0;

/********************************** Forwards **********************************/

static void emit(const DbItem *item);

/*********************************** Code *************************************/

static void usage(void)
{
    rFprintf(stderr, "usage: db [options] database [model] [key] [props:value...] [props=value...]\n"
             "Options:\n"
             "    --expire                Remove all expired items\n"
             "    --load data.json        Load the database with the given file\n"
             "    --show                  Show full updated item\n"
             "    --reset                 Reset the database\n"
             "    --schema schema.json    Database schema\n"
             "Commands:\n"
             "    model\n"
             "    model [prop:value,...] [props=value]\n"
             "\n\n");
    exit(1);
}

PUBLIC int main(int argc, char *argv[])
{
    Db           *db;
    Json         *props, *result;
    const RList  *grid;
    const DbItem *item;
    DbModel      *model;
    RbNode       *rp;
    cchar        *argp, *load, *name, *path, *schema;
    char         *prop, *value;
    int          argind, expire, flags, index, reset, update;

    fields = rAllocList(0, 0);
    expire = 0;
    load = 0;
    model = 0;
    path = 0;
    reset = 0;
    schema = 0;
    update = 0;

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;
        }
        if (smatch(argp, "--load")) {
            if (argind >= argc) {
                usage();
            }
            load = argv[++argind];

        } else if (smatch(argp, "--expire")) {
            expire = 1;

        } else if (smatch(argp, "--reset")) {
            reset = 1;

        } else if (smatch(argp, "--schema")) {
            if (argind >= argc) {
                usage();
            }
            schema = argv[++argind];

        } else if (smatch(argp, "--show") || smatch(argp, "-s")) {
            show = 1;

        } else {
            usage();
        }
    }
    if (argind >= argc) {
        usage();
    }
    if (!schema && !load && !reset) {
        rFprintf(stderr, "db: Must specify --schema, --load or --reset\n");
        exit(1);
    }
    if (rInit(0, 0) < 0) {
        rFprintf(stderr, "db: Cannot initialize runtime\n");
        exit(1);
    }
    path = argv[argind++];
    if (reset) {
        dbReset(path);
    }

    /*
        Open read-only if possible. i.e. not modifying properties and not loading or reset.
        Helpful if examining a running database.
     */
    flags = ((argind + 1) < argc || load || reset) ? 0 : DB_READ_ONLY;

    if ((db = dbOpen(path, schema, flags)) == 0) {
        rFatal("db", "Cannot open database");
    }
    if (load) {
        if (dbLoadData(db, load) < 0) {
            rFatal("db", "Cannot load data: %s", db->error);
        }

    } else if (expire) {
        dbRemoveExpired(db, 0);

    } else {
        props = jsonAlloc(0);
        if (argind < argc) {
            if ((model = dbGetModel(db, argv[argind++])) == NULL) {
                rFatal("db", "Cannot find model");
            }
        }
        if (!model) {
            //  Entire database
            for (rp = rbFirst(db->primary); rp; rp = rbNext(db->primary, rp)) {
                emit(rp->data);
            }
        } else if (argind == argc) {
            //  Entire model
            grid = dbFind(db, model->name, props, NULL);
            result = jsonParse("[]", 0);
            for (ITERATE_ITEMS(grid, item, index)) {
                jsonBlend(result, 0, "[$]", dbJson(item), 0, 0, 0);
            }
            rPrintf("%s\n", jsonString(result, JSON_PRETTY));
            jsonFree(result);

        } else {
            //  One or more model items
            while (argind < argc) {
                prop = argv[argind++];
                if (scontains(prop, "=")) {
                    //  Assignment
                    name = stok(prop, "=", &value);
                    jsonSet(props, 0, name, value, 0);
                    update = 1;
                } else if (scontains(prop, ":")) {
                    //  query
                    name = stok(prop, ":", &value);
                    jsonSet(props, 0, name, value, 0);
                } else {
                    rAddItem(fields, prop);
                }
            }
            if (update) {
                if ((item = dbUpdate(db, model->name, props, DB_PARAMS(.upsert = 1))) == NULL) {
                    rFatal("db", "Cannot update %s", model->name);
                }
                if (show) {
                    emit(item);
                }
            } else {
                if ((grid = dbFind(db, model->name, props, NULL)) == NULL) {
                    rFatal("db", "Cannot find model items");
                }
                if (rGetListLength(grid) == 1) {
                    emit(rGetItem((RList*) grid, 0));
                } else {
                    result = jsonParse("[]", 0);
                    for (ITERATE_ITEMS(grid, item, index)) {
                        jsonBlend(result, 0, "[$]", dbJson(item), 0, 0, 0);
                    }
                    rPrintf("%s\n", jsonString(result, JSON_PRETTY));
                    rFree(result);
                }
            }
        }
        jsonFree(props);
    }
    if (load || reset || update) {
        dbSave(db, NULL);
    }
    dbClose(db);
    rTerm();
    return 0;
}

static void emit(const DbItem *item)
{
    Json  *json;
    cchar *field;
    int   index;

    if (fields && rGetListLength(fields) > 0) {
        for (ITERATE_ITEMS(fields, field, index)) {
            rPrintf("%s\n", dbField(item, field));
        }
    } else {
        json = jsonParse(item->value, 0);
        jsonPrint(json);
        jsonFree(json);
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
