/*
 * JSON library Library Source
 */

#include "json.h"

#if ME_COM_JSON



/********* Start of file ../../../src/jsonLib.c ************/

/*
    json.c - JSON parser and query engine.

    This modules provides APIs to load and save JSON to files. The query module provides a
    high performance lookup of in-memory parsed JSON node trees.

    JSON text is parsed and converted to a node tree with a query API to permit searching
    and updating the in-memory JSON tree. The tree can then be serialized to send or save.

    This file supports JSON and JSON 5/6 which roughly parallels Javascript object notation.
    Specifically, keys to not always have to be quoted if they do not contain spaces. Trailing
    commas are not required. Comments are supported and preserved.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if ME_COM_JSON
/*********************************** Locals ***********************************/

#ifndef ME_JSON_INC
    #define ME_JSON_INC              64
#endif
#ifndef ME_JSON_MAX_RECURSION
    #define ME_JSON_MAX_RECURSION    1000
#endif

#ifndef ME_JSON_DEFAULT_PROPERTY
    #define ME_JSON_DEFAULT_PROPERTY 64
#endif

#define JSON_TMP                     ".tmp.json"

/********************************** Forwards **********************************/

static JsonNode *allocNode(Json *json, int type, cchar *name, cchar *value);
static int blendRecurse(Json *dest, int did, cchar *dkey, const Json *csrc, int sid, cchar *skey, int flags, int depth);
static char *copyProperty(Json *json, cchar *key);
static void freeNode(JsonNode *node);
static bool isfnumber(cchar *s, ssize len);
static int jerror(Json *json, cchar *fmt, ...);
static int jquery(Json *json, int nid, cchar *key, cchar *value, int type);
static int parseJson(Json *json, char *text, int flags);
static int sleuthValueType(cchar *value, ssize len);
static void spaces(RBuf *buf, int count);

/************************************* Code ***********************************/

PUBLIC Json *jsonAlloc(int flags)
{
    Json *json;

    json = rAllocType(Json);
    json->flags = flags;
    json->lineNumber = 1;
    json->strict = (flags & JSON_STRICT) ? 1 : 0;
    json->size = ME_JSON_INC;
    json->nodes = rAlloc(sizeof(JsonNode) * json->size);
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
    rFree(json->errorMsg);
    rFree(json->path);
    rFree(json->nodes);

    // Help detect double free
    json->nodes = 0;
    rFree(json);
}

static void freeNode(JsonNode *node)
{
    assert(node);

    if (node->allocatedName) {
        rFree(node->name);
        node->allocatedName = 0;
    }
    if (node->allocatedValue) {
        rFree(node->value);
        node->allocatedValue = 0;
    }
}

static bool growNodes(Json *json, int num)
{
    void *p;

    assert(json);
    assert(num > 0);

    if (num > ME_JSON_MAX_NODES) {
        jerror(json, "Too many nodes");
        return 0;
    }
    if ((json->count + num) > json->size) {
        json->size += max(num, ME_JSON_INC);
        if (json->size > ME_JSON_MAX_NODES) {
            jerror(json, "Too many nodes");
            return 0;
        }
        p = rRealloc(json->nodes, sizeof(JsonNode) * json->size);
        if (p == 0) {
            jerror(json, "Cannot allocate memory");
            return 0;
        }
        json->nodes = p;
    }
    return 1;
}

static void initNode(Json *json, int nid)
{
    JsonNode *node;

    assert(json);
    assert(0 <= nid && nid < json->count);

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

    assert(json);
    assert(0 <= nid && nid < json->count);

    node = &json->nodes[nid];
    node->type = type;

    if (name != node->name && !smatch(name, node->name)) {
        if (node->allocatedName) {
            node->allocatedName = 0;
            rFree(node->name);
        }
        node->name = 0;

        if (name) {
            node->allocatedName = allocatedName;
            if (allocatedName) {
                name = sclone(name);
            }
            node->name = (char*) name;
        }
    }

    if (value != node->value && !smatch(value, node->value)) {
#if JSON_TRIGGER
        if (json->trigger) {
            (json->trigger)(json->triggerArg, json, node, name, value, node->value);
        }
#endif
        if (node->allocatedValue) {
            node->allocatedValue = 0;
            rFree(node->value);
        }
        node->value = 0;

        if (value) {
            node->allocatedValue = allocatedValue;
            if (allocatedValue) {
                value = sclone(value);
            }
            node->value = (char*) value;
        }
    }
}

