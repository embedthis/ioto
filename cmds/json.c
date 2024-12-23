/*
    json.c -- JSON parsing and query program

    Examples:

    json [options] [cmd] file
    json --stdin [cmd] <file
    json field=value    # assign
    json field          # query
    json .              # convert

    Options:
    --blend | --check | --compress | --default | --env | --header |
    --json | --json5 | --profile name | --remove | --stdin

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/******************************** Includes ***********************************/

#define ME_COM_JSON 1
#define ME_COM_R    1

#include    "osdep.h"
#include    "r.h"
#include    "json.h"

/*********************************** Locals ***********************************/
/*
    Default trace filters for -v and --debug
 */
#define TRACE_FILTER         "stderr:raw,error,info,!trace,!debug:all,!mbedtls"
#define TRACE_QUIET_FILTER   "stderr:!error,!info,!trace,!debug:all,!mbedtls"
#define TRACE_VERBOSE_FILTER "stderr:raw,error,info,trace,debug:all,!mbedtls"
#define TRACE_DEBUG_FILTER   "stderr:all:all"
#define TRACE_FORMAT         "%S: %T: %M"

#define JSON_FORMAT_COMPRESS 1
#define JSON_FORMAT_ENV      1
#define JSON_FORMAT_HEADER   2
#define JSON_FORMAT_JSON     3
#define JSON_FORMAT_JSON5    4

#define JSON_CMD_ASSIGN      1
#define JSON_CMD_CONVERT     2
#define JSON_CMD_QUERY       3
#define JSON_CMD_REMOVE      4

static cchar *defaultValue;
static Json  *json;
static char  *path;
static cchar *profile;
static char  *property;
static cchar *trace;                   /* Trace spec */

static int blend;
static int check;
static int cmd;
static int compress;
static int format;
static int newline;
static int overwrite;
static int noerror;
static int quiet;
static int stdinput;

/***************************** Forward Declarations ***************************/

static int blendFiles(Json *json);
static int mergeConditionals(Json *json, cchar *property);
static void cleanup(void);
static int error(cchar *fmt, ...);
static char *makeName(cchar *name);
static ssize mapChars(char *dest, cchar *src);
static int parseArgs(int argc, char **argv);
static void output(Json *json, JsonNode *parent, char *base);
static char *readInput();
static int run();

/*********************************** Code *************************************/

static int usage(void)
{
    rFprintf(stderr, "usage: json [options] [cmd] [file | <file]\n"
             "  Options:\n"
             "  --blend          # Blend included files from blend[].\n"
             "  --check          # Check syntax with no output.\n"
             "  --compress       # Emit without redundant white space.\n"
             "  --default value  # Default value to use if query not found.\n"
             "  --env            # Emit query result as shell env vars.\n"
             "  --header         # Emit query result as C header defines.\n"
             "  --json           # Emit output in JSON form.\n"
             "  --json5          # Emit output in JSON5 form (default).\n"
             "  --noerror        # Ignore file open errors.\n"
             "  --profile name   # Merge the properties from the named profile.\n"
             "  --quiet          # Quiet mode with no error messages.\n"
             "  --stdin          # Read from stdin.\n"
             "  --remove         # Remove queried property.\n"
             "  --overwrite      # Overwrite file when converting instead of stdout.\n"
             "\n"
             "  Commands:\n"
             "  property=value   # Set queried property.\n"
             "  property         # Query property (can be dotted property).\n"
             "  .                # Convert input to desired format\n\n");
    return R_ERR_BAD_ARGS;
}

