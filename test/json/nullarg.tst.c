/*
    nullarg.c.tst - Unit tests for null and bad argument tolerance

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void nullLog(cchar *type, cchar *source, cchar *msg)
{
}

static void jsonNullArgTest()
{
    Json        *obj, *clone;
    JsonNode    *node;
    char        *s;

    //  Freeing a null object is tolerated
    jsonFree(0);
    jsonPrint(0);

    ttrue(jsonToString(0, 0, 0, 0) == 0);
    ttrue(jsonString(0, 0) == 0);
    ttrue(jsonGetType(0, 0, 0) < 0);
    ttrue(jsonGetNode(0, 0, 0) == 0);
    ttrue(jsonGetNodeId(0, 0) < 0);
    ttrue(jsonGetChildNode(0, 0, 0) == 0);
    ttrue(jsonGet(0, 0, "name", "default") != 0);
    tmatch(jsonGet(0, 0, "name", "default"), "default");
    ttrue(jsonSet(0, 0, "name", "value", 0) < 0);
    ttrue(jsonRemove(0, 0, "name") < 0);
    ttrue(jsonGetLength(0, 0, 0) < 0);
    clone = jsonClone(0, 0);
    ttrue(clone != 0);
    jsonFree(clone);

    obj = parse("{name: 'John', list: [1, 2]}");

    //  Negative and out of range node IDs
    ttrue(jsonGetNode(obj, -1, "name") == 0);
    ttrue(jsonGetId(obj, -1, "name") < 0);
    ttrue(jsonGetId(obj, obj->count, "name") < 0);
    tmatch(jsonGet(obj, -1, "name", "default"), "default");
    tmatch(jsonGet(obj, obj->count, "name", "default"), "default");
    ttrue(jsonSet(obj, -1, "name", "value", 0) < 0);
    ttrue(jsonRemove(obj, -1, "name") < 0);
    ttrue(jsonGetChildNode(obj, -1, 0) == 0);
    ttrue(jsonGetChildNode(obj, obj->count, 0) == 0);
    ttrue(jsonGetChildNode(obj, 0, -1) == 0);

    //  A node outside the tree has no ID
    ttrue(jsonGetNodeId(obj, &obj->nodes[obj->count]) < 0);
    node = jsonGetNode(obj, 0, "name");
    ttrue(node != 0);
    ttrue(jsonGetNodeId(obj, node) > 0);

    //  Missing keys
    ttrue(jsonGetType(obj, 0, "missing") < 0);
    ttrue(jsonGetNode(obj, 0, "missing") == 0);
    ttrue(jsonGetId(obj, 0, "missing") < 0);
    ttrue(jsonRemove(obj, 0, "missing") < 0);
    ttrue(jsonRemove(obj, 0, "") < 0);
    ttrue(jsonToString(obj, 0, "missing", 0) == 0);

    //  Containers stringify to a stub value
    tmatch(jsonGet(obj, 0, "list", 0), "[]");
    tmatch(jsonGet(obj, 0, 0, 0), "{}");

    //  Templates are tolerant
    s = jsonTemplate(0, "no ${json} object", 0);
    tmatch(s, "no ${json} object");
    rFree(s);
    s = jsonTemplate(obj, 0, 0);
    rFree(s);
    s = jsonTemplate(obj, "no tokens here", 0);
    tmatch(s, "no tokens here");
    rFree(s);

    jsonFree(obj);
}

static void jsonSaveErrorTest()
{
    Json    *obj;
    char    *path;
    int     rc, pid;

    pid = getpid();
    obj = parse("{name: 'John'}");

    ttrue(jsonSave(0, 0, 0, "any.json", 0644, 0) < 0);
    ttrue(jsonSave(obj, 0, 0, 0, 0644, 0) < 0);
    ttrue(jsonSave(obj, 0, 0, "", 0644, 0) < 0);

    //  A key that does not resolve cannot be rendered
    ttrue(jsonSave(obj, 0, "missing", "unused.json", 0644, 0) < 0);

    //  A directory that does not exist cannot be opened
    ttrue(jsonSave(obj, 0, 0, "no-such-dir-for-json-tests/out.json", 0644, 0) < 0);

    //  A zero mode selects the default file mode
    path = sfmt("test_defmode_%d.json", pid);
    rc = jsonSave(obj, 0, 0, path, 0, JSON_JSON);
    ttrue(rc == 0);
    remove(path);
    rFree(path);

    jsonFree(obj);
}

static void jsonParseFileErrorTest()
{
    Json    *obj;
    char    *error, *path;
    int     pid;

    pid = getpid();

    ttrue(jsonParseFile(0, 0, 0) == 0);
    ttrue(jsonParseFile("", 0, 0) == 0);

    //  A missing file reports an error without an allocated tree
    error = 0;
    obj = jsonParseFile("no-such-file-for-json-tests.json", &error, 0);
    ttrue(obj == 0);
    ttrue(error != 0);
    rFree(error);

    //  Errors are optional
    obj = jsonParseFile("no-such-file-for-json-tests.json", 0, 0);
    ttrue(obj == 0);

    //  A parse error in a file names the file
    path = sfmt("test_badfile_%d.json", pid);
    ttrue(rWriteFile(path, "{name: ", 7, 0644) == 7);
    error = 0;
    obj = jsonParseFile(path, &error, 0);
    ttrue(obj == 0);
    ttrue(error != 0);
    ttrue(scontains(error, path) != 0);
    rFree(error);

    //  Errors are optional for file content failures too
    obj = jsonParseFile(path, 0, 0);
    ttrue(obj == 0);
    remove(path);
    rFree(path);
}

static void jsonIterationGuardTest()
{
    Json        *obj;
    JsonNode    *child;
    RLogHandler prior;
    int         count;

    obj = parse("{item: {first: 1, second: 2, third: 3}}");

    //  Permuting the tree while iterating aborts the iteration
    prior = rSetLogHandler(nullLog);
    count = 0;
    for (ITERATE_JSON_KEY(obj, 0, "item", child, nid)) {
        count++;
        jsonSet(obj, 0, "item.fourth", "4", JSON_PRIMITIVE);
    }
    ttrue(count == 1);

    //  And is detectable directly
    ttrue(jsonCheckIteration(obj, obj->count - 1, 1) < 0);
    ttrue(jsonCheckIteration(obj, obj->count, 1) == 1);
    rSetLogHandler(prior);

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonNullArgTest();
    jsonSaveErrorTest();
    jsonParseFileErrorTest();
    jsonIterationGuardTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