static JsonNode *allocNode(Json *json, int type, cchar *name, cchar *value)
{
    int nid;

    assert(json);

    if (json->count >= json->size && !growNodes(json, 1)) {
        return 0;
    }
    nid = json->count++;
    initNode(json, nid);
    setNode(json, nid, type, name, 0, value, 0);
    return &json->nodes[nid];
}

/*
    Copy nodes for jsonBlend only
 */
static void copyNodes(Json *dest, int did, Json *src, int sid, ssize slen)
{
    JsonNode *dp, *sp;
    int      i;

    assert(dest);
    assert(src);
    assert(0 <= did && did < dest->count);
    assert(0 <= sid && sid < src->count);

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
    Insert room for 'num' nodes at json-nodes[at]. This should always be at the end of an array or object.
 */
static int insertNodes(Json *json, int nid, int num, int parentId)
{
    JsonNode *node;
    int      i;

    assert(json);
    assert(0 <= nid && nid <= json->count);
    assert(num > 0);

    if ((json->count + num) >= json->size && !growNodes(json, num)) {
        return R_ERR_MEMORY;
    }
    node = &json->nodes[nid];
    if (nid < json->count) {
        memmove(node + num, node, (json->count - nid) * sizeof(JsonNode));
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
            assert(node->last >= 0);
        }
    }
    for (i = 0; i < num; i++) {
        initNode(json, nid + i);
    }
    return nid;
}

static int removeNodes(Json *json, int nid, int num)
{
    JsonNode *node;
    int      i;

    assert(json);
    assert(0 <= nid && nid < json->count);
    assert(num >= 0);

    if (num <= 0) {
        return 0;
    }
    node = &json->nodes[nid];
    for (i = 0; i < num; i++) {
        freeNode(&json->nodes[nid + i]);
    }
    json->count -= num;

    if (nid < json->count) {
        memmove(node, node + num, (json->count - nid) * sizeof(JsonNode));
    }
    for (i = 0; i < json->count; i++) {
        node = &json->nodes[i];
        if (node->last > nid) {
            node->last -= num;
            assert(node->last >= 0);
        }
    }
    return nid;
}

PUBLIC void jsonLock(Json *json)
{
    json->flags |= JSON_LOCK;
}

PUBLIC void jsonUnlock(Json *json)
{
    json->flags &= ~JSON_LOCK;
}

/*
    Parse json text and return a JSON tree.
    Tolerant of null or empty text.
 */
