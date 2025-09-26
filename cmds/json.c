/*
    json.c -- JSON parsing and query program

    Examples:

    json [options] [cmd] file
    json <file
    json file
    json --overwrite file
    json --stdin [options] [cmd] <file

    Commands:
    json field=value            # assign
    json field                  # query
    json --remove field         # remove field
    json <options> <no-args>    # convert formats

    Options:
    --blend | --bump | --check | --default | --double | --encode | --env | --expand | --export |
    --header | --indent | --js | --json | --json5 | --keys | --length | --one | --profile name |
    --overwrite |--remove | --stdin | --strict | --trace | --verbose | --version

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

#define JSON_FORMAT_ENV      1
#define JSON_FORMAT_HEADER   2
#define JSON_FORMAT_JSON     3
#define JSON_FORMAT_JSON5    4
#define JSON_FORMAT_JS       5

#define JSON_CMD_ASSIGN      1
#define JSON_CMD_CONVERT     2
#define JSON_CMD_QUERY       3
#define JSON_CMD_REMOVE      4
#define JSON_CMD_BUMP        5

static cchar *defaultValue;
static char  *bump;
static Json  *json;
static char  *path;
static cchar *profile;
static char  *property;
static cchar *trace;                   // Trace spec

static int blend = 0;
static int check = 0;
static int cmd = 0;
static int compact = 0;     // Compact output format
static int encode = 0;
static int expand = 0;
static int export = 0;
static int format = JSON_MAX_LINE_LENGTH;
static int keys = 0;
static int multiline = 1;
static int newline = 1;     // Add a trailing newline to the output
static int noerror = 0;
static int overwrite = 0;
static int quiet = 0;
static int quotes = 0;      // 1 = single quotes, 2 = double quotes, 0 = use default
static int stdinput = 0;
static int strict = 0;      // Strict JSON parsing

/***************************** Forward Declarations ***************************/

static int blendFiles(Json *json);
static int bumpVersion(Json *json, cchar *property);
static int mergeConditionals(Json *json, cchar *property);
static void cleanup(void);
static int error(cchar *fmt, ...);
static char *makeName(cchar *name);
static ssize mapChars(char *dest, cchar *src);
static int parseArgs(int argc, char **argv);
static void outputAll(Json *json, int flags);
static void outputNode(Json *json, JsonNode *node, char *name, int flags);
static void outputProperty(Json *json, cchar *name, cchar *value, int type);
static char *readInput();
static int run();

/*********************************** Code *************************************/

