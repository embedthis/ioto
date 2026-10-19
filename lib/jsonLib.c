/*
    jsonLib.c -- JSON Library Source

    This file is a catenation of all the source code. Amalgamating into a
    single file makes embedding simpler and the resulting application faster,
    by using compiler optimization within the JSON library.

    Prepared by: buildLib.sh
 */

#include "json.h"

#if ME_COM_JSON


/********* Start of file src/jsonLib.c ************/

/*
    jsonLib.c - JSON parser and query engine implementation

    This module provides the core implementation of JSON parsing, querying, and manipulation.
    It supports loading and saving JSON to files with a high-performance query API for
    in-memory JSON node trees.

    Architecture:
    - JSON text is parsed into a flat array of JsonNode structures (not a pointer-based tree)
    - Provides dot-notation query API for searching and updating the tree (e.g., "user.name")
    - Trees can be serialized back to JSON/JSON5/JSON6 text with various formatting options

    JSON5/JSON6 Support:
    - Unquoted object keys when they don't contain special characters
    - Trailing commas allowed in objects and arrays
    - Single-line (//) and multi-line comments (not preserved during serialization)
    - Single quotes, double quotes, and backticks for strings
    - JavaScript primitives: undefined, null, true, false

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "json.h"

#if ME_COM_JSON
/*********************************** Locals ***********************************/

#ifndef ME_JSON_INC
    #define ME_JSON_INC              64        /**< Node array growth increment when expanding */
#endif

#ifndef ME_JSON_MAX_RECURSION
    #define ME_JSON_MAX_RECURSION    32        /**< Maximum recursion depth for blending and serialization */
#endif

#ifndef ME_JSON_DEFAULT_PROPERTY
    #define ME_JSON_DEFAULT_PROPERTY 64        /**< Default property name buffer size in bytes */
#endif

#ifndef ME_JSON_BUFSIZE
/*
    Most conversions are done for short properties and values, so we use a small buffer size.
 */
    #define ME_JSON_BUFSIZE 128                /**< Default bufsize for jsonToString operation */
#endif


static int maxLength = JSON_MAX_LINE_LENGTH;   // Maximum line length for compact output
static int indentLevel = JSON_DEFAULT_INDENT;  // Indentation spaces per level

/********************************** Forwards **********************************/

static JsonNode *allocNode(Json *json, int type, cchar *name, cchar *value);
static int blendRecurse(Json *dest, int did, cchar *dkey, const Json *csrc, int sid, cchar *skey, int flags, int depth);
static void compactProperties(RBuf *buf, char *sol, int indent);
static char *copyProperty(Json *json, cchar *key);
static int expandValue(const Json *json, RBuf *buf, cchar *key, int indent, int depth, int flags);
static void freeNode(JsonNode *node);
static bool isfnumber(cchar *s, size_t len);
static int jerror(Json *json, cchar *fmt, ...);
static int jquery(Json *json, int nid, cchar *key, cchar *value, int type);
static int jquerySetTerm(Json *json, int nid, cchar *property, cchar *rest, int qtype, cchar *value, int type);
static cchar *nodeValueAsString(JsonNode *node, cchar *defaultValue);
static int nodeToString(const Json *json, int nid, int indent, int depth, int flags, RBuf *buf);
static int putValueToBuf(const Json *json, RBuf *buf, cchar *value, int flags, int indent, int depth);
static char *parseValue(Json *json, int parent, int type, char *name, char *value, int flags);
static int replaceFile(cchar *tmp, cchar *path);
static int sleuthValueType(cchar *value, size_t len, int flags);
static void spaces(RBuf *buf, int count);

/************************************* Code ***********************************/

PUBLIC Json *jsonAlloc(void)
{
    Json *json;

    json = rAllocType(Json);
    json->lineNumber = 1;
    json->lock = 0;
    json->size = ME_JSON_INC;
    json->nodes = rAlloc(sizeof(JsonNode) * (size_t) json->size);
    return json;
}

PUBLIC void jsonFree(Json *json)
{
    JsonNode *node;

    if (!json) {
        return;
    }
    if (!json->nodes) {
        return;
    }
    for (node = json->nodes; node < &json->nodes[json->count]; node++) {
        freeNode(node);
    }
    rFree(json->text);
    rFree(json->value);
    rFree(json->property);

#if R_USE_EVENT
    if (json->event) {
        rStopEvent(json->event);
    }
#endif
    rFree(json->error);
    rFree(json->path);
    rFree(json->nodes);

    // Help detect double free
    json->nodes = 0;
    rFree(json);
}

static void freeNode(JsonNode *node)
{
    if (!node) {
        return;
    }
    if (node->allocatedName) {
        rFree(node->name);
        node->name = 0;
        node->allocatedName = 0;
    }
    if (node->allocatedValue) {
        rFree(node->value);
        node->value = 0;
        node->allocatedValue = 0;
    }
}

static bool growNodes(Json *json, int num)
{
    void *p;
    int  size;

    if (!json || num <= 0) {
        return 0;
    }
    if (num > ME_JSON_MAX_NODES) {
        jerror(json, "Too many elements in json text");
        return 0;
    }
    if ((json->count + num) > json->size) {
        size = json->size + max(num, ME_JSON_INC);
        if (size > ME_JSON_MAX_NODES) {
            jerror(json, "Too many elements in json text");
            return 0;
        }
        p = rRealloc(json->nodes, sizeof(JsonNode) * (size_t) size);
        if (p == 0) {
            jerror(json, "Cannot allocate memory");
            return 0;
        }
        json->nodes = p;
        json->size = size;
    }
    return 1;
}

static void initNode(Json *json, int nid)
{
    JsonNode *node;

    if (!json || nid < 0 || nid >= json->count) {
        return;
    }
    node = &json->nodes[nid];
    node->name = 0;
    node->value = 0;
    node->allocatedValue = 0;
    node->allocatedName = 0;
    node->last = nid + 1;
#if ME_DEBUG
    node->lineNumber = json->lineNumber;
#endif
}

/*
    Set the name and value of a node.
 */
static void setNode(Json *json, int nid, int type, cchar *name, int allocatedName,
                    cchar *value, int allocatedValue)
{
    JsonNode *node;
    char     *copy;

    if (!json || nid < 0 || nid >= json->count) {
        return;
    }
    node = &json->nodes[nid];
    node->type = (uint) type;

    /*
        Clone the new name and value before freeing the old, as the caller may pass a pointer into the old one.
     */
    if (name != node->name && !smatch(name, node->name)) {
        copy = (name && allocatedName) ? sclone(name) : (char*) name;
        if (node->allocatedName) {
            rFree(node->name);
        }
        node->name = copy;
        node->allocatedName = copy ? (uint) allocatedName : 0;
    }

    if (value != node->value && !smatch(value, node->value)) {
#if JSON_TRIGGER
        if (json->trigger) {
            (json->trigger)(json->triggerArg, json, node, node->name, value, node->value);
        }
#endif
        copy = (value && allocatedValue) ? sclone(value) : (char*) value;
        if (node->allocatedValue) {
            rFree(node->value);
        }
        node->value = copy;
        node->allocatedValue = copy ? (uint) allocatedValue : 0;
    }
}

static JsonNode *allocNode(Json *json, int type, cchar *name, cchar *value)
{
    int nid;

    if (!json || (json->count >= json->size && !growNodes(json, 1))) {
        return 0;
    }
    nid = json->count++;
    initNode(json, nid);
    setNode(json, nid, type, name, 0, value, 0);
    return &json->nodes[nid];
}

/*
    Copy nodes from source to destination for jsonBlend operation.
    Creates deep copies of node names and values with proper memory allocation tracking.
    Updates the 'last' index to maintain tree structure in the destination.
 */
static void copyNodes(Json *dest, int did, Json *src, int sid, int slen)
{
    JsonNode *dp, *sp;
    int      i;

    if (!dest || !src || did < 0 || did >= dest->count || sid < 0 || sid >= src->count ||
        slen < 0 || slen > dest->count - did || slen > src->count - sid) {
        return;
    }
    dp = &dest->nodes[did];
    sp = &src->nodes[sid];

    for (i = 0; i < slen; i++, dp++, sp++) {
        if (dp->allocatedName) {
            rFree(dp->name);
        }
        if (dp->allocatedValue) {
            rFree(dp->value);
        }
        *dp = *sp;
        if ((dp->name = sclone(sp->name)) != 0) {
            dp->allocatedName = 1;
        }
        if ((dp->value = sclone(sp->value)) != 0) {
            dp->allocatedValue = 1;
        }
        dp->last = did + sp->last - sid;
    }
}

/*
    Insert room for 'num' nodes at json->nodes[nid].
    This creates space by shifting existing nodes and updating all 'last' indices.
    Should be called at the end of an array or object to maintain tree structure.
    Returns the node ID where insertion occurred, or negative error code on failure.
 */
static int insertNodes(Json *json, int nid, int num, int parentId)
{
    JsonNode *node;
    int      i;

    if (!json || nid < 0 || nid > json->count || num <= 0) {
        return R_ERR_BAD_ARGS;
    }
    if ((json->count + num) >= json->size && !growNodes(json, num)) {
        return R_ERR_MEMORY;
    }
    node = &json->nodes[nid];
    if (nid < json->count) {
        memmove(node + num, node, (size_t) (json->count - nid) * sizeof(JsonNode));
    }
    json->count += num;

    for (i = 0; i < json->count; i++) {
        if (nid <= i && i < (nid + num)) {
            continue;
        }
        node = &json->nodes[i];
        if (node->last == nid && i > parentId) {
            // Current oldest sibling
            continue;
        }
        if (node->last >= nid) {
            node->last += num;
        }
    }
    for (i = 0; i < (int) num; i++) {
        initNode(json, nid + i);
    }
    return nid;
}