PUBLIC Json *jsonParse(cchar *ctext, int flags)
{
    Json *json;
    char *text;

    json = jsonAlloc(flags);
    text = sclone(ctext);
    if (parseJson(json, text, flags) < 0) {
        if (json->errorMsg && !rEmitLog("json", "trace")) {
            rError("json", "%s", json->errorMsg);
        }
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

    json = jsonAlloc(flags);
    if (parseJson(json, text, flags) < 0) {
        if (json->errorMsg && !rEmitLog("json", "trace")) {
            rError("json", "%s", json->errorMsg);
        }
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
    json = jsonParseKeep(buf, 0);
    va_end(ap);
    msg = jsonToString(json, 0, 0, JSON_STRICT);
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
    msg = jsonToString(json, 0, 0, JSON_STRICT);
    sncopy(buf, size, msg, slen(msg));
    rFree(msg);
    jsonFree(json);
    return buf;
}

/*
    Parse JSON text and return a JSON tree and error message if parsing fails.
 */
PUBLIC Json *jsonParseString(cchar *atext, char **errorMsg, int flags)
{
    Json *json;
    char *text;

    if (errorMsg) {
        *errorMsg = 0;
    }
    json = jsonAlloc(flags);
    text = sclone(atext);

    if (parseJson(json, text, flags) < 0) {
        if (errorMsg) {
            *errorMsg = (char*) json->errorMsg;
            json->errorMsg = 0;
        }
        jsonFree(json);
        return 0;
    }
    return json;

}

/*
    Parse JSON text from a file
 */
PUBLIC Json *jsonParseFile(cchar *path, char **errorMsg, int flags)
{
    Json *json;
    char *text;

    assert(path && *path);

    if (errorMsg) {
        *errorMsg = 0;
    }
    if ((text = rReadFile(path, 0)) == 0) {
        if (errorMsg) {
            *errorMsg = sfmt("Cannot open: \"%s\"", path);
        }
        return 0;
    }
    json = jsonAlloc(flags);
    if (path) {
        json->path = sclone(path);
    }
    if (parseJson(json, text, flags) < 0) {
        if (errorMsg) {
            *errorMsg = json->errorMsg;
            json->errorMsg = 0;
        }
        jsonFree(json);
        return 0;
    }
    return json;
}

/*
    Save the JSON tree to a file.
    The tree rooted at the node specified by "nid/key" is saved.
    Flags can be JSON_PRETTY for a human readable format.
 */
PUBLIC int jsonSave(Json *json, int nid, cchar *key, cchar *path, int mode, int flags)
{
    char *text, *tmp;
    int  fd;
    ssize len;

    assert(json);
    assert(path && *path);

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
    if (write(fd, text, len) != len) {
        rFree(text);
        rFree(tmp);
        close(fd);
        return R_ERR_CANT_WRITE;
    }
    close(fd);
    rFree(text);

    if (rename(tmp, path) < 0) {
        rFree(tmp);
        return R_ERR_CANT_WRITE;
    }
    rFree(tmp);
    return 0;
}

/*
    Set the json error message
 */
static int jerror(Json *json, cchar *fmt, ...)
{
    va_list args;
    char    *msg;

    assert(json);
    assert(fmt);

    if (!json->errorMsg) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        va_end(args);
        if (json->path) {
            json->errorMsg = sfmt("JSON Parse Error: %s\nIn file '%s' at line %d. Near:\n%s\n", msg, json->path,
                                  json->lineNumber + 1, json->next);
        } else {
            json->errorMsg =
                sfmt("JSON Parse Error: %s\nAt line %d. Near:\n%s\n", msg, json->lineNumber + 1, json->next);
        }
        rTrace("json", "%s", json->errorMsg);
        rFree(msg);
    }
    return R_ERR_BAD_STATE;
}

/*
    Parse primitive values including unquoted strings
 */
static int parsePrimitive(Json *json)
{
    char *next, *start;

    assert(json);
    assert(json->next);

    for (start = next = json->next; next < json->end && *next; next++) {
        switch (*next) {
        case '\n':
            json->lineNumber++;
        // Fall through
        case ' ':
        case '\t':
        case '\r':
            *next = 0;
            json->next = next;
            return 0;

        //  Balance nest '{' ']'
        case '}':
        case ']':
        case ':':
        case ',':
            json->next = next - 1;
            return 0;

        default:
            if (*next != '_' && *next != '-' && *next != '.' && !isalnum((uchar) * next)) {
                *next = 0;
                json->next = next;
                return 0;
            }
            if (*next < 32 || *next >= 127) {
                json->next = start;
                return jerror(json, "Illegal character in primitive");
            }
            if ((*next == '.' || *next == '[') && (next == start || !isalnum((uchar) next[-1]))) {
                return jerror(json, "Illegal dereference in primitive");
            }
        }
    }
    json->next = next - 1;
    return 0;
}

static int parseRegExp(Json *json)
{
    char *end, *next, *start, c;

    assert(json);
    assert(json->next);
    assert(json->end);

    start = json->next;
    end = json->end;

    for (next = start; next < end && *next; next++) {
        c = *next;
        if (c == '/' && next[-1] != '\\') {
            *next = '\0';
            json->next = next;
            return 0;
        }
    }
    // Ran out of input
    json->next = start;
    jerror(json, "Incomplete regular expression");
    return R_ERR_BAD_STATE;
}

static int parseString(Json *json)
{
    char *end, *next, *op, *start, c, quote;
    int  d, j;

    assert(json);
    assert(json->next);
    assert(json->end);

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
                for (j = c = 0, next++; j < 4 && next < end && *next; next++, j++) {
                    d = tolower((uchar) * next);
                    if (isdigit((uchar) d)) {
                        c = (c * 16) + d - '0';
                    } else if (d >= 'a' && d <= 'f') {
                        c = (c * 16) + d - 'a' + 10;
                    } else {
                        jerror(json, "Unexpected hex characters");
                        return R_ERR_BAD_STATE;
                    }
                }
                if (j < 4) {
                    jerror(json, "Invalid unicode characters");
                    return R_ERR_BAD_STATE;
                }
                next--;
                break;

            default:
                json->next = start;
                jerror(json, "Unexpected characters in string");
                return R_ERR_BAD_STATE;
            }
            *op = c;

        } else if (c == quote) {
            *op = '\0';
            json->next = next;
            return 0;

        } else if (op != next) {
            *op = c;
        }
    }
    // Ran out of input
    json->next = start;
    jerror(json, "Incomplete string");
    return R_ERR_BAD_STATE;
}

