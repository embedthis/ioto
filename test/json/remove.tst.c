/*
    remove.c.tst - Unit tests for JSON remove primitives

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonRemoveTest()
{
    Json    *obj;
    char    *s, *text;
    int     rc;

    obj = parse("{}");
    rc = jsonSet(obj, 0, "number", "42", JSON_PRIMITIVE);
    ttrue(rc > 0);
    tmatch(jsonGet(obj, 0, "number", 0), "42");
    rc = jsonRemove(obj, 0, "number");
    ttrue(rc == 0);
    ttrue(jsonGet(obj, 0, "number", 0) == 0);
    jsonFree(obj);

    obj = parse("{}");
    rc = jsonRemove(obj, 0, "not-here");
    ttrue(rc < 0);
    jsonFree(obj);

    text = "\
{\n\
    info: {\n\
        users: [\n\
            {\n\
                name: 'mary',\n\
                rank: 1,\n\
            },\n\
            {\n\
                name: 'john',\n\
                rank: 2,\n\
            },\n\
        ],\n\
        updated: 'today',\n\
        colors: ['red', 'white', 'blue', 'yellow']\n\
    }\n\
}\n";
    obj = parse(text);
    rc = jsonRemove(obj, 0, "info.users[1]");
    ttrue(rc == 0);
    s = jsonToString(obj, 0, 0, 0);
    ttrue(!scontains(s, "john"));
    jsonFree(obj);
    rFree(s);
}

int main(void)
{
    rInit(0, 0);
    jsonRemoveTest();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