static int removeNodes(Json *json, int nid, int num)
{
    JsonNode *node;
    int      i;

    if (!json || nid < 0 || nid >= json->count || num <= 0) {
        return R_ERR_BAD_ARGS;
    }
    node = &json->nodes[nid];
    for (i = 0; i < num; i++) {
        freeNode(&json->nodes[nid + i]);
    }
    json->count -= num;

    if (nid < json->count) {
        memmove(node, node + num, (size_t) (json->count - nid) * sizeof(JsonNode));
    }
    for (i = 0; i < json->count; i++) {
        node = &json->nodes[i];
        if (node->last > nid) {
            node->last -= num;
        }
    }
    return nid;
}

PUBLIC void jsonLock(Json *json)
{
    json->lock = 1;
}

PUBLIC void jsonUnlock(Json *json)
{
    json->lock = 0;
}

PUBLIC void jsonSetUserFlags(Json *json, int flags)
{
    json->userFlags = (uint) flags;
}

PUBLIC int jsonGetUserFlags(Json *json)
{
    return (int) json->userFlags;
}

/*
    Parse json text and return a JSON tree.
    Tolerant of null or empty text.
 */
PUBLIC Json *jsonParse(cchar *ctext, int flags)
{
    Json *json;
    char *text;

    json = jsonAlloc();
    text = sclone(ctext);
    if (jsonParseText(json, text, flags) < 0) {
        jsonFree(json);
        return 0;
    }
    return json;
}

/**
    Parse a json string into a json object and assume ownership of the supplied text memory.
    The caller must not free the text which will be freed by this function.
    Use this method if you are sure the supplied JSON text is valid or do not
    need to receive diagnostics of parse failures other than the return value.
 */
PUBLIC Json *jsonParseKeep(char *text, int flags)
{
    Json *json;

    json = jsonAlloc();
    if (jsonParseText(json, text, flags) < 0) {
        jsonFree(json);
        return 0;
    }
    return json;
}

PUBLIC Json *jsonParseFmt(cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    Json    *json;

    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    json = jsonParseKeep(buf, 0);
    va_end(ap);
    return json;
}

/*
    Convert a string into strict json. Caller must free.
 */
PUBLIC char *jsonConvert(cchar *fmt, ...)
{
    va_list ap;
    Json    *json;
    char    *buf, *msg;

    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    if (!buf || *buf == '\0') {
        rFree(buf);
        return 0;
    }
    json = jsonParseKeep(buf, 0);
    va_end(ap);
    if (!json) {
        return 0;
    }
    msg = jsonToString(json, 0, 0, JSON_JSON);
    jsonFree(json);
    return msg;
}

/*
    Convert a string into a strict json string.
 */
PUBLIC cchar *jsonConvertBuf(char *buf, size_t size, cchar *fmt, ...)
{
    va_list ap;
    Json    *json;
    char    *msg;

    va_start(ap, fmt);
    sfmtbufv(buf, size, fmt, ap);
    va_end(ap);

    json = jsonParse(buf, 0);
    msg = jsonToString(json, 0, 0, JSON_JSON);
    sncopy(buf, size, msg, slen(msg));
    rFree(msg);
    jsonFree(json);
    return buf;
}

/*
    Parse JSON text and return a JSON tree and error message if parsing fails.
 */
PUBLIC Json *jsonParseString(cchar *atext, char **error, int flags)
{
    Json *json;
    char *text;

    if (error) {
        *error = 0;
    }
    json = jsonAlloc();
    text = sclone(atext);

    if (jsonParseText(json, text, flags) < 0) {
        if (error) {
            *error = (char*) json->error;
            json->error = 0;
        }
        jsonFree(json);
        return 0;
    }
    return json;

}

/*
    Parse JSON text from a file
 */
PUBLIC Json *jsonParseFile(cchar *path, char **error, int flags)
{
    Json *json;
    char *text;

    if (!path || *path == '\0') {
        return NULL;
    }
    if (error) {
        *error = 0;
    }
    if ((text = rReadFile(path, 0)) == 0) {
        if (error) {
            *error = sfmt("Cannot open: \"%s\"", path);
        }
        return 0;
    }
    json = jsonAlloc();
    if (path) {
        json->path = sclone(path);
    }
    if (jsonParseText(json, text, flags) < 0) {
        if (error) {
            *error = json->error;
            json->error = 0;
        }
        jsonFree(json);
        return 0;
    }
    return json;
}

/*
    Save the JSON tree to a file.
    The tree rooted at the node specified by "nid/key" is saved.
 */
PUBLIC int jsonSave(Json *json, int nid, cchar *key, cchar *path, int mode, int flags)
{
    char   *text, *tmp;
    int    fd;
    size_t len;

    if (!json || !path || *path == '\0') {
        return R_ERR_BAD_ARGS;
    }
    if ((text = jsonToString(json, nid, key, flags)) == 0) {
        return R_ERR_BAD_STATE;
    }
    if (mode == 0) {
        mode = 0644;
    }
    tmp = sjoin(path, ".tmp", NULL);
    if ((fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, mode)) < 0) {
        rFree(text);
        rFree(tmp);
        return R_ERR_CANT_OPEN;
    }
    len = slen(text);
    if ((ssize) write(fd, text, (uint) len) != (ssize) len) {
        close(fd);
        unlink(tmp);
        rFree(text);
        rFree(tmp);
        return R_ERR_CANT_WRITE;
    }
    rFree(text);
    if (close(fd) < 0 || replaceFile(tmp, path) < 0) {
        unlink(tmp);
        rFree(tmp);
        return R_ERR_CANT_WRITE;
    }
    rFree(tmp);
    return 0;
}

/*
    Atomically replace path with tmp so path is never absent
 */
static int replaceFile(cchar *tmp, cchar *path)
{
#if ME_WIN_LIKE
    return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(tmp, path);
#endif
}

/*
    Parse primitive values including key names and unquoted strings
    Return with json->next pointing to the character after the primitive value.
 */
static char *parsePrimitive(Json *json)
{
    char *next, *start;

    if (!json || !json->next || !json->end) {
        jerror(json, "Bad args");
        return NULL;
    }
    for (start = next = json->next; next < json->end; next++) {
        switch (*next) {
        case '\n':
            json->lineNumber++;
        // Fall through
        case ' ':
        case '\t':
        case '\r':
            // Can terminate here as we have a space/newline/tab/carriage return
            *next++ = 0;
            json->next = next;
            return start;

        //  Balance nest '{' ']'
        case '}':
        case ']':
        case ':':
        case ',':
            // Cant terminate here as we need the brace/colon/comma to parse next token
            json->next = next;
            return start;

        default:
            if (*next == '+') {
                /*
                    A plus belongs to a primitive only as an exponent sign, so that
                    "1e+5" tokenizes as one number. A leading plus does not, and stays
                    a parse error as before.
                 */
                if (next > start && (next[-1] == 'e' || next[-1] == 'E')) {
                    break;
                }
                json->next = next;
                return start;
            }
            if (*next != '_' && *next != '-' && *next != '.' && !isalnum((uchar) * next)) {
                json->next = next;
                return start;
            }
            if (*next < 32 || *next >= 127) {
                jerror(json, "Illegal character in primitive");
                return NULL;
            }
            if ((*next == '.' || *next == '[') && (next == start || !isalnum((uchar) next[-1]))) {
                jerror(json, "Illegal dereference in primitive");
                return NULL;
            }
        }
    }
    // Cant terminate here as we need the brace/colon/comma to parse next token
    json->next = next;
    return start;
}

static char *parseRegExp(Json *json)
{
    char *end, *next, *start, c;

    if (!json || !json->next || !json->end) {
        jerror(json, "Bad args");
        return NULL;
    }
    start = json->next;
    end = json->end;

    for (next = start; next < end && *next; next++) {
        c = *next;
        if (c == '/' && (next == start || next[-1] != '\\')) {
            *next++ = '\0';
            json->next = next;
            return start;
        }
    }
    // Ran out of input
    json->next = start;
    jerror(json, "Incomplete regular expression");
    return NULL;
}

/*
    Decode four hex digits at *nextp and advance past them. Return the value, or -1 on error.
 */
static int parseHex4(Json *json, char **nextp)
{
    char *next;
    int  d, j, value;

    value = 0;
    for (j = 0, next = *nextp; j < 4; j++, next++) {
        if (next >= json->end || !*next) {
            jerror(json, "Invalid unicode characters");
            return -1;
        }
        d = tolower((uchar) * next);
        if (isdigit((uchar) d)) {
            value = (value * 16) + d - '0';
        } else if (d >= 'a' && d <= 'f') {
            value = (value * 16) + d - 'a' + 10;
        } else {
            jerror(json, "Unexpected hex characters");
            return -1;
        }
    }
    *nextp = next;
    return value;
}

/*
    Decode the \u escape whose 'u' is at *nextp into UTF-8 at op. A surrogate pair is one code
    point, and a lone surrogate is an error as it has no UTF-8 form. The escape is at least six
    characters and its UTF-8 at most four bytes, so it always fits in place. On return *nextp is
    on the last hex digit consumed. Return the byte count written, or -1 on error.
 */