static int parseComment(Json *json)
{
    char *next;
    int  startLine;

    assert(json);
    assert(json->next);

    next = json->next;
    startLine = json->lineNumber;

    if (*next == '/') {
        for (next++; next < json->end && *next && *next != '\n'; next++) {
        }

    } else if (*next == '*') {
        for (next++; next < json->end && *next && (*next != '*' || next[1] != '/'); next++) {
            if (*next == '\n') {
                json->lineNumber++;
            }
        }
        if (*next) {
            next += 2;
        } else {
            return jerror(json, "Cannot find end of comment started on line %d", startLine);
        }
    }
    json->next = next - 1;
    return 0;
}

/*
    Parse the text and assume ownership of the text.
 */
static int parseJson(Json *json, char *text, int flags)
{
    JsonNode *node;
    char     *name, *value;
    uchar    c;
    int      level, parent, prior, type, rc;

    assert(json);

    json->next = json->text = text;
    json->end = &json->text[slen(text)];

    name = 0;
    parent = -1;
    level = 0;

    for (; json->next < json->end && *json->next; json->next++) {
        c = *json->next;
        switch (c) {
        case '{':
        case '[':
            *json->next = 0;
            level++;
            type = (c == '{' ? JSON_OBJECT : JSON_ARRAY);
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
            *json->next = 0;
            prior = json->nodes[parent].last;
            json->nodes[parent].last = json->count;
            parent = prior;
            name = 0;
            break;

        case '/':
            if (++json->next < json->end && (*json->next == '*' || *json->next == '/')) {
                if (parseComment(json) < 0) {
                    return R_ERR_BAD_STATE;
                }
            } else {
                type = JSON_REGEXP;
                value = json->next;
                rc = parseRegExp(json);
                goto value;
            }
            break;

        case '\n':
            json->lineNumber++;
            break;

        case '\t':
        case '\r':
        case ' ':
            break;

        case ',':
            name = 0;
            *json->next = 0;
            break;

        case ':':
            if (!name) {
                return jerror(json, "Missing property name");
            }
            *json->next = 0;
            break;

        case '"':
            type = JSON_STRING;
            value = json->next + 1;
            rc = parseString(json);
            goto value;

        case '\'':
        case '`':
            if (flags & JSON_STRICT) {
                return jerror(json, "Single quotes are not allowed in strict mode");
            }
            type = JSON_STRING;
            value = json->next + 1;
            rc = parseString(json);
            goto value;

        default:
            value = json->next;
            rc = parsePrimitive(json);
            if (*value == 0) {
                return jerror(json, "Empty primitive token");
            }
            type = sleuthValueType(value, json->next - value + 1);
            if (type != JSON_PRIMITIVE) {
                if (level == 0 || (flags & JSON_STRICT)) {
                    return jerror(json, "Invalid primitive token");
                }
            }
            goto value;

value:
            if (rc < 0) {
                return R_ERR_BAD_STATE;
            }
            if (parent >= 0 && json->nodes[parent].type == JSON_ARRAY) {
                allocNode(json, type, 0, value);
            } else if (name) {
                // The Object value may not be null terminated here. It will be terminated after parsing the next token.
                allocNode(json, type, name, value);
                name = 0;
            } else if (parent >= 0) {
                // Object property name
                name = value;
            } else {
                // Value outside an array or object
                allocNode(json, type, 0, value);
            }
            break;
        }
    }
    if (json->next) {
        *json->next = 0;
    }
    if (level != 0) {
        return jerror(json, "Unclosed brace/bracket");
    }
    return 0;
}

static int sleuthValueType(cchar *value, ssize len)
{
    uchar c;
    int   type;

    assert(value);

    if (!value) {
        return JSON_PRIMITIVE;
    }
    c = value[0];
    if ((c == 't' && sncmp(value, "true", len) == 0) ||
        (c == 'f' && sncmp(value, "false", len) == 0) ||
        (c == 'n' && sncmp(value, "null", len) == 0) ||
        (c == 'u' && sncmp(value, "undefined", len) == 0)) {
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
    assert(json);

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
    char  *start, *end, *seps;
    ssize i;

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
            *type = JSON_ARRAY;
            *end++ = '\0';
        } else if (*end == '.') {
            *type = JSON_OBJECT;
            *end++ = '\0';
        } else {
            //  Strip off matching quotes
            if ((*start == '"' || *start == '\'') && end > start && end[-1] == *start) {
                start++;
                end[-1] = '\0';
            }
            *end++ = '\0';
            i = strspn(end, seps);
            end += i;
            if (*end == '\0') {
                end = 0;
            }
        }
    }
    *rest = end;
    return start;
}