int main(int argc, char **argv, char **envp)
{
    int rc;

    if (rInit(NULL, NULL) < 0) {
        rFprintf(stderr, "Cannot initialize runtime");
        exit(2);
    }
    if (parseArgs(argc, argv) < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (trace && rSetLog(trace, 0, 1) < 0) {
        error("Cannot open trace %s", trace);
        exit(1);
    }
    rSetLogFormat(TRACE_FORMAT, 1);
    rc = run();
    cleanup();
    rTerm();
    return rc;
}

void cleanup(void)
{
    rFree(path);
    rFree(property);
    jsonFree(json);
}

static int parseArgs(int argc, char **argv)
{
    char *argp;
    int  nextArg;

    cmd = 0;
    format = JSON_FORMAT_JSON5;
    newline = 1;
    path = 0;
    trace = TRACE_FILTER;

    for (nextArg = 1; nextArg < argc; nextArg++) {
        argp = argv[nextArg];
        if (*argp != '-') {
            break;
        }
        if (smatch(argp, "--blend")) {
            blend = 1;

        } else if (smatch(argp, "--check")) {
            check = 1;
            cmd = JSON_CMD_QUERY;

        } else if (smatch(argp, "--compress")) {
            compress = 1;

        } else if (smatch(argp, "--debug") || smatch(argp, "-d")) {
            trace = TRACE_DEBUG_FILTER;

        } else if (smatch(argp, "--default")) {
            if (nextArg >= argc) {
                return usage();
            } else {
                defaultValue = argv[++nextArg];
            }

        } else if (smatch(argp, "--env")) {
            format = JSON_FORMAT_ENV;

        } else if (smatch(argp, "--header")) {
            format = JSON_FORMAT_HEADER;

        } else if (smatch(argp, "--json")) {
            format = JSON_FORMAT_JSON;

        } else if (smatch(argp, "--json5")) {
            format = JSON_FORMAT_JSON5;

        } else if (smatch(argp, "--noerror") || smatch(argp, "-n")) {
            noerror = 1;

        } else if (smatch(argp, "--overwrite")) {
            overwrite = 1;

        } else if (smatch(argp, "--profile")) {
            if (nextArg >= argc) {
                usage();
            }
            profile = argv[++nextArg];

        } else if (smatch(argp, "--quiet") || smatch(argp, "-q")) {
            quiet = 1;
            trace = TRACE_QUIET_FILTER;

        } else if (smatch(argp, "--remove")) {
            cmd = JSON_CMD_REMOVE;

        } else if (smatch(argp, "--stdin")) {
            stdinput = 1;

        } else if (smatch(argp, "--trace") || smatch(argp, "-t")) {
            if (nextArg >= argc) {
                return usage();
            } else {
                trace = argv[++nextArg];
            }

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            trace = TRACE_VERBOSE_FILTER;

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            rPrintf("%s\n", ME_VERSION);
            exit(0);

        } else if (smatch(argp, "--")) {
            nextArg++;
            break;

        } else {
            return usage();
        }
    }
    if (argc == nextArg) {
        return usage();
    }
    property = sclone(argv[nextArg++]);
    if (!cmd) {
        if (smatch(property, ".")) {
            cmd = JSON_CMD_CONVERT;
        } else if (schr(property, '=')) {
            cmd = JSON_CMD_ASSIGN;
        } else {
            cmd = JSON_CMD_QUERY;
        }
    }
    if (argc == nextArg) {
        if (check) {
            //  Special case to allow "json --check file"
            path = property;
            property = sclone(".");
        } else if (stdinput) {
            path = NULL;
        } else {
            return usage();
        }
    } else if (argc == nextArg + 1) {
        path = sclone(argv[nextArg]);
    } else {
        return usage();
    }
    return 0;
}

static int run()
{
    JsonNode *node;
    char     *data, *str, *value;
    int      flags;

    if ((data = readInput()) == 0) {
        return R_ERR_CANT_READ;
    }
    json = jsonParse(data, JSON_PASS_TEXT);
    if (json == 0) {
        error("Cannot parse input");
        return R_ERR_CANT_READ;
    }
    if (blend) {
        if (blendFiles(json) < 0) {
            return R_ERR_CANT_READ;
        }
    }
    if (profile) {
        if (mergeConditionals(json, profile) < 0) {
            return R_ERR_CANT_READ;
        }
    }
    flags = 0;
    if (format == JSON_FORMAT_JSON) {
        flags |= JSON_STRICT;
    }
    if (compress) {
        flags |= JSON_SINGLE;
    } else {
        flags |= JSON_PRETTY;
    }
    if (cmd == JSON_CMD_ASSIGN) {
        stok(property, "=", &value);
        if (jsonSet(json, 0, property, value, 0) < 0) {
            return error("Cannot assign to \"%s\"", property);
        }
        if (jsonSave(json, 0, NULL, path, 0, flags) < 0) {
            return error("Cannot save \"%s\"", path);
        }

    } else if (cmd == JSON_CMD_REMOVE) {
        if (jsonRemove(json, 0, property) < 0) {
            return error("Cannot remove property \"%s\"", property);
        }
        if (jsonSave(json, 0, NULL, path, 0, flags) < 0) {
            return error("Cannot save \"%s\"", path);
        }

    } else if (cmd == JSON_CMD_QUERY) {
        if (!check) {
            node = jsonGetNode(json, 0, property);
            output(json, node, property);
        }

    } else if (cmd == JSON_CMD_CONVERT) {
        if (overwrite) {
            if (jsonSave(json, 0, NULL, path, 0, flags) < 0) {
                return error("Cannot save \"%s\"", path);
            }
        } else if (!check) {
            str = jsonToString(json, 0, 0, flags);
            rPrintf("%s\n", str);
            rFree(str);
        }
    }
    return 0;
}

static int blendFiles(Json *json)
{
    Json     *inc;
    JsonNode *item;
    char     *dir, *err, *file;
    int      bid, nid;

    bid = jsonGetId(json, 0, "blend");
    if (bid < 0) {
        return 0;
    }
    for (ITERATE_JSON_DYNAMIC(json, bid, item, nid)) {
        if (path && *path) {
            dir = rDirname(sclone(path));
            file = sjoin(dir, "/", item->value, NULL);
            rFree(dir);
        } else {
            file = sclone(item->value);
        }
        if ((inc = jsonParseFile(file, &err, 0)) == 0) {
            return error("Cannot parse %s: %s", file, err);
        }
        if (jsonBlend(json, 0, 0, inc, 0, 0, JSON_COMBINE) < 0) {
            return error("Cannot blend %s", file);
        }
        rFree(file);
    }
    jsonRemove(json, 0, "blend");
    return 0;
}

static int mergeConditionals(Json *json, cchar *property)
{
    JsonNode *collection;
    cchar    *value;
    int      cid, nid, set;

    cid = jsonGetId(json, 0, "conditional");
    if (cid < 0) {
        return 0;
    }
    for (ITERATE_JSON_DYNAMIC(json, cid, collection, nid)) {
        //  Collection name: profile
        value = 0;
        if (smatch(collection->name, "profile")) {
            if ((value = profile) == 0) {
                value = jsonGet(json, 0, "profile", "dev");
            }
        }
        if (!value) {
            value = jsonGet(json, 0, collection->name, 0);
        }
        if (value) {
            set = jsonGetId(json, jsonGetNodeId(json, collection), value);
            if (set >= 0) {
                //  WARNING: property references are not stable over a blend
                if (jsonBlend(json, 0, 0, json, set, 0, JSON_COMBINE) < 0) {
                    return error("Cannot blend %s", collection->name);
                }
            }
        }
    }
    jsonRemove(json, 0, "conditional");
    return 0;
}

static char *readInput()
{
    char  *buf;
    ssize bytes, pos;

    if (path) {
        if (!rFileExists(path)) {
            if (noerror) {
                return sclone("{}");
            }
            error("Cannot locate file %s", path);
        }
        return rReadFile(path, NULL);
    } else {
        buf = malloc(ME_BUFSIZE + 1);
        pos = 0;
        do {
            buf = realloc(buf, pos + ME_BUFSIZE + 1);
            bytes = fread(&buf[pos], 1, sizeof(buf), stdin);
            pos += bytes;
        } while (bytes > 0);
        buf[pos] = '\0';
    }
    return buf;
}

static void output(Json *json, JsonNode *node, char *name)
{
    JsonNode *child;
    cchar    *value;
    char     *property;
    int      id, type;

    if (node) {
        value = node->value;
        type = node->type;
        if (node->type == JSON_ARRAY || node->type == JSON_OBJECT) {
            for (ITERATE_JSON(json, node, child, id)) {
                property = sjoin(name, ".", child->name, NULL);
                output(json, child, property);
                rFree(property);
            }
            return;
        }
    } else if (defaultValue) {
        value = defaultValue;
        type = JSON_PRIMITIVE;
    } else {
        error("Cannot find property \"%s\"", name);
        return;
    }
    property = makeName(name);
    if (format == JSON_FORMAT_ENV) {
        if (type & JSON_STRING) {
            rPrintf("%s='%s'", property, value);
        } else {
            rPrintf("%s=%s", property, value);
        }
    } else if (format == JSON_FORMAT_HEADER) {
        if (smatch(value, "true")) {
            rPrintf("#define %s 1", property);
        } else if (smatch(value, "false")) {
            rPrintf("#define %s 0", property);
        } else {
            rPrintf("#define %s \"%s\"", property, value);
        }
    } else if (format == JSON_FORMAT_JSON) {
        rPrintf("%s", value);
    } else if (format == JSON_FORMAT_JSON5) {
        rPrintf("%s", value);
    }
    if (newline) {
        rPrintf("\n");
    }
    fflush(stdout);
    rFree(property);
}

static char *makeName(cchar *name)
{
    char  *buf;
    ssize len;

    len = slen(name) * 2 + 1;
    buf = rAlloc(len);
    mapChars(buf, name);
    return buf;
}

static ssize mapChars(char *dest, cchar *src)
{
    char *dp;

    for (dp = dest; *src; src++, dp++) {
        if (isupper(*src)) {
            *dp++ = '_';
        }
        if (*src == '.') {
            *dp = '_';
        } else {
            *dp = toupper(*src);
        }
    }
    *dp = '\0';
    return dp - dest;
}

static int error(cchar *fmt, ...)
{
    va_list args;
    char    *msg;

    assert(fmt);

    if (!quiet) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        va_end(args);
        if (json && json->errorMsg) {
            rError("json", "%s: %s", msg, json->errorMsg);
        } else {
            rError("json", "%s", msg);
        }
        rFree(msg);
    }
    return R_ERR_CANT_COMPLETE;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