static int parseUnicode(Json *json, char **nextp, char *op)
{
    char *next;
    int  cp, low;

    next = *nextp + 1;
    if ((cp = parseHex4(json, &next)) < 0) {
        return -1;
    }
    if (cp >= 0xD800 && cp <= 0xDBFF) {
        if (&next[1] >= json->end || next[0] != '\\' || next[1] != 'u') {
            jerror(json, "Invalid unicode surrogate");
            return -1;
        }
        next += 2;
        if ((low = parseHex4(json, &next)) < 0) {
            return -1;
        }
        if (low < 0xDC00 || low > 0xDFFF) {
            jerror(json, "Invalid unicode surrogate");
            return -1;
        }
        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
        jerror(json, "Invalid unicode surrogate");
        return -1;
    }
    *nextp = next - 1;
    if (cp < 0x80) {
        op[0] = (char) cp;
        return 1;
    } else if (cp < 0x800) {
        op[0] = (char) (0xC0 | (cp >> 6));
        op[1] = (char) (0x80 | (cp & 0x3F));
        return 2;
    } else if (cp < 0x10000) {
        op[0] = (char) (0xE0 | (cp >> 12));
        op[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
        op[2] = (char) (0x80 | (cp & 0x3F));
        return 3;
    }
    op[0] = (char) (0xF0 | (cp >> 18));
    op[1] = (char) (0x80 | ((cp >> 12) & 0x3F));
    op[2] = (char) (0x80 | ((cp >> 6) & 0x3F));
    op[3] = (char) (0x80 | (cp & 0x3F));
    return 4;
}

/*
    Parse a string and advance json->next to the end of the string.
    Quotes are removed. Return with json->next pointing to the character after the closing quote.
 */
static char *parseString(Json *json)
{
    char *end, *next, *op, *start, c, quote;
    int  d;

    if (!json || !json->next || !json->end) {
        jerror(json, "Bad args");
        return NULL;
    }
    next = json->next;
    end = json->end;
    quote = *next++;

    for (op = start = next; next < end && *next; op++, next++) {
        c = *next;
        if (c == '\\' && next < end) {
            c = *++next;
            switch (c) {
            case '\'':
            case '`':
            case '\"':
            case '/':
            case '\\':
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'r':
                c = '\r';
                break;
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
                break;
            case 'u':
                /*
                    parseUnicode writes the UTF-8 bytes at op and leaves next on the last hex
                    digit. Step op to the last byte written so the store below and the loop
                    increment carry on from there.
                 */
                if ((d = parseUnicode(json, &next, op)) < 0) {
                    return NULL;
                }
                op += d - 1;
                c = *op;
                break;

            default:
                json->next = start;
                jerror(json, "Unexpected characters in string");
                return NULL;
            }
            *op = c;

        } else if (c == quote) {
            *op = '\0';
            json->next = next + 1;
            return start;

        } else if (op != next) {
            *op = c;
        }
    }
    // Ran out of input
    json->next = start;
    jerror(json, "Incomplete string");
    return NULL;
}

static int parseComment(Json *json)
{
    char *next;
    int  startLine;

    if (!json || !json->next) {
        return R_ERR_BAD_ARGS;
    }
    next = json->next;
    startLine = json->lineNumber;

    if (*next == '/') {
        for (next++; next < json->end && *next && *next != '\n'; next++) {
        }

    } else if (*next == '*') {
        for (next++; next < &json->end[-1] && (*next != '*' || next[1] != '/'); next++) {
            if (*next == '\n') {
                json->lineNumber++;
            }
        }
        if (*next == '*' && next[1] == '/') {
            next += 2;
        } else {
            return jerror(json, "Cannot find end of comment started on line %d", startLine);
        }
    }
    json->next = next;
    return 0;
}

/*
    Parse the text and assume ownership of the text.
    This is a fast, linear, insitu-parser. It does not use recursion or a parser stack.
    Because we parse in-situ, the text is modified as we parse it. This also means that
    terminating values witih '\0' is somewhat delayed until we parse the next token --
    otherwise we would erase the next token prematurely. This happens when parsing key
    names followed by a colon or a comma.
 */
PUBLIC int jsonParseText(Json *json, char *text, int flags)
{
    JsonNode *node;
    char     *name;
    int      level, parent, type;
    uchar    c;

    if (!json || !text) {
        return R_ERR_BAD_ARGS;
    }
    json->next = json->text = text;
    json->end = &json->text[slen(text)];

    name = 0;
    parent = -1;
    level = 0;
    flags &= ~JSON_EXPECT_KEY;
    type = 0;

    while (json->next < json->end && !json->error) {
        c = (uchar) * json->next;
        switch (c) {
        case '{':
        case '[':
            if (flags & JSON_EXPECT_COMMA) {
                return jerror(json, "Invalid brace/bracket");
            }
            /*
                A container in an object position needs a key, exactly as a scalar does.
                Without this, "{{}}" parsed into an object holding a nameless child, which
                serialized as the invalid "{null:{}}" and crashed jsonBlend.
             */
            if (!name && parent >= 0 && json->nodes[parent].type == JSON_OBJECT) {
                return jerror(json, "Missing property name");
            }
            flags &= ~JSON_PARSE_FLAGS;
            type = (c == '{' ? JSON_OBJECT : JSON_ARRAY);
            flags |= type == JSON_OBJECT ? JSON_EXPECT_KEY: JSON_EXPECT_VALUE;
            *json->next++ = 0;
            level++;
            if ((node = allocNode(json, type, name, 0)) == 0) {
                return R_ERR_MEMORY;
            }
            // Until the array/object is closed, 'last' holds the parent index
            node->last = parent;
            parent = json->count - 1;
            name = 0;
            break;

        case '}':
        case ']':
            if (--level < 0) {
                return jerror(json, "Unmatched brace/bracket");
            }
            if ((c == '}' ? JSON_OBJECT : JSON_ARRAY) != type) {
                return jerror(json, "Mismatched brace/bracket");
            }
            if (name) {
                return jerror(json, "Missing property value");
            }
            /*
                An element is only missing if the container has one. An empty object or
                array still carries the expectation set when it was opened, and {} and []
                are both legal.
             */
            if (flags & JSON_STRICT_PARSE && flags & (JSON_EXPECT_VALUE | JSON_EXPECT_KEY) &&
                json->count > parent + 1) {
                return jerror(json, "Missing value");
            }
            flags &= ~JSON_PARSE_FLAGS;
            int prior = json->nodes[parent].last;
            if (prior >= 0) {
                flags = JSON_EXPECT_COMMA;
            }
            *json->next++ = 0;
            json->nodes[parent].last = json->count;
            parent = prior;
            if (parent >= 0) {
                type = json->nodes[parent].type;
            }
            break;

        case '\n':
            json->lineNumber++;
            json->next++;
            break;

        case '\t':
        case '\r':
        case ' ':
            json->next++;
            break;

        case ',':
            if (type != JSON_OBJECT && type != JSON_ARRAY) {
                return jerror(json, "Comma in non-object or array");
            }
            if (name) {
                return jerror(json, "Missing property value");
            }
            if (flags & JSON_STRICT_PARSE && flags & (JSON_EXPECT_VALUE | JSON_EXPECT_KEY)) {
                return jerror(json, "Invalid comma");
            }
            flags &= ~JSON_EXPECT_COMMA;
            flags |= type == JSON_OBJECT ? JSON_EXPECT_KEY: JSON_EXPECT_VALUE;
            name = 0;
            *json->next++ = 0;
            break;

        case ':':
            if (!name) {
                return jerror(json, "Missing property name");
            }
            *json->next++ = 0;
            flags &= ~JSON_EXPECT_KEY;
            break;

        case '/':
            /*
                Terminate any pending primitive token, exactly as white space does.
                parsePrimitive returns at this '/' without writing a null, and unlike a
                brace, bracket, colon or comma nothing overwrites it later, so without
                this the token's value ran on into the comment text. The '/' itself is
                consumed either way: parseComment and parseRegExp both start after it.
             */
            *json->next++ = 0;
            if (json->next < json->end && (*json->next == '*' || *json->next == '/')) {
                if (flags & JSON_STRICT_PARSE) {
                    return jerror(json, "Comments are not allowed in JSON mode");
                }
                if (parseComment(json) < 0) {
                    return R_ERR_BAD_STATE;
                }
            } else {
                if (flags & JSON_STRICT_PARSE) {
                    return jerror(json, "Regular expressions are not allowed in JSON mode");
                }
                name = parseValue(json, parent, JSON_REGEXP, name, parseRegExp(json), flags);
            }
            break;

        case '\'':
        case '`':
            if (flags & JSON_STRICT_PARSE) {
                return jerror(json, "Single and backtick quotes are not allowed in JSON mode");
            }
        // Fall through
        case '"':
            if (flags & JSON_EXPECT_COMMA) {
                return jerror(json, "Comma expected");
            }
            name = parseValue(json, parent, JSON_STRING, name, parseString(json), flags);
            /*
                A string used as a value satisfies the pending element expectation, exactly
                as a primitive does. A string used as a key does not: the colon clears
                JSON_EXPECT_KEY, so that a missing colon is still caught.
             */
            if (!name) {
                flags |= JSON_EXPECT_COMMA;
                flags &= ~JSON_EXPECT_VALUE;
            }
            break;

        default:
            /*
                Either a key name or a primitive value (including unquoted strings)
             */
            if (flags & JSON_EXPECT_COMMA) {
                return jerror(json, "Comma expected");
            }
            if (flags & JSON_STRICT_PARSE && type == JSON_OBJECT && !name) {
                return jerror(json, "Invalid property name");
            }
            name = parseValue(json, parent, 0, name, parsePrimitive(json), flags);
            if (!name) {
                flags |= JSON_EXPECT_COMMA;
            }
            flags &= ~(JSON_EXPECT_KEY | JSON_EXPECT_VALUE);
            break;
        }
    }
    if (json->next) {
        *json->next = 0;
    }
    if (level != 0) {
        return jerror(json, "Unclosed brace/bracket");
    }
    if (json->error) {
        return R_ERR_BAD_STATE;
    }
    if (flags & JSON_STRICT_PARSE && json->count == 0) {
        return jerror(json, "Empty JSON document");
    }
    return 0;
}

/*
    Parse a value which may be either a key name or a primitive value (including unquoted strings)
 */
static char *parseValue(Json *json, int parent, int type, char *name, char *value, int flags)
{
    size_t len;

    if (value == NULL || json->error) {
        return NULL;
    }
    /*
        Object and expecting a key name. Use the value as the key name. Note: the value is not yet null
        terminated here while we awaiting parsing the next colon, comma or closing brace/bracket.
     */
    if (!name && parent >= 0 && json->nodes[parent].type == JSON_OBJECT) {
        if (!(flags & JSON_EXPECT_KEY)) {
            jerror(json, "Missing property name");
        }
        return value;
    }
    /*
        Determine the type of the value.
     */
    if (type == 0) {
        /*
            The token runs to json->next. A token terminated by white space has had that
            terminator overwritten with a null, so exclude it. A token terminated by a
            brace, bracket, colon or comma keeps its delimiter for the next token and so
            has no null to discount.
         */
        len = (size_t) (json->next - value);
        if (len > 0 && json->next[-1] == '\0') {
            len--;
        }
        type = sleuthValueType(value, len, flags);
        if (flags & JSON_STRICT_PARSE && type != JSON_PRIMITIVE) {
            jerror(json, "Invalid primitive token");
            return NULL;
        }
    }
    /*
        Empty primitive token is not allowed.
     */
    if (type == JSON_PRIMITIVE && *value == 0) {
        jerror(json, "Empty primitive token");
        return NULL;
    }

    if (parent >= 0 && json->nodes[parent].type == JSON_ARRAY) {
        // Value for array element.
        allocNode(json, type, 0, value);
    } else if (name) {
        // Object property value. May not be null terminated here while parsing next token.
        allocNode(json, type, name, value);
        name = 0;
    } else if (json->count == 0) {
        // Top-level value outside an array or object
        allocNode(json, type, 0, value);
    } else {
        jerror(json, "Invalid primitive");
        return NULL;
    }
    return name;
}

static int sleuthValueType(cchar *value, size_t len, int flags)
{
    uchar c;
    int   type;

    if (!value) {
        return JSON_PRIMITIVE;
    }
    /*
        The length must match exactly, otherwise a keyword prefix such as "nul" would match.
     */
    c = (uchar) value[0];
    if ((c == 't' && len == 4 && sncmp(value, "true", len) == 0) ||
        (c == 'f' && len == 5 && sncmp(value, "false", len) == 0) ||
        (c == 'n' && len == 4 && sncmp(value, "null", len) == 0) ||
        ((c == 'u' && len == 9 && sncmp(value, "undefined", len) == 0) && !(flags & JSON_STRICT_PARSE))) {
        type = JSON_PRIMITIVE;

    } else if (isfnumber(value, len)) {
        type = JSON_PRIMITIVE;

    } else {
        type = JSON_STRING;
    }
    return type;
}

PUBLIC int jsonGetType(Json *json, int nid, cchar *key)
{
    if (!json) {
        return R_ERR_BAD_ARGS;
    }
    if (key) {
        nid = jquery(json, nid, key, 0, 0);
    } else {
        nid = 0;
    }
    if (nid < 0 || nid >= json->count) {
        return R_ERR_BAD_ARGS;
    }
    return json->nodes[nid].type;
}

static char *getNextTerm(char *str, char **rest, int *type)
{
    char   *start, *end, *seps;
    size_t i;

    seps = ".[]";
    start = str;
    if (start == NULL) {
        if (rest == NULL) {
            return 0;
        }
        start = *rest;
    }
    if (start == 0) {
        if (rest) {
            *rest = 0;
        }
        return 0;
    }
    while (isspace((int) *start)) start++;
    if ((i = strspn(start, seps)) > 0) {
        start += i;
    }
    if (*start == '\0') {
        if (rest) {
            *rest = 0;
        }
        return 0;
    }
    end = strpbrk(start, seps);
    if (end != 0) {
        if (*end == '[') {
            /*
                A quoted term inside brackets is a property name, not an index, so the
                container it selects from is an object. Without this "a['q']" typed the
                container as an array and the write path built one, losing the name.
             */
            *type = (end[1] == '"' || end[1] == '\'') ? JSON_OBJECT : JSON_ARRAY;
            *end++ = '\0';
        } else if (*end == '.') {
            *type = JSON_OBJECT;
            *end++ = '\0';
        } else {
            //  Strip off matching quotes
            //  end > start + 1 so the opening and closing quotes are distinct
            //  characters. A lone quote before "]" matched itself and left an
            //  empty term, which set and get then resolved differently.
            if ((*start == '"' || *start == '\'') && end > start + 1 && end[-1] == *start) {
                start++;
                end[-1] = '\0';
            }
            *end++ = '\0';
            *type = JSON_OBJECT;
        }
        /*
            A remainder of nothing but separators is not another term. Only the
            bracket-close branch used to normalize this, so a trailing "." or "["
            left a non-null rest and the caller believed another term followed --
            which made jsonSet create an empty container instead of setting a value.
         */
        end += strspn(end, seps);
        if (*end == '\0') {
            end = 0;
        }
    }
    *rest = end;
    return start;
}

static int findProperty(Json *json, int nid, cchar *property)
{
    JsonNode *node, *np;
    int64    index;
    int      id;

    if (!json || nid < 0 || nid >= json->count) {
        return R_ERR_BAD_ARGS;
    }
    if (json->count == 0) {
        return R_ERR_CANT_FIND;
    }
    node = &json->nodes[nid];
    if (!property || *property == 0) {
        return R_ERR_CANT_FIND;
    }
    if (node->type == JSON_ARRAY) {
        if (!isdigit((uchar) * property) || (index = stoi(property)) < 0) {
            for (id = nid + 1; id < node->last; id = np->last) {
                np = &json->nodes[id];
                if (smatch(property, np->value)) {
                    return id;
                }
            }
            return R_ERR_CANT_FIND;

        } else {
            if (index >= (node->last - nid - 1)) {
                return R_ERR_CANT_FIND;
            }
            for (id = nid + 1; index-- > 0 && id < node->last; id = np->last) {
                np = &json->nodes[id];
            }
            if (id <= nid || id >= node->last) {
                return R_ERR_CANT_FIND;
            }
            return id;
        }

    } else if (node->type == JSON_OBJECT) {
        for (id = nid + 1; id < node->last; id = np->last) {
            np = &json->nodes[id];
            if (smatch(property, np->name)) {
                return id;
            }
        }
        return R_ERR_CANT_FIND;
    }
    return R_ERR_BAD_STATE;
}

/*
    Set one property. rest indicates a following path term; qtype selects new container types.
 */
static int jquerySetTerm(Json *json, int nid, cchar *property, cchar *rest, int qtype, cchar *value, int type)
{
    int64 index;
    int   cid, count, id, ntype;

    id = findProperty(json, nid, property);
    /*
        Array lookup by value is read-only: changing the value would invalidate the lookup key.
     */
    if (nid < json->count && json->nodes[nid].type == JSON_ARRAY && !smatch(property, "$") &&
        !isdigit((uchar) * property)) {
        return R_ERR_CANT_CREATE;
    }
    if (id < 0) {
        // Property not found
        if (nid >= json->count) {
            allocNode(json, JSON_OBJECT, 0, 0);
        }
        /*
            findProperty cannot resolve empty names.
         */
        if (*property == '\0') {
            return R_ERR_CANT_FIND;
        }
        /*
            Convert scalars to objects so findProperty can reach the new child.
         */
        if (!(json->nodes[nid].type & (JSON_OBJECT | JSON_ARRAY))) {
            setNode(json, nid, JSON_OBJECT, json->nodes[nid].name, 1, 0, 0);
        }
        /*
            Fill missing array entries with nulls to preserve the requested index.
         */
        if (json->nodes[nid].type == JSON_ARRAY && isdigit((uchar) * property)) {
            /*
                Reject indexes beyond the node limit.
             */
            index = stoi(property);
            if (index < 0 || index >= ME_JSON_MAX_NODES) {
                return R_ERR_CANT_CREATE;
            }
            for (count = 0, cid = nid + 1; cid < json->nodes[nid].last; cid = json->nodes[cid].last) {
                count++;
            }
            /*
                Insert the gap in one operation to avoid quadratic work and partial updates.
             */
            if (count < index) {
                if ((cid = insertNodes(json, json->nodes[nid].last, (int) (index - count), nid)) < 0) {
                    return R_ERR_CANT_CREATE;
                }
                // Use a shared literal to avoid allocating each null separately.
                while (count < index) {
                    setNode(json, cid++, JSON_PRIMITIVE, 0, 0, "null", 0);
                    count++;
                }
            }
        }
        if ((cid = insertNodes(json, json->nodes[nid].last, 1, nid)) < 0) {
            return R_ERR_CANT_CREATE;
        }
        if (rest) {
            // Not yet at the leaf node so make intervening array/object
            setNode(json, cid, qtype, property, 1, 0, 0);

        } else if (json->nodes[nid].type == JSON_ARRAY && smatch(property, "$")) {
            if (type == JSON_OBJECT || type == JSON_ARRAY) {
                setNode(json, cid, type, property, 1, 0, 0);
            } else {
                setNode(json, cid, type, property, 1, value, 1);
            }

        } else {
            setNode(json, cid, type, property, 1, value, 1);
        }
        id = cid;

    } else {
        if (rest) {
            /*
                Convert intervening scalars to containers; dot and bracket syntax accept either container type.
             */
            ntype = json->nodes[id].type;
            if (!(ntype & (JSON_OBJECT | JSON_ARRAY))) {
                setNode(json, id, qtype, property, 1, 0, 0);
            }
        } else {
            /*
                Store the value before removing a replaced container's subtree, as the value may point into it.
             */
            count = (json->nodes[id].type & (JSON_OBJECT | JSON_ARRAY)) ? json->nodes[id].last - id - 1 : 0;
            setNode(json, id, type, property, 1, value, 1);
            if (count > 0) {
                removeNodes(json, id + 1, count);
            }
        }
    }
    return id;
}

/*
    Resolve or create each term in a property path.
 */
static int jquery(Json *json, int nid, cchar *key, cchar *value, int type)
{
    char *property, *rest;
    int  id, qtype, terms;

    if (!json || nid < 0 || nid > json->count) {
        return R_ERR_BAD_ARGS;
    }
    if (key == 0 || *key == '\0') {
        return R_ERR_CANT_FIND;
    }
    property = copyProperty(json, key);

    qtype = 0;
    rest = 0;
    terms = 0;

    for (; (property = getNextTerm(property, &rest, &qtype)) != 0; property = rest, nid = id) {
        terms++;
        if (value) {
            if ((id = jquerySetTerm(json, nid, property, rest, qtype, value, type)) < 0) {
                return id;
            }
        } else {
            /*
                Get or remove
             */
            if ((id = findProperty(json, nid, property)) < 0) {
                return R_ERR_CANT_FIND;
            }
            if (rest == 0) {
                nid = id;
                break;
            }
        }
    }
    if (terms == 0) {
        //  A key of nothing but separators names no property, as an empty key does
        return R_ERR_CANT_FIND;
    }
    return nid;
}

static char *copyProperty(Json *json, cchar *key)
{
    size_t len;
    void   *p;

    len = slen(key) + 1;
    if (len > json->propertyLength) {
        p = rRealloc(json->property, len);
        if (p == NULL) {
            jerror(json, "Cannot allocate memory for property");
            return NULL;
        }
        json->property = p;
        json->propertyLength = len;
    }
    scopy(json->property, json->propertyLength, key);
    return json->property;
}

/*
    Get the JSON tree node for a given key that is rooted at the "nid" node.
 */
PUBLIC JsonNode *jsonGetNode(const Json *json, int nid, cchar *key)
{
    if (!json || nid < 0) {
        return NULL;
    }
    if ((nid = jsonGetId(json, nid, key)) < 0) {
        return 0;
    }
    return &json->nodes[nid];
}

/*
    Get the node ID for a given tree node
 */
PUBLIC int jsonGetNodeId(const Json *json, JsonNode *node)
{
    if (!json || !node || node < json->nodes || node >= &json->nodes[json->count]) {
        return -1;
    }
    return (int) (node - json->nodes);
}

/*
    Get the node ID for a given key that is rooted at the "nid" node.
 */
PUBLIC int jsonGetId(const Json *json, int nid, cchar *key)
{
    if (!json || nid < 0 || nid >= json->count) {
        return R_ERR_CANT_FIND;
    }
    if (key && *key) {
        if ((nid = jquery((Json*) json, nid, key, 0, 0)) < 0) {
            return R_ERR_CANT_FIND;
        }
    }
    return nid;
}

/*
    Get the nth child node below a node specified by "pid"
 */
PUBLIC JsonNode *jsonGetChildNode(Json *json, int pid, int nth)
{
    JsonNode *child;

    if (!json || pid < 0 || pid >= json->count || nth < 0) {
        return NULL;
    }
    for (ITERATE_JSON_ID(json, pid, child, id)) {
        if (nth-- <= 0) {
            return child;
        }
    }
    return 0;
}

/*
    Containers use empty placeholders; null uses defaultValue.
 */
static cchar *nodeValueAsString(JsonNode *node, cchar *defaultValue)
{
    if (node->type & JSON_OBJECT) {
        return "{}";
    } else if (node->type & JSON_ARRAY) {
        return "[]";
    } else if (node->type & JSON_PRIMITIVE && smatch(node->value, "null")) {
        return defaultValue;
    }
    return node->value;
}

/*
    Read a property path relative to nid. The returned reference is invalidated by updates.
 */
PUBLIC cchar *jsonGet(Json *json, int nid, cchar *key, cchar *defaultValue)
{
    if (!json || nid < 0 || nid >= json->count) {
        return defaultValue;
    }
    if (key && *key) {
        if ((nid = jquery(json, nid, key, 0, 0)) < 0) {
            return defaultValue;
        }
    }
    return nodeValueAsString(&json->nodes[nid], defaultValue);
}

/*
    Read a direct child by literal name.
 */
PUBLIC cchar *jsonGetKey(Json *json, int nid, cchar *key, cchar *defaultValue)
{
    int id;

    if (!json || nid < 0 || nid >= json->count) {
        return defaultValue;
    }
    if (key == 0 || *key == '\0') {
        return defaultValue;
    }
    if ((id = findProperty(json, nid, key)) < 0) {
        return defaultValue;
    }
    return nodeValueAsString(&json->nodes[id], defaultValue);
}

//  DEPRECATED
PUBLIC cchar *jsonGetRef(Json *json, int nid, cchar *key, cchar *defaultValue)
{
    return jsonGet(json, nid, key, defaultValue);
}

/*
    This returns a cloned message that the caller must free
 */
PUBLIC char *jsonGetClone(Json *json, int nid, cchar *key, cchar *defaultValue)
{
    cchar *value;

    if ((value = jsonGet(json, nid, key, defaultValue)) == NULL) {
        return NULL;
    }
    return sclone(value);
}

/*
    This routine is tolerant and will accept boolean values, numbers and string types set to 1 or true.
 */
PUBLIC bool jsonGetBool(Json *json, int nid, cchar *key, bool defaultValue)
{
    cchar *value;

    value = jsonGet(json, nid, key, 0);
    if (value) {
        return smatch(value, "1") || smatch(value, "true");
    }
    return defaultValue;
}

PUBLIC Time jsonGetDate(Json *json, int nid, cchar *key, int64 defaultValue)
{
    cchar *value;

    if ((value = jsonGet(json, nid, key, 0)) == 0) {
        return defaultValue;
    }
    if (snumber(value)) {
        return stoi(value);
    }
    return rParseIsoDate(value);
}

PUBLIC int jsonGetInt(Json *json, int nid, cchar *key, int defaultValue)
{
    int64 value;

    value = jsonGetNum(json, nid, key, defaultValue);
    if (value > MAXINT) {
        return MAXINT;
    } else if (value < -MAXINT - 1) {
        return -MAXINT - 1;
    }
    return (int) value;
}

PUBLIC ssize jsonGetLength(Json *json, int nid, cchar *key)
{
    JsonNode *node, *child;
    ssize    length;

    if ((node = jsonGetNode(json, nid, key)) == NULL) {
        return R_ERR_CANT_FIND;
    }
    length = 0;
    for (ITERATE_JSON(json, node, child, id)) {
        length++;
    }
    return length;
}


PUBLIC int64 jsonGetNum(Json *json, int nid, cchar *key, int64 defaultValue)
{
    cchar  *value;
    char   *end;
    int64  result;
    double d;

    if ((value = jsonGet(json, nid, key, 0)) == 0) {
        return defaultValue;
    }
    end = (char*) value;
    result = stoix(value, &end, 10);
    if (*end == '\0') {
        return result;
    }
    /*
        A fraction, an exponent or a hex literal. Convert through the double path, saturating at
        the int64 limits, rather than stopping at the first non-digit as stoi does.
     */
    d = atof(value);
    if (d != d) {
        return 0;
    } else if (d >= (double) MAXINT64) {
        return MAXINT64;
    } else if (d <= -(double) MAXINT64 - 1) {
        return -MAXINT64 - 1;
    }
    return (int64) d;
}

PUBLIC double jsonGetDouble(Json *json, int nid, cchar *key, double defaultValue)
{
    cchar *value;

    if ((value = jsonGet(json, nid, key, 0)) == 0) {
        return defaultValue;
    }
    return atof(value);
}

PUBLIC int64 jsonGetValue(Json *json, int nid, cchar *key, cchar *defaultValue)
{
    cchar *value;

    value = jsonGet(json, nid, key, defaultValue);
    return svalue(value);
}

/*
    Set a property path relative to nid, creating missing nodes.
 */
PUBLIC int jsonSet(Json *json, int nid, cchar *key, cchar *value, int type)
{
    if (!json || nid < 0 || nid > json->count) {
        return R_ERR_BAD_ARGS;
    }
    if (json->lock) {
        return jerror(json, "Cannot set value in a locked JSON object");
    }
    if (type <= 0 && value) {
        type = sleuthValueType(value, slen(value), 0);
    }
    if (!value) {
        value = "undefined";
    }
    return jquery(json, nid, key, value, type);
}

/*
    Set a direct child by literal name, preserving siblings.
 */
PUBLIC int jsonSetKey(Json *json, int nid, cchar *key, cchar *value, int type)
{
    if (!json || nid < 0 || nid > json->count) {
        return R_ERR_BAD_ARGS;
    }
    if (json->lock) {
        return jerror(json, "Cannot set value in a locked JSON object");
    }
    if (key == 0 || *key == '\0') {
        return R_ERR_BAD_ARGS;
    }
    if (type <= 0 && value) {
        type = sleuthValueType(value, slen(value), 0);
    }
    if (!value) {
        value = "undefined";
    }
    return jquerySetTerm(json, nid, key, NULL, JSON_OBJECT, value, type);
}

PUBLIC int jsonSetJsonFmt(Json *json, int nid, cchar *key, cchar *fmt, ...)
{
    va_list ap;
    Json    *jvalue;
    char    *value;

    if (fmt == 0) {
        return R_ERR_BAD_ARGS;
    }
    va_start(ap, fmt);
    value = sfmtv(fmt, ap);
    va_end(ap);

    if ((jvalue = jsonParseString(value, 0, 0)) == 0) {
        rFree(value);
        return R_ERR_BAD_ARGS;
    }
    rFree(value);
    return jsonBlend(json, nid, key, jvalue, 0, 0, JSON_OVERWRITE);
}

PUBLIC int jsonSetBool(Json *json, int nid, cchar *key, bool value)
{
    cchar *data;

    data = value ? "true" : "false";
    return jsonSet(json, nid, key, data, JSON_PRIMITIVE);
}

PUBLIC int jsonSetDouble(Json *json, int nid, cchar *key, double value)
{
    char buf[32];

    rSnprintf(buf, sizeof(buf), "%f", value);
    return jsonSet(json, nid, key, buf, JSON_PRIMITIVE);
}

PUBLIC int jsonSetDate(Json *json, int nid, cchar *key, Time value)
{
    char *date;
    int  rc;

    date = rGetIsoDate(value);
    rc = jsonSet(json, nid, key, date, JSON_STRING);
    rFree(date);
    return rc;
}

PUBLIC int jsonSetFmt(Json *json, int nid, cchar *key, cchar *fmt, ...)
{
    va_list ap;
    char    *value;
    int     result;

    if (fmt == 0) {
        return 0;
    }
    va_start(ap, fmt);
    value = sfmtv(fmt, ap);
    va_end(ap);

    result = jsonSet(json, nid, key, value, sleuthValueType(value, slen(value), 0));
    rFree(value);
    return result;
}

PUBLIC int jsonSetNumber(Json *json, int nid, cchar *key, int64 value)
{
    char buf[32];

    return jsonSet(json, nid, key, sitosbuf(buf, sizeof(buf), value, 10), JSON_PRIMITIVE);
}

PUBLIC int jsonSetString(Json *json, int nid, cchar *key, cchar *value)
{
    return jsonSet(json, nid, key, value, JSON_STRING);
}

/*
    Update a node value
 */
PUBLIC void jsonSetNodeValue(JsonNode *node, cchar *value, int type, int flags)
{
    char *copy;

    //  Take or clone the new value before freeing the old, as it may point into the old one
    copy = (flags & JSON_PASS_VALUE) ? (char*) value : sclone(value);
    if (node->allocatedValue && node->value != copy) {
        rFree(node->value);
    }
    node->value = copy;
    node->allocatedValue = 1;
    node->type = (uint) type;
}

PUBLIC void jsonSetNodeType(JsonNode *node, int type)
{
    node->type = (uint) type;
}

PUBLIC int jsonRemove(Json *json, int nid, cchar *key)
{
    if (!json || nid < 0 || nid > json->count) {
        return R_ERR_BAD_ARGS;
    }
    if (key) {
        if ((nid = jquery(json, nid, key, 0, 0)) <= 0) {
            return R_ERR_CANT_FIND;
        }
    }
    removeNodes(json, nid, json->nodes[nid].last - nid);
    return 0;
}

/*
    Convert a JSON value to a string and add to the given buffer.
 */
static int putValueToBuf(const Json *json, RBuf *buf, cchar *value, int flags, int indent, int depth)
{
    cchar *cp;
    char  *end, *key;
    int   bareFlags, encode, quotes, quoteKeys, rc;

    if (!buf) {
        return 0;
    }
    if (!value) {
        rPutStringToBuf(buf, "null");
        return 0;
    }
    quotes = flags & JSON_DOUBLE_QUOTES ? 2 : 1;
    quoteKeys = (flags & JSON_QUOTE_KEYS) ? 1 : 0;
    if ((flags & JSON_KEY) && value && *value) {
        if (!quoteKeys) {
            for (cp = value; *cp; cp++) {
                if (!isalnum((uchar) * cp) && *cp != '_') {
                    quoteKeys++;
                    break;
                }
            }
        }
    } else {
        quoteKeys = 1;
    }
    encode = (flags & (JSON_ENCODE)) ? 1 : 0;

    if (flags & JSON_BARE) {
        quotes = 0;
        quoteKeys = 0;
    }
    if (quoteKeys) {
        rPutCharToBuf(buf, quotes == 1 ? '\'' : '\"');
    }
    if (value) {
        for (cp = value; *cp; cp++) {
            if (*cp == '\\') {
                rPutCharToBuf(buf, '\\');
                rPutCharToBuf(buf, *cp);
            } else if (*cp == '"' && quotes == 2) {
                rPutCharToBuf(buf, '\\');
                rPutCharToBuf(buf, *cp);
            } else if (*cp == '\'' && quotes == 1) {
                rPutCharToBuf(buf, '\\');
                rPutCharToBuf(buf, *cp);
            } else if (*cp == '\b') {
                rPutStringToBuf(buf, "\\b");
            } else if (*cp == '\f') {
                rPutStringToBuf(buf, "\\f");
            } else if (*cp == '\n') {
                if (encode) {
                    rPutStringToBuf(buf, "\\n");
                } else {
                    rPutCharToBuf(buf, '\n');
                }
            } else if (*cp == '\r') {
                if (encode) {
                    rPutStringToBuf(buf, "\\r");
                } else {
                    rPutCharToBuf(buf, '\r');
                }
            } else if (*cp == '\t') {
                if (encode) {
                    rPutStringToBuf(buf, "\\t");
                } else {
                    rPutCharToBuf(buf, '\t');
                }
            } else if (iscntrl((uchar) * cp)) {
                rPutToBuf(buf, "\\u%04x", *cp);
            } else if (*cp == '$' && cp[1] == '{' && (flags & JSON_EXPAND) && json) {
                if ((end = strchr(cp + 2, '}')) != 0) {
                    key = snclone(cp + 2, (size_t) (end - cp - 2));
                    bareFlags = flags | JSON_BARE;
                    rc = expandValue(json, buf, key, indent, depth + 1, bareFlags);
                    rFree(key);
                    if (rc == R_ERR_WONT_FIT) {
                        return rc;
                    }
                    cp = end;
                }
            } else {
                rPutCharToBuf(buf, *cp);
            }
        }
    }
    if (quoteKeys) {
        rPutCharToBuf(buf, quotes == 1 ? '\'' : '\"');
    }
    return 0;
}

// Public version without indent
PUBLIC void jsonPutValueToBuf(RBuf *buf, cchar *value, int flags)
{
    putValueToBuf(NULL, buf, value, flags, 0, 0);
}

/*
    Expand a ${path.var} reference described by the key.
    Indent is the textual indentation. Depth is the recursive depth.
 */
static int expandValue(const Json *json, RBuf *buf, cchar *key, int indent, int depth, int flags)
{
    int nid;

    if (flags & JSON_EXPANDING) {
        jerror((Json*) json, "Recursive expanding not supported");
        // Keep the original text
        rPutToBuf(buf, "${%s}", key);
        return R_ERR_BAD_ARGS;
    }
    if ((nid = jquery((Json*) json, 0, key, 0, 0)) >= 0) {
        return nodeToString(json, nid, indent, depth, flags | JSON_EXPANDING, buf);
    }
    return R_ERR_CANT_FIND;
}

static int nodeToString(const Json *json, int nid, int indent, int depth, int flags, RBuf *buf)
{
    JsonNode *node;
    char     *sol, *start;
    bool     multiline;
    int      rc;

    if (!json || !buf || nid < 0 || nid > json->count || indent < 0) {
        return R_ERR_BAD_ARGS;
    }
    if (json->count == 0) {
        return nid;
    }
    if (depth > ME_JSON_MAX_RECURSION) {
        jerror((Json*) json, "Serialization recursion limit exceeded");
        return R_ERR_WONT_FIT;
    }
    node = &json->nodes[nid];
    multiline = (flags & JSON_MULTILINE) ? 1 : 0;

    if (flags & JSON_DEBUG) {
        rPutToBuf(buf, "<%d/%d> ", nid, node->last);
    }
    if (node->type & JSON_PRIMITIVE) {
        /*
            "undefined" is a JSON5 primitive with no JSON representation. Emitting it
            verbatim into a JSON document produces text no JSON parser will accept, so
            render it as null when the caller asked for JSON quoting. JSON5 and JS output
            keep it, where it is legal.
         */
        if ((flags & JSON_DOUBLE_QUOTES) && smatch(node->value, "undefined")) {
            rPutStringToBuf(buf, "null");
        } else {
            rPutStringToBuf(buf, node->value);
        }
        nid++;

    } else if (node->type & JSON_REGEXP) {
        /*
            A regular expression is a JSON6 extension with no JSON representation.
            Render the pattern as a quoted string when the caller asked for JSON
            quoting, as "undefined" is rendered as null. JSON5 and JS keep /pattern/.
         */
        if (flags & JSON_DOUBLE_QUOTES) {
            if ((rc = putValueToBuf(json, buf, node->value, flags, indent, depth)) < 0) {
                return rc;
            }
        } else {
            rPutCharToBuf(buf, '/');
            rPutStringToBuf(buf, node->value);
            rPutCharToBuf(buf, '/');
        }
        nid++;

    } else if (node->type == JSON_STRING) {
        if ((rc = putValueToBuf(json, buf, node->value, flags, indent, depth)) < 0) {
            return rc;
        }
        nid++;

    } else if (node->type == JSON_ARRAY) {
        start = rGetBufStart(buf);
        sol = rGetBufEnd(buf);
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, '[');
        }
        if (multiline) rPutCharToBuf(buf, '\n');
        for (++nid; nid < node->last; ) {
            if (json->nodes[nid].type == 0) {
                nid++;
                continue;
            }
            if (multiline) spaces(buf, indent + 1);
            if ((nid = nodeToString(json, nid, indent + 1, depth + 1, flags, buf)) < 0) {
                return nid;
            }
            if (nid < node->last) {
                rPutCharToBuf(buf, ',');
            }
            if (multiline) rPutCharToBuf(buf, '\n');
        }
        if (multiline) spaces(buf, indent);
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, ']');
        }
        if (flags & JSON_COMPACT) {
            // Incase the buffer was reallocated, update the sol pointer
            sol = rGetBufStart(buf) + (sol - start);
            compactProperties(buf, sol, indent);
        }

    } else if (node->type == JSON_OBJECT) {
        start = rGetBufStart(buf);
        sol = rGetBufEnd(buf);
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, '{');
        }
        if (multiline) rPutCharToBuf(buf, '\n');
        for (++nid; nid < node->last; ) {
            if (json->nodes[nid].type == 0) {
                nid++;
                continue;
            }
            if (multiline) spaces(buf, indent + 1);
            /*
                JSON_EXPAND expands ${path.value} references in values only. Keys are
                rendered as written.
             */
            jsonPutValueToBuf(buf, json->nodes[nid].name, flags | JSON_KEY);
            rPutCharToBuf(buf, ':');
            if (multiline) rPutCharToBuf(buf, ' ');
            if ((nid = nodeToString(json, nid, indent + 1, depth + 1, flags, buf)) < 0) {
                return nid;
            }
            if (nid < node->last) {
                rPutCharToBuf(buf, ',');
            }
            if (multiline) rPutCharToBuf(buf, '\n');
        }
        if (multiline) spaces(buf, indent);
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, '}');
        }
        if (flags & JSON_COMPACT && indent > 0) {
            // Incase the buffer was reallocated, update the sol pointer
            sol = rGetBufStart(buf) + (sol - start);
            compactProperties(buf, sol, indent);
        }
    } else {
        rPutStringToBuf(buf, "undefined");
        nid++;
    }
    return nid;
}