static int findProperty(Json *json, int nid, cchar *property)
{
    JsonNode *node, *np;
    int      index, id;

    assert(json);
    assert((0 <= nid && nid < json->count) || json->count == 0);

    if (json->count == 0) {
        return R_ERR_CANT_FIND;
    }
    node = &json->nodes[nid];
    if (!property || *property == 0) {
        return R_ERR_CANT_FIND;
    }
    if (node->type == JSON_ARRAY) {
        if (!isdigit((uchar) * property) || (index = (int) stoi(property)) < 0) {
            for (id = nid + 1; id < node->last; id = np->last) {
                np = &json->nodes[id];
                if (smatch(property, np->value)) {
                    return id;
                }
            }
            return R_ERR_CANT_FIND;

        } else {
            if (index >= INT_MAX) {
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
    Internal JSON get/set query.
 */
static int jquery(Json *json, int nid, cchar *key, cchar *value, int type)
{
    char *property, *rest;
    int  cid, id, ntype, qtype;

    assert(json);
    assert((0 <= nid && nid < json->count) || json->count == 0);

    if (key == 0 || *key == '\0') {
        return R_ERR_CANT_FIND;
    }
    property = copyProperty(json, key);

    qtype = 0;
    rest = 0;

    for (; (property = getNextTerm(property, &rest, &qtype)) != 0; property = rest, nid = id) {
        id = findProperty(json, nid, property);
        if (value) {
            if (id < 0) {
                // Property not found
                if (nid >= json->count) {
                    allocNode(json, JSON_OBJECT, 0, 0);
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
                    ntype = json->nodes[id].type;
                    //  Node type mismatch. But allow query type to be an array index even if node is an object
                    if (ntype != qtype && !(ntype == JSON_OBJECT && qtype == JSON_ARRAY)) {
                        setNode(json, id, qtype, property, 1, 0, 0);
                    }
                } else {
                    //  Property found. Just update the value
                    setNode(json, id, type, property, 1, value, 1);
                }
            }
        } else {
            /*
                Get or remove
             */
            if (id < 0) {
                return R_ERR_CANT_FIND;
            }
            if (rest == 0) {
                nid = id;
                break;
            }
        }
    }
    return nid;
}

static char *copyProperty(Json *json, cchar *key)
{
    ssize len;
    void *p;

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
    assert(json);

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
        if ((nid = jquery((Json*)json, nid, key, 0, 0)) < 0) {
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

    assert(json);

    for (ITERATE_JSON_ID(json, pid, child, id)) {
        if (--nth <= 0) {
            return child;
        }
    }
    return 0;
}

/*
    Get a property value. The nid provides the index of the base node, and the "name"
    is search using that index as a starting point. Name can contain "." or "[]".

    This returns a short-term reference into the JSON tree. It is not stable over updates.
 */
PUBLIC cchar *jsonGet(Json *json, int nid, cchar *key, cchar *defaultValue)
{
    JsonNode *node;

    if (!json || nid < 0 || nid >= json->count) {
        return defaultValue;
    }
    if (key && *key) {
        if ((nid = jquery(json, nid, key, 0, 0)) < 0) {
            return defaultValue;
        }
    }
    node = &json->nodes[nid];
    if (node->type & JSON_OBJECT) {
        return "{}";
    } else if (node->type & JSON_ARRAY) {
        return "[]";
    } else if (node->type & JSON_PRIMITIVE && smatch(json->nodes[nid].value, "null")) {
        return defaultValue;
    }
    return json->nodes[nid].value;
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

PUBLIC int jsonGetInt(Json *json, int nid, cchar *key, int defaultValue)
{
    cchar *value;
    char  defbuf[16];

    sfmtbuf(defbuf, sizeof(defbuf), "%d", defaultValue);
    value = jsonGet(json, nid, key, defbuf);
    return (int) stoi(value);
}


PUBLIC int64 jsonGetNum(Json *json, int nid, cchar *key, int64 defaultValue)
{
    cchar *value;
    char  defbuf[16];

    sfmtbuf(defbuf, sizeof(defbuf), "%lld", defaultValue);
    value = jsonGet(json, nid, key, defbuf);
    return stoi(value);
}

PUBLIC double jsonGetDouble(Json *json, int nid, cchar *key, double defaultValue)
{
    cchar *value;
    char  defbuf[16];

    sfmtbuf(defbuf, sizeof(defbuf), "%f", defaultValue);
    value = jsonGet(json, nid, key, defbuf);
    return atof(value);
}

PUBLIC uint64 jsonGetValue(Json *json, int nid, cchar *key, cchar *defaultValue)
{
    cchar *value;

    value = jsonGet(json, nid, key, defaultValue);
    return svalue(value);
}

/*
    Set a property value. The nid provides the index of the base node, and the "name"
    is search using that index as a starting point. Name can contain "." or "[]".
 */
PUBLIC int jsonSet(Json *json, int nid, cchar *key, cchar *value, int type)
{
    if (!json) {
        return R_ERR_BAD_ARGS;
    }
    assert((0 <= nid && nid < json->count) || json->count == 0);

    if (json->flags & JSON_LOCK) {
        return jerror(json, "Cannot set value in a locked JSON object");
    }
    if (type <= 0 && value) {
        type = sleuthValueType(value, slen(value));
    }
    if (!value) {
        value = "undefined";
    }
    return jquery(json, nid, key, value, type);
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

    result = jsonSet(json, nid, key, value, sleuthValueType(value, slen(value)));
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
    if (node->allocatedValue) {
        rFree(node->value);
        node->allocatedValue = 0;
    }
    if (flags & JSON_PASS_VALUE) {
        node->value = (char*) value;
    } else {
        node->value = sclone(value);
    }
    node->allocatedValue = 1;
    node->type = type;
}

PUBLIC void jsonSetNodeType(JsonNode *node, int type)
{
    node->type = type;
}

PUBLIC int jsonRemove(Json *json, int nid, cchar *key)
{
    if (!json) {
        return R_ERR_BAD_ARGS;
    }
    assert(0 <= nid && nid < json->count);

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
PUBLIC void jsonToBuf(RBuf *buf, cchar *value, int flags)
{
    cchar *cp;
    int   quotes;

    assert(buf);

    if (!value) {
        rPutStringToBuf(buf, "null");
        return;
    }
    quotes = 0;
    if (!(flags & JSON_BARE)) {
        if ((flags & JSON_KEY) && value && *value) {
            quotes = flags & (JSON_QUOTES | JSON_STRICT);
            if (!quotes) {
                for (cp = value; *cp; cp++) {
                    if (!isalnum((uchar) * cp) && *cp != '_') {
                        quotes++;
                        break;
                    }
                }
            }
        } else {
            quotes = 1;
        }
    }
    if (quotes) {
        rPutCharToBuf(buf, '\"');
    }
    if (value) {
        for (cp = value; *cp; cp++) {
            if (*cp == '\"' || *cp == '\\') {
                rPutCharToBuf(buf, '\\');
                rPutCharToBuf(buf, *cp);
            } else if (*cp == '\b') {
                rPutStringToBuf(buf, "\\b");
            } else if (*cp == '\f') {
                rPutStringToBuf(buf, "\\f");
            } else if (*cp == '\n') {
                if (flags & (JSON_SINGLE | JSON_STRICT)) {
                    rPutStringToBuf(buf, "\\n");
                } else {
                    rPutCharToBuf(buf, '\n');
                }
            } else if (*cp == '\r') {
                if (flags & (JSON_SINGLE | JSON_STRICT)) {
                    rPutStringToBuf(buf, "\\r");
                } else {
                    rPutCharToBuf(buf, '\r');
                }
            } else if (*cp == '\t') {
                if (flags & (JSON_SINGLE | JSON_STRICT)) {
                    rPutStringToBuf(buf, "\\t");
                } else {
                    rPutCharToBuf(buf, '\t');
                }
            } else if (iscntrl((uchar) * cp)) {
                rPutToBuf(buf, "\\u%04x", *cp);
            } else {
                rPutCharToBuf(buf, *cp);
            }
        }
    }
    if (quotes) {
        rPutCharToBuf(buf, '\"');
    }
}

static int nodeToString(const Json *json, RBuf *buf, int nid, int indent, int flags)
{
    JsonNode *node;
    bool     pretty;

    assert(json);
    assert(buf);
    assert(0 <= nid && nid <= json->count);
    assert(0 <= indent);
    assert(indent < ME_JSON_MAX_RECURSION);

    if (indent > ME_JSON_MAX_RECURSION) {
        return R_ERR_BAD_ARGS;
    }
    if (nid < 0 || nid > json->count) {
        return R_ERR_BAD_ARGS;
    }
    if (json->count == 0) {
        return nid;
    }
    node = &json->nodes[nid];
    pretty = (flags & JSON_PRETTY) ? 1 : 0;

    if (flags & JSON_DEBUG) {
        rPutToBuf(buf, "<%d/%d> ", nid, node->last);
    }
    if (node->type & JSON_PRIMITIVE) {
        rPutStringToBuf(buf, node->value);
        nid++;

    } else if (node->type & JSON_REGEXP) {
        rPutCharToBuf(buf, '/');
        rPutStringToBuf(buf, node->value);
        rPutCharToBuf(buf, '/');
        nid++;

    } else if (node->type == JSON_STRING) {
        jsonToBuf(buf, node->value, flags);
        nid++;

    } else if (node->type == JSON_ARRAY) {
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, '[');
        }
        if (pretty) rPutCharToBuf(buf, '\n');
        for (++nid; nid < node->last; ) {
            if (json->nodes[nid].type == 0) {
                nid++;
                continue;
            }
            if (pretty) spaces(buf, indent + 1);
            nid = nodeToString(json, buf, nid, indent + 1, flags);
            if (nid < node->last) {
                rPutCharToBuf(buf, ',');
            }
            if (pretty) rPutCharToBuf(buf, '\n');
        }
        if (pretty) spaces(buf, indent);
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, ']');
        }

    } else if (node->type == JSON_OBJECT) {
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, '{');
        }
        if (pretty) rPutCharToBuf(buf, '\n');
        for (++nid; nid < node->last; ) {
            if (json->nodes[nid].type == 0) {
                nid++;
                continue;
            }
            if (pretty) spaces(buf, indent + 1);
            jsonToBuf(buf, json->nodes[nid].name, flags | JSON_KEY);
            rPutCharToBuf(buf, ':');
            if (pretty) rPutCharToBuf(buf, ' ');
            nid = nodeToString(json, buf, nid, indent + 1, flags);
            if (nid < node->last) {
                rPutCharToBuf(buf, ',');
            }
            if (pretty) rPutCharToBuf(buf, '\n');
        }
        if (pretty) spaces(buf, indent);
        if (!(flags & JSON_BARE)) {
            rPutCharToBuf(buf, '}');
        }
    } else {
        rPutStringToBuf(buf, "undefined");
        nid++;
    }
    return nid;
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
    if ((buf = rAllocBuf(0)) == 0) {
        return 0;
    }
    if (key && *key && (nid = jsonGetId(json, nid, key)) < 0) {
        rFreeBuf(buf);
        return 0;
    }
    if (json->strict || (flags & JSON_STRICT)) {
        flags |= JSON_SINGLE | JSON_QUOTES;
    }
    nodeToString(json, buf, nid, 0, flags);
    if (flags & JSON_PRETTY) {
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
        flags = JSON_PRETTY;
    }
    json->value = jsonToString(json, 0, 0, flags);
    return json->value;
}

/*
    Print a JSON tree for debugging
 */
PUBLIC void jsonPrint(Json *json)
{
    char *str;

    if (!json) return;
    str = jsonToString(json, 0, 0, JSON_PRETTY);
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
    int      at, id, dlen, slen, didc, kind, pflags;

    if (depth > ME_JSON_MAX_RECURSION) {
        return jerror(dest, "Blend recursion limit exceeded");
    }

    //  Cast const away because ITERATE macros make this difficult
    src = (Json*) csrc;
    if (dest->flags & JSON_LOCK) {
        return jerror(dest, "Cannot blend into a locked JSON object");
    }
    if (dest == 0) {
        return R_ERR_BAD_ARGS;
    }
    if (src == 0 || src->count == 0) {
        return 0;
    }
    assert(0 <= did && did <= dest->count);
    assert(0 <= sid && sid <= src->count);

    if (dest->count == 0) {
        allocNode(dest, JSON_OBJECT, 0, 0);
    }
    if (dest == src) {
        srcData = jsonToString(src, sid, 0, 0);
        src = tmpSrc = jsonAlloc(0);
        parseJson(tmpSrc, srcData, flags);
        rFree(srcData);
        sid = 0;
    } else {
        tmpSrc = 0;
    }
    if (dkey && *dkey) {
        if ((id = jquery(dest, did, dkey, 0, 0)) < 0) {
            did = jquery(dest, did, dkey, "", JSON_OBJECT);
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
            if (flags & JSON_COMBINE) {
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
                    insertNodes(dest, at, 1, did);
                    dp = &dest->nodes[did];
                    if (spc->type & (JSON_ARRAY | JSON_OBJECT)) {
                        setNode(dest, at, spc->type, property, 1, 0, 0);
                        if (blendRecurse(dest, at, 0, src, sidc, 0, pflags & ~JSON_CCREATE, depth + 1) < 0) {
                            jsonFree(tmpSrc);
                            return R_ERR_BAD_ARGS;
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
                if (blendRecurse(dest, didc, 0, src, sidc, 0, pflags, depth + 1) < 0) {
                    jsonFree(tmpSrc);
                    return R_ERR_BAD_ARGS;
                }
                dp = &dest->nodes[did];
                if (pflags & JSON_REPLACE && !(sp->type & (JSON_OBJECT | JSON_ARRAY)) && sspace(dpc->value)) {
                    removeNodes(dest, didc, dpc->last - didc);
                    dp = &dest->nodes[did];
                }
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
            slen = sp->last - sid - 1;
            insertNodes(dest, at, slen, did);
            copyNodes(dest, at, src, sid + 1, slen);

        } else {
            // Default is to JSON_OVERWRITE
            slen = sp->last - sid;
            dlen = dp->last - did;
            if (dlen > slen) {
                removeNodes(dest, did + 1, dlen - slen);
            } else if (dlen < slen) {
                insertNodes(dest, did + 1, slen - dlen, did);
            }
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
        assert(sp->type & (JSON_PRIMITIVE | JSON_STRING | JSON_REGEXP));
        if (flags & JSON_APPEND) {
            freeNode(dp);
            dp->value = sjoin(dp->value, " ", sp->value, NULL);
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

    dest = jsonAlloc(flags);
    if (csrc) {
        jsonBlend(dest, 0, 0, csrc, 0, 0, 0);
    }
    return dest;
}
#endif /* JSON_BLEND */

static void spaces(RBuf *buf, int count)
{
    int i;

    assert(buf);
    assert(0 <= count);

    for (i = 0; i < count; i++) {
        rPutStringToBuf(buf, "    ");
    }
}

#if JSON_TRIGGER
PUBLIC void jsonSetTrigger(Json *json, JsonTrigger proc, void *arg)
{
    json->trigger = proc;
    json->triggerArg = arg;
}
#endif

/*
    Expand ${token} references in a path or string.
    Unexpanded tokens are replaced with $^{token}
    Return clone of the string if passed NULL or empty string or json not defined.
    Unterminated tokens are an error and return NULL.
 */
PUBLIC char *jsonTemplate(Json *json, cchar *str, bool keep)
{
    RBuf  *buf;
    cchar *value;
    char  *src, *cp, *start, *tok;

    if (!str || schr(str, '$') == 0 || !json) {
        return sclone(str);
    }
    buf = rAllocBuf(0);
    for (src = (char*) str; *src; src++) {
        if (src[0] == '$' && src[1] == '{') {
            for (cp = start = src + 2; *cp && *cp != '}'; cp++) {}
            if (*cp == '\0') {
                // Unterminated token
                return NULL;
            }
            tok = snclone(start, cp - start);
            value = jsonGet(json, 0, tok, 0);
            if (*tok && value) {
                rPutStringToBuf(buf, value);
            } else if (keep) {
                rPutStringToBuf(buf, "${");
                rPutStringToBuf(buf, tok);
                rPutCharToBuf(buf, '}');
            }
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

static bool isfnumber(cchar *s, ssize len)
{
    cchar *cp;
    int   dots;

    if (!s || !*s) {
        return 0;
    }
    if (schr("+-1234567890", *s) == 0) {
        return 0;
    }
    for (cp = s; cp < s + len; cp++) {
        if (schr("1234567890.+-eE", *cp) == 0) {
            return 0;
        }
    }
    /*
        Some extra checks
     */
    for (cp = s, dots = 0; cp < s + len; cp++) {
        if (*cp == '.') {
            if (dots++ > 0) {
                return 0;
            }
        }
    }
    return 1;
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