static int usage(void)
{
    rFprintf(stderr, "usage: json [options] [cmd] [file | <file]\n"
             "  Options:\n"
             "  --blend          # Blend included files from blend[].\n"
             "  --bump property  # Bump version property.\n"
             "  --check          # Check syntax with no output.\n"
             "  --compact        # Emit with minimal whitespace.\n"
             "  --default value  # Default value to use if query not found.\n"
             "  --double         # Use double quotes (default with JSON and JS).\n"
             "  --encode         # Encode control characters.\n"
             "  --env            # Emit query result as shell env vars.\n"
             "  --expand         # Expand ${var} references in output.\n"
             "  --export         # Add 'export' prefix to shell env vars.\n"
             "  --header         # Emit query result as C header defines.\n"
             "  --indent num     # Set indent level for compacted output.\n"
             "  --js             # Emit output in JS form (export {}).\n"
             "  --json           # Emit output in JSON form.\n"
             "  --json5          # Emit output in JSON5 form (default).\n"
             "  --length num     # Set line length limit for compacted output.\n"
             "  --keys           # Emit propty key names only.\n"
             "  --noerror        # Ignore errors.\n"
             "  --one            # Emit on one line.\n"
             "  --overwrite      # Overwrite file when converting instead of stdout.\n"
             "  --profile name   # Merge the properties from the named profile.\n"
             "  --quiet          # Quiet mode with no error messages.\n"
             "  --remove         # Remove queried property.\n"
             "  --single         # Use single quotes (default with JSON5).\n"
             "  --stdin          # Read from stdin (default if no file specified).\n"
             "  --strict         # Perform strict JSON standard parsing of input.\n"
             "\n"
             "  Commands:\n"
             "  property=value   # Set queried property.\n"
             "  property         # Query property (can be dotted property).\n"
             "                   # If not command, then convert input to desired format\n\n");
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
    format = 0;
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

        } else if (smatch(argp, "--bump")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                bump = argv[++nextArg];
            }

        } else if (smatch(argp, "--check")) {
            check = 1;
            cmd = JSON_CMD_QUERY;

        } else if (smatch(argp, "--compact") || smatch(argp, "-c")) {
            compact = 1;

        } else if (smatch(argp, "--debug") || smatch(argp, "-d")) {
            trace = TRACE_DEBUG_FILTER;

        } else if (smatch(argp, "--default")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                defaultValue = argv[++nextArg];
            }

        } else if (smatch(argp, "--double")) {
            quotes = 2;

        } else if (smatch(argp, "--expand")) {
            expand = 1;

        } else if (smatch(argp, "--encode")) {
            encode = 1;

        } else if (smatch(argp, "--env")) {
            format = JSON_FORMAT_ENV;

        } else if (smatch(argp, "--export")) {
            export = 1;

        } else if (smatch(argp, "--header")) {
            format = JSON_FORMAT_HEADER;

        } else if (smatch(argp, "--indent")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                jsonSetIndent(atoi(argv[++nextArg]));
            }

        } else if (smatch(argp, "--js")) {
            format = JSON_FORMAT_JS;

        } else if (smatch(argp, "--json")) {
            format = JSON_FORMAT_JSON;

        } else if (smatch(argp, "--json5")) {
            format = JSON_FORMAT_JSON5;

        } else if (smatch(argp, "--noerror") || smatch(argp, "-n")) {
            noerror = 1;

        } else if (smatch(argp, "--keys")) {
            keys = 1;

        } else if (smatch(argp, "--length")) {
            if (nextArg + 1 >= argc) {
                return usage();
            } else {
                jsonSetMaxLength(atoi(argv[++nextArg]));
            }

        } else if (smatch(argp, "--one")) {
            multiline = 0;

        } else if (smatch(argp, "--overwrite") || smatch(argp, "-o")) {
            overwrite = 1;

        } else if (smatch(argp, "--profile")) {
            if (nextArg + 1 >= argc) {
                usage();
            }
            profile = argv[++nextArg];

        } else if (smatch(argp, "--quiet") || smatch(argp, "-q")) {
            quiet = 1;
            trace = TRACE_QUIET_FILTER;

        } else if (smatch(argp, "--remove")) {
            cmd = JSON_CMD_REMOVE;

        } else if (smatch(argp, "--single")) {
            quotes = 1;

        } else if (smatch(argp, "--stdin")) {
            stdinput = 1;

        } else if (smatch(argp, "--strict") || smatch(argp, "-s")) {
            strict = 1;

        } else if (smatch(argp, "--trace") || smatch(argp, "-t")) {
            if (nextArg + 1 >= argc) {
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
    if (bump) {
        cmd = JSON_CMD_BUMP;
    } else if (argc == nextArg) {
        //  No args
        cmd = JSON_CMD_CONVERT;
        stdinput = 1;
    } else if (argc == nextArg + 1) {
        //  File only
        cmd = JSON_CMD_CONVERT;
    } else {
        property = sclone(argv[nextArg++]);
        if (property == NULL) {
            return error("Cannot allocate memory for property");
        }
    }
    if (!cmd) {
        if (schr(property, '=')) {
            cmd = JSON_CMD_ASSIGN;
        } else if (property) {
            // MOB if no property, should be convert
            cmd = JSON_CMD_QUERY;
        } else {
            cmd = JSON_CMD_CONVERT;
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
    cchar    *ext;
    char     *data, *value;
    int      flags;

    if ((data = readInput()) == 0) {
        return R_ERR_CANT_READ;
    }
    if (!format) {
        if (path) {
            ext = rGetFileExt(path);
            if (scaselessmatch(ext, "json")) {
                format = JSON_FORMAT_JSON;
            } else if (scaselessmatch(ext, "json5")) {
                format = JSON_FORMAT_JSON5;
            } else if (scaselessmatch(ext, "js")) {
                format = JSON_FORMAT_JS;
            } else {
                format = JSON_FORMAT_JSON5;
            }
        } else {
            format = JSON_FORMAT_JSON5;
        }
    }
    flags = 0;
    if (strict) {
        if (!multiline) {
            error("Cannot use --one with --strict mode");
        }
        flags |= JSON_STRICT_PARSE | JSON_JSON;
    }
    json = jsonAlloc();
    if (jsonParseText(json, data, flags) < 0) {
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
    /*
        Set the output flags
     */
    if (format == JSON_FORMAT_JSON || strict) {
        flags |= JSON_JSON;
    } else if (format == JSON_FORMAT_JSON5) {
        flags |= JSON_JSON5;
    } else if (format == JSON_FORMAT_JS) {
        flags |= JSON_JS;
    }
    if (compact) {
        flags |= JSON_COMPACT;
    }
    if (encode) {
        flags |= JSON_ENCODE;
    }
    if (expand) {
        flags |= JSON_EXPAND;
    }
    if (multiline) {
        flags |= JSON_MULTILINE;
    }
    if (quotes == 1) {
        flags &= ~JSON_DOUBLE_QUOTES;
        flags |= JSON_SINGLE_QUOTES;
    } else if (quotes == 2) {
        flags &= ~JSON_SINGLE_QUOTES;
        flags |= JSON_DOUBLE_QUOTES;
    }

    if (cmd == JSON_CMD_ASSIGN) {
        stok(property, "=", &value);
        if (jsonSet(json, 0, property, value, 0) < 0) {
            return error("Cannot assign to \"%s\"", property);
        }
        if (overwrite) {
            if (jsonSave(json, 0, NULL, path, 0, flags) < 0) {
                return error("Cannot save \"%s\"", path);
            }
        } else {
            outputAll(json, flags);
        }
    } else if (cmd == JSON_CMD_BUMP) {
        if (bumpVersion(json, bump) < 0) {
            return error("Cannot bump property \"%s\"", bump);
        }
        if (overwrite) {
            if (jsonSave(json, 0, NULL, path, 0, flags) < 0) {
                return error("Cannot save \"%s\"", path);
            }
        } else {
            outputAll(json, flags);
        }

    } else if (cmd == JSON_CMD_REMOVE) {
        if (jsonRemove(json, 0, property) < 0) {
            if (noerror) {
                return 0;
            }
            return error("Cannot remove property \"%s\"", property);
        }
        if (overwrite) {
            if (jsonSave(json, 0, NULL, path, 0, flags) < 0) {
                return error("Cannot save \"%s\"", path);
            }
        } else {
            outputAll(json, flags);
        }

    } else if (cmd == JSON_CMD_QUERY) {
        if (!check) {
            node = jsonGetNode(json, 0, property);
            outputNode(json, node, property, flags);
        }

    } else if (cmd == JSON_CMD_CONVERT) {
        if (overwrite) {
            if (jsonSave(json, 0, NULL, path, 0, flags) < 0) {
                return error("Cannot save \"%s\"", path);
            }
        } else if (!check) {
            outputAll(json, flags);
        }
    }
    return 0;
}

static int bumpVersion(Json *json, cchar *property)
{
    cchar *dot, *version;
    char  vbuf[80], *vp;
    int   num;

    version = jsonGet(json, 0, property, 0);
    if (!version) {
        return R_ERR_BAD_ARGS;
    }
    if ((dot = strrchr(version, '.')) != NULL) {
        sncopy(vbuf, sizeof(vbuf), version, dot - version);
        num = (int) stoi(dot + 1) + 1;
        vp = sfmt("%s.%d", vbuf, num);
        jsonSet(json, 0, property, vp, 0);
        rFree(vp);

    } else if (sfnumber(version)) {
        jsonSetNumber(json, 0, property, stoi(version) + 1);
    } else {
        return R_ERR_BAD_ARGS;
    }
    return 0;
}

static int blendFiles(Json *json)
{
    Json     *blend, *inc;
    JsonNode *item;
    char     *dir, *err, *file;
    char     *toBlend;

    /*
        Extract the blend[] array from the input JSON as we can't iterate while mutating the JSON
     */
    if ((toBlend = jsonToString(json, 0, "blend", 0)) == NULL) {
        return 0;
    }
    blend = jsonParseKeep(toBlend, 0);
    if (blend == NULL) {
        rFree(toBlend);
        return error("Cannot parse blended properties");
    }
    for (ITERATE_JSON_KEY(blend, 0, NULL, item, nid)) {
        if (path && *path) {
            dir = (char*) rDirname(sclone(path));
            if (dir && *dir) {
                file = sjoin(dir, "/", item->value, NULL);
            } else {
                file = sclone(item->value);
            }
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
    jsonFree(blend);
    return 0;
}

static int mergeConditionals(Json *json, cchar *property)
{
    Json     *conditional;
    JsonNode *collection;
    cchar    *value;
    char     *text;
    int      id, rootId;

    rootId = jsonGetId(json, 0, property);
    if (rootId < 0) {
        return 0;
    }
    /*
        Extract the conditional set as we can't iterate while mutating the JSON
     */
    if ((text = jsonToString(json, rootId, "conditional", 0)) == NULL) {
        return 0;
    }
    conditional = jsonParseKeep(text, 0);
    if (conditional == NULL) {
        rFree(text);
        return error("Cannot parse conditional properties");
    }

    for (ITERATE_JSON(conditional, NULL, collection, nid)) {
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
            id = jsonGetId(conditional, jsonGetNodeId(conditional, collection), value);
            if (id >= 0) {
                if (jsonBlend(json, 0, property, conditional, id, 0, JSON_COMBINE) < 0) {
                    return error("Cannot blend %s", collection->name);
                }
            }
        }
    }
    jsonRemove(json, rootId, "conditional");
    jsonFree(conditional);
    return 0;
}

static char *readInput()
{
    char  *buf, *newBuf;
    ssize bytes, pos;

    if (path) {
        if (!rFileExists(path)) {
            if (noerror) {
                return sclone("{}");
            }
            error("Cannot locate file %s", path);
            return 0;
        }
        if ((buf = rReadFile(path, NULL)) == 0) {
            error("Cannot read input from %s", path);
            return 0;
        }
    } else {
        buf = rAlloc(ME_BUFSIZE + 1);
        pos = 0;
        do {
            if (pos >= SSIZE_MAX - (ME_BUFSIZE + 1)) {
                rFree(buf);
                error("Input too large");
                return NULL;
            }
            if ((newBuf = rRealloc(buf, pos + ME_BUFSIZE + 1)) == NULL) {
                rFree(buf);
                error("Cannot reallocate memory for input");
                return NULL;
            }
            buf = newBuf;
            bytes = fread(&buf[pos], 1, ME_BUFSIZE, stdin);
            if (ferror(stdin)) {
                rFree(buf);
                error("Cannot read from stdin");
                return NULL;
            }
            if (bytes > 0) {
                if (pos > SSIZE_MAX - bytes) {
                    rFree(buf);
                    error("Input too large, size overflow");
                    return NULL;
                }
                pos += bytes;
            }
        } while (bytes > 0);
        buf[pos] = '\0';
    }
    return buf;
}

static void outputAll(Json *json, int flags)
{
    JsonNode *child, *node;
    char     *name, *output, *property;

    if (format == JSON_FORMAT_JSON) {
        output = jsonToString(json, 0, 0, flags);
        rPrintf("%s", output);
        rFree(output);

    } else if (format == JSON_FORMAT_JS) {
        rPrintf("export default %s\n", jsonString(json, flags));

    } else if (format == JSON_FORMAT_JSON5) {
        rPrintf("%s\n", jsonString(json, flags));

    } else if (format == JSON_FORMAT_ENV || format == JSON_FORMAT_HEADER) {
        name = "";
        for (ITERATE_JSON(json, NULL, node, id)) {
            if (node->type == JSON_ARRAY || node->type == JSON_OBJECT) {
                for (ITERATE_JSON(json, node, child, id)) {
                    property = sjoin(name, ".", child->name, NULL);
                    outputNode(json, child, property, flags);
                    rFree(property);
                }
                break;
            } else {
                outputNode(json, node, node->name, flags);
            }
        }
    }
}

static void outputNode(Json *json, JsonNode *node, char *name, int flags)
{
    JsonNode *child;
    cchar    *value;
    char     *output, *property;
    int      id, type;

    if (node) {
        value = node->value;
        type = node->type;
        if (node->type == JSON_ARRAY || node->type == JSON_OBJECT) {
            if (keys || format == JSON_FORMAT_ENV || format == JSON_FORMAT_HEADER) {
                for (ITERATE_JSON(json, node, child, id)) {
                    if (keys) {
                        if (node->type == JSON_ARRAY) {
                            rPrintf("%s", child->value);
                        } else {
                            rPrintf("%s", child->name);
                        }
                        rPrintf("\n");
                    } else {
                        property = sjoin(name, ".", child->name, NULL);
                        outputNode(json, child, property, flags);
                        rFree(property);
                    }
                }
            } else {
                id = jsonGetNodeId(json, node);
                output = jsonToString(json, id, 0, flags);
                rPrintf("%s", output);
                rFree(output);
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
    outputProperty(json, name, value, type);
}

static void outputProperty(Json *json, cchar *name, cchar *value, int type)
{
    char *exp, *property;

    property = makeName(name);
    if (format == JSON_FORMAT_ENV) {
        exp = export ? "export " : "";
        if (type & JSON_STRING) {
            rPrintf("%s%s='%s'", exp, property, value);
        } else {
            rPrintf("%s%s=%s", exp, property, value);
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
    } else if (format == JSON_FORMAT_JS) {
        rPrintf("export default %s", value);

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
    char  *dp;
    cchar *sp;

    for (dp = dest, sp = src; *sp; sp++, dp++) {
        if (isupper(*sp) && sp != src) {
            *dp++ = '_';
        }
        if (*sp == '.') {
            *dp = '_';
        } else {
            *dp = toupper(*sp);
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
        if (json && json->error) {
            rError("json", "%s. %s", msg, json->error);
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