/*
    Return the position after the quoted string that opens at cp, or end if it is unterminated.
 */
static char *skipQuoted(char *cp, char *end)
{
    char quote;

    for (quote = *cp++; cp < end; cp++) {
        if (*cp == '\\' && cp + 1 < end) {
            cp++;
        } else if (*cp == quote) {
            return cp + 1;
        }
    }
    return end;
}

/*
    Collapse whitespace outside strings when the line fits maxLength. Append the terminator through rAddNullToBuf.
 */
static void compactProperties(RBuf *buf, char *sol, int indent)
{
    char *cp, *dp, *end, *qp;
    int  spaces;

    end = buf->end;
    for (spaces = 0, cp = sol; cp < end; cp++) {
        if (*cp == '"' || *cp == '\'') {
            cp = skipQuoted(cp, end) - 1;
        } else if (isspace((uchar) * cp) && cp + 1 < end && isspace((uchar) cp[1])) {
            spaces++;
        }
    }
    if (end - sol - spaces + indent * 4 >= maxLength) {
        return;
    }
    for (cp = dp = sol; cp < end; ) {
        if (*cp == '"' || *cp == '\'') {
            for (qp = skipQuoted(cp, end); cp < qp; ) {
                *dp++ = *cp++;
            }
        } else if (isspace((uchar) * cp)) {
            *dp++ = ' ';
            while (++cp < end && isspace((uchar) * cp)) {
            }
        } else {
            *dp++ = *cp++;
        }
    }
    buf->end = dp;
    rAddNullToBuf(buf);
}

PUBLIC int jsonPutToBuf(RBuf *buf, const Json *json, int nid, int flags)
{
    if (!buf) {
        return R_ERR_BAD_ARGS;
    }
    if (!json) {
        return 0;
    }
    return nodeToString(json, nid, 0, 0, flags, buf);
}

/*
    Serialize a JSON object to a string. The caller must free the result.
 */
PUBLIC char *jsonToString(const Json *json, int nid, cchar *key, int flags)
{
    RBuf *buf;

    if (!json) {
        return 0;
    }
    if ((buf = rAllocBuf(ME_JSON_BUFSIZE)) == 0) {
        return 0;
    }
    if (key && *key && (nid = jsonGetId(json, nid, key)) < 0) {
        rFreeBuf(buf);
        return 0;
    }
    if (nodeToString(json, nid, 0, 0, flags, buf) < 0) {
        rFreeBuf(buf);
        return 0;
    }
    if (flags & JSON_MULTILINE) {
        rPutCharToBuf(buf, '\n');
    }
    return rBufToStringAndFree(buf);
}

/*
    Serialize a JSON object to a string. The string is saved in json->value so the caller
    does not need to free the result.
 */
PUBLIC cchar *jsonString(const Json *cjson, int flags)
{
    Json *json;

    if (!cjson) {
        return 0;
    }
    /*
        We except modifying the json->value from the const
        The downside of this exception is exceeded by the benefit of using (const Json*) elsewhere
     */
    json = (Json*) cjson;
    rFree(json->value);
    if (flags == 0) {
        flags = JSON_HUMAN;
    }
    json->value = jsonToString(json, 0, 0, flags);
    return json->value;
}

/*
    Print a JSON tree for debugging. This is in JSON5 compact format.
 */
PUBLIC void jsonPrint(Json *json)
{
    char *str;

    if (!json) return;
    str = jsonToString(json, 0, 0, JSON_HUMAN);
    rPrintf("%s\n", str);
    rFree(str);
}

#if JSON_BLEND
/*
    Blend sub-trees by copying.
    This performs an N-level deep clone of the source JSON nodes to be blended into the destination object.
    By default, this add new object properies and overwrite arrays and string values.
    The Property combination prefixes: '+', '=', '-' and '?' to append, overwrite, replace and
    conditionally overwrite are supported if the JSON_COMBINE flag is present.
    The flags may contain JSON_COMBINE to enable property prefixes: '+', '=', '-', '?' to append, overwrite,
        replace and conditionally overwrite key values if not already present. When adding string properies, values
        will be appended using a space separator. Extra spaces will not be removed on replacement.
    Without JSON_COMBINE or for properies without a prefix, the default is to blend objects by creating new
        properies if not already existing in the destination, and to treat overwrite arrays and strings.
    Use the JSON_OVERWRITE flag to override the default appending of objects and rather overwrite existing
        properies. Use the JSON_APPEND flag to override the default of overwriting arrays and strings and rather
        append to existing properies.

    NOTE: This is recursive when blending properties for each level of property nest.
    It uses 16 words of stack plus stack frame for each recursion. i.e. >64bytes on 32-bit CPU.
 */
PUBLIC int jsonBlend(Json *dest, int did, cchar *dkey, const Json *csrc, int sid, cchar *skey, int flags)
{
    return blendRecurse(dest, did, dkey, csrc, sid, skey, flags, 0);
}

static int blendRecurse(Json *dest, int did, cchar *dkey, const Json *csrc, int sid, cchar *skey, int flags, int depth)
{
    Json     *src, *tmpSrc;
    JsonNode *dp, *sp, *spc, *dpc;
    cchar    *property;
    char     *srcData, *value;
    int      at, slen, dlen, id, didc, kind, pflags, rc;

    if (dest == 0) {
        return R_ERR_BAD_ARGS;
    }
    if (depth > ME_JSON_MAX_RECURSION) {
        return jerror(dest, "Blend recursion limit exceeded");
    }
    //  Cast const away because ITERATE macros make this difficult
    src = (Json*) csrc;
    if (dest->lock) {
        return jerror(dest, "Cannot blend into a locked JSON object");
    }
    if (src == 0 || src->count == 0) {
        return 0;
    }
    if (dest->count == 0) {
        allocNode(dest, JSON_OBJECT, 0, 0);
    }
    if (dest == src) {
        //  jsonParseText parses in-situ and takes ownership of srcData
        srcData = jsonToString(src, sid, 0, 0);
        src = tmpSrc = jsonAlloc();
        jsonParseText(tmpSrc, srcData, 0);
        sid = 0;
    } else {
        tmpSrc = 0;
    }
    if (dkey && *dkey) {
        if ((id = jquery(dest, did, dkey, 0, 0)) < 0) {
            /*
                Creating the destination key can fail -- an array parent cannot take a
                named property, and the node array can hit its limit. The result was
                used as an index without checking, so a failure indexed out of bounds.
             */
            if ((did = jquery(dest, did, dkey, "", JSON_OBJECT)) < 0) {
                jsonFree(tmpSrc);
                return R_ERR_CANT_CREATE;
            }
        } else {
            did = id;
        }
    }
    if (skey && *skey) {
        if ((id = jquery(src, sid, skey, 0, 0)) < 0) {
            return 0;
        } else {
            sid = id;
        }
    }
    dp = &dest->nodes[did];
    sp = &src->nodes[sid];

    if ((JSON_OBJECT & dp->type) != (JSON_OBJECT & sp->type)) {
        if (flags & (JSON_APPEND | JSON_REPLACE)) {
            jsonFree(tmpSrc);
            return R_ERR_BAD_ARGS;
        }
    }
    if (sp->type & JSON_OBJECT) {
        if (!(dp->type & JSON_OBJECT)) {
            // Convert destination to object
            setNode(dest, did, sp->type, dp->name, 1, 0, 0);
        }
        //  Examine each property for: JSON_APPEND (default) | JSON_REPLACE)
        for (ITERATE_JSON(src, sp, spc, sidc)) {
            property = src->nodes[sidc].name;
            pflags = flags;
            //  A null name has no combine prefix to decode. Belt and braces: the parser
            //  no longer creates one, but the node API can still set one.
            if (flags & JSON_COMBINE && property) {
                kind = property[0];
                if (kind == '+') {
                    pflags = JSON_APPEND | (flags & JSON_COMBINE);
                    property++;
                } else if (kind == '-') {
                    pflags = JSON_REPLACE | (flags & JSON_COMBINE);
                    property++;
                } else if (kind == '?') {
                    pflags = JSON_CCREATE | (flags & JSON_COMBINE);
                    property++;
                } else if (kind == '=') {
                    pflags = JSON_OVERWRITE | (flags & JSON_COMBINE);
                    property++;
                } else {
                    pflags = JSON_OVERWRITE | (flags & JSON_COMBINE);
                }
            }
            if ((didc = findProperty(dest, did, property)) < 0) {
                // Absent in destination, copy node and children
                if (!(pflags & JSON_REPLACE)) {
                    at = dp->last;
                    if (insertNodes(dest, at, 1, did) < 0) {
                        jsonFree(tmpSrc);
                        return R_ERR_MEMORY;
                    }
                    dp = &dest->nodes[did];
                    if (spc->type & (JSON_ARRAY | JSON_OBJECT)) {
                        setNode(dest, at, spc->type, property, 1, 0, 0);
                        if ((rc = blendRecurse(dest, at, 0, src, sidc, 0, pflags & ~JSON_CCREATE, depth + 1)) < 0) {
                            jsonFree(tmpSrc);
                            return rc;
                        }
                        dp = &dest->nodes[did];
                    } else {
                        copyNodes(dest, at, src, sidc, 1);
                        setNode(dest, at, spc->type, property, 1, spc->value, 1);
                    }
                }

            } else if (!(pflags & JSON_CCREATE)) {
                // Already present in destination
                dpc = &dest->nodes[didc];
                if (spc->type & JSON_OBJECT && !(dpc->type & JSON_OBJECT)) {
                    removeNodes(dest, didc, dpc->last - didc - 1);
                    setNode(dest, didc, JSON_OBJECT, property, 1, 0, 0);
                }
                if ((rc = blendRecurse(dest, didc, 0, src, sidc, 0, pflags, depth + 1)) < 0) {
                    jsonFree(tmpSrc);
                    return rc;
                }
                /*
                    A JSON_REPLACE that empties a property leaves it in place with an empty
                    value. Pruning the property instead would be a behaviour change, so it is
                    not done here.
                 */
                dp = &dest->nodes[did];
            }
        }
    } else if (sp->type & JSON_ARRAY) {
        if (flags & JSON_REPLACE) {
            if (dp->type & JSON_ARRAY) {
                for (ITERATE_JSON(src, sp, spc, sidc)) {
                    for (ITERATE_JSON(dest, dp, dpc, didc)) {
                        if (dpc->value && *dpc->value && smatch(dpc->value, spc->value)) {
                            //  We're mutating the destination here, but immediately breaking out of the loop
                            removeNodes(dest, didc, 1);
                            dp = &dest->nodes[did];
                            break;
                        }
                    }
                }
            }
        } else if (flags & JSON_CCREATE) {
            // Already present

        } else if (flags & JSON_APPEND) {
            at = dp->last;
            slen = (int) (sp->last - sid - 1);
            if (slen > 0) {
                if (insertNodes(dest, at, slen, did) < 0) {
                    jsonFree(tmpSrc);
                    return R_ERR_MEMORY;
                }
                copyNodes(dest, at, src, sid + 1, slen);
            }

        } else {
            // Default is to JSON_OVERWRITE
            slen = (int) (sp->last - sid);
            dlen = (int) (dp->last - did);
            if (dlen > slen) {
                removeNodes(dest, did + 1, dlen - slen);
            } else if (dlen < slen && insertNodes(dest, did + 1, slen - dlen, did) < 0) {
                jsonFree(tmpSrc);
                return R_ERR_MEMORY;
            }
            /*
                insertNodes may have grown the node array, which reallocates it and
                invalidates every JsonNode pointer into this tree. Node IDs survive;
                pointers do not.
             */
            dp = &dest->nodes[did];
            if (--slen > 0) {
                // Keep the existing array and just copy the elements
                copyNodes(dest, did + 1, src, sid + 1, slen);
                if (dp->allocatedValue) {
                    rFree(dp->value);
                    dp->value = 0;
                }
                dp->type = JSON_ARRAY;
            }
        }
    } else {
        if (flags & JSON_APPEND) {
            //  Build the joined value before releasing the old one, as JSON_REPLACE does
            value = sjoin(dp->value, " ", sp->value, NULL);
            freeNode(dp);
            dp->value = value;
            dp->allocatedValue = 1;
            dp->type = JSON_STRING;

        } else if (flags & JSON_REPLACE) {
            value = sreplace(dp->value, sp->value, NULL);
            freeNode(dp);
            dp->value = value;
            dp->allocatedValue = 1;
            dp->type = sp->type;

        } else if (flags & JSON_CCREATE) {
            // Do nothing

        } else if (flags & JSON_REMOVE_UNDEF && smatch(sp->value, "undefined")) {
            removeNodes(dest, did, 1);
            // dp = &dest->nodes[did];

        } else {
            copyNodes(dest, did, src, sid, 1);
        }
    }
    jsonFree(tmpSrc);
    return 0;
}

/*
    Deep copy of a JSON tree
 */
PUBLIC Json *jsonClone(const Json *csrc, int flags)
{
    Json *dest;

    (void) flags;

    dest = jsonAlloc();
    if (csrc && jsonBlend(dest, 0, 0, csrc, 0, 0, 0) < 0) {
        jsonFree(dest);
        return NULL;
    }
    return dest;
}
#endif /* JSON_BLEND */

static void spaces(RBuf *buf, int count)
{
    int i, j;

    if (!buf || count < 0) {
        return;
    }
    for (i = 0; i < count; i++) {
        for (j = 0; j < indentLevel; j++) {
            rPutCharToBuf(buf, ' ');
        }
    }
}

#if JSON_TRIGGER
PUBLIC void jsonSetTrigger(Json *json, JsonTrigger proc, void *arg)
{
    json->trigger = proc;
    json->triggerArg = arg;
}
#endif

PUBLIC void jsonSetMaxLength(int length)
{
    maxLength = length;
}

PUBLIC void jsonSetIndent(int indent)
{
    indentLevel = indent;
}

/*
    Expand scalar and JSON container references. keep preserves unresolved tokens.
    Return a clone for empty input or missing json; return NULL for unterminated tokens.
 */
PUBLIC char *jsonTemplate(Json *json, cchar *str, bool keep)
{
    RBuf     *buf;
    JsonNode *node;
    cchar    *value;
    char     *src, *cp, *start, *tok, *rendered;
    int      nid;

    if (!str || schr(str, '$') == 0 || !json) {
        return sclone(str);
    }
    buf = rAllocBuf(ME_JSON_BUFSIZE);
    for (src = (char*) str; *src; src++) {
        if (src[0] == '$' && src[1] == '{') {
            for (cp = start = src + 2; *cp && *cp != '}'; cp++) {
            }
            if (*cp == '\0') {
                // Unterminated token. The buffer was allocated before the loop.
                rFreeBuf(buf);
                return NULL;
            }
            tok = snclone(start, (size_t) (cp - start));
            rendered = NULL;
            value = jsonGet(json, 0, tok, 0);
            if (*tok && value && (nid = jsonGetId(json, 0, tok)) >= 0) {
                node = &json->nodes[nid];
                if (node->type & (JSON_OBJECT | JSON_ARRAY)) {
                    value = rendered = jsonToString(json, nid, 0, JSON_JSON);
                }
            }
            if (*tok && value) {
                rPutStringToBuf(buf, value);
            } else if (keep) {
                rPutStringToBuf(buf, "${");
                rPutStringToBuf(buf, tok);
                rPutCharToBuf(buf, '}');
            }
            rFree(rendered);
            src = (*cp == '}') ? cp : cp - 1;
            rFree(tok);
        } else {
            rPutCharToBuf(buf, *src);
        }
    }
    return rBufToStringAndFree(buf);
}

PUBLIC int jsonCheckIteration(struct Json *json, int count, int nid)
{
    if (json->count != count) {
        rError("json", "Json iteration error. MUST not permute JSON nodes while iterating.");
        return R_ERR_BAD_ARGS;
    }
    return nid;
}

/*
    Test whether a token is a JSON number, per the RFC 7159 grammar:

        [ '-' ] ( '0' | [1-9] *digit ) [ '.' 1*digit ] [ ('e'|'E') [ '+'|'-' ] 1*digit ]

    This is a grammar, not a character-set test. Anything else -- "01", "1.", "1e",
    "-", "1-2", "12x" -- is a string, and quoting it is what keeps JSON output valid.
 */
static bool isfnumber(cchar *s, size_t len)
{
    cchar *cp, *end;

    if (!s || !*s || len == 0) {
        return 0;
    }
    cp = s;
    end = s + len;

    if (*cp == '-') {
        cp++;
    }
    if (cp == end || !isdigit((uchar) * cp)) {
        return 0;
    }
    if (*cp == '0') {
        //  A leading zero may not be followed by more digits
        cp++;
    } else {
        while (cp < end && isdigit((uchar) * cp)) cp++;
    }
    if (cp < end && *cp == '.') {
        cp++;
        if (cp == end || !isdigit((uchar) * cp)) {
            return 0;
        }
        while (cp < end && isdigit((uchar) * cp)) cp++;
    }
    if (cp < end && (*cp == 'e' || *cp == 'E')) {
        cp++;
        if (cp < end && (*cp == '+' || *cp == '-')) {
            cp++;
        }
        if (cp == end || !isdigit((uchar) * cp)) {
            return 0;
        }
        while (cp < end && isdigit((uchar) * cp)) cp++;
    }
    return cp == end;
}

PUBLIC cchar *jsonGetError(Json *json)
{
    return json->error;
}

/*
   Set the json error message
 */
static int jerror(Json *json, cchar *fmt, ...)
{
    va_list args;
    char    *msg;

    if (!json || !fmt) {
        return R_ERR_BAD_ARGS;
    }
    if (!json->error) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        va_end(args);
        if (json->path) {
            json->error = sfmt("JSON Parse Error: %s\nIn file '%s' at line %d. Near => %s\n", msg, json->path,
                               json->lineNumber + 1, json->next);
        } else {
            json->error =
                sfmt("JSON Parse Error: %s\nAt line %d. Near:\n%s\n", msg, json->lineNumber + 1, json->next);
        }
        rTrace("json", "%s", json->error);
        rFree(msg);
    }
    return R_ERR_BAD_STATE;
}

#else
void dummyJson()
{
}
#endif /* ME_COM_JSON */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyJson(){}
#endif /* ME_COM_JSON */
