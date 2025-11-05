/*
    set.c.tst - Unit tests for JSON set primitives

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonSetTest()
{
    Json    *obj;
    char    *s;
    int     aid, rc;

    obj = parse("{}");
    rc = jsonSet(obj, 0, "color", "red", 0);
    ttrue(rc > 0);
    tmatch(jsonGet(obj, 0, "color", 0), "red");
    jsonFree(obj);

    obj = parse("{}");
    rc = jsonSet(obj, 0, "number", "42", JSON_PRIMITIVE);
    ttrue(rc > 0);
    tmatch(jsonGet(obj, 0, "number", 0), "42");

    /* Test update of existing value */
    rc = jsonSet(obj, 0, "number", "43", 0);
    ttrue(rc > 0);
    tmatch(jsonGet(obj, 0, "number", 0), "43");
    jsonFree(obj);

    obj = parse("{ user: { name: 'john' }}");
    rc = jsonSet(obj, 0, "user.rank", "42", 0);
    ttrue(rc > 0);
    tmatch(jsonGet(obj, 0, "user.rank", 0), "42");
    tmatch(jsonGet(obj, 0, "user.name", 0), "john");
    jsonFree(obj);

    /*
        Set JSON
     */
    obj = parse("{}");
    rc = jsonSetJsonFmt(obj, 0, "item", "{prop: %d}", 42);
    ttrue(rc == 0);
    ttrue(jsonGetNum(obj, 0, "item.prop", 0) == 42);
    jsonFree(obj);

    /*
        Create an array
    */
    obj = parse("{}");
    aid = jsonSet(obj, 0, "list[$]", NULL, JSON_OBJECT);
    rc = jsonSet(obj, aid, "name", "fred", 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{list:[{name:'fred'}]}");
    jsonFree(obj);
    rFree(s);

    /*
        Set an array when a property already exists of the wrong type
     */
    obj = parse("{list: 'not an array'}");
    aid = jsonSet(obj, 0, "list[$]", NULL, 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{list:[]}");
    jsonFree(obj);
    rFree(s);

    /*
        Set an object when a property already exists of the wrong type
     */
    obj = parse("{list: 'not an object'}");
    aid = jsonSet(obj, 0, "list", NULL, JSON_OBJECT);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{list:{}}");
    jsonFree(obj);
    rFree(s);

    /*
        Add element to array
     */
    obj = parse("{}");
    rc = jsonSet(obj, 0, "abc[$]", "fred", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{abc:['fred']}");
    rFree(s);
    rc = jsonSet(obj, 0, "abc[$]", "joe", 0);
    ttrue(rc > 0);

    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{abc:['fred','joe']}");
    jsonFree(obj);
    rFree(s);

    obj = parse("{users: []}");
    rc = jsonSet(obj, 0, "users[$]", "fred", 0);
    ttrue(rc > 0);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(jsonGet(obj, 0, "users[0]", 0), "fred");
    ttrue(jsonGet(obj, 0, "users[1]", 0) == 0);
    ttrue(jsonGet(obj, 0, "users[-1]", 0) == 0);
    jsonFree(obj);
    rFree(s);
}

int main(void)
{
    rInit(0, 0);
    jsonSetTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
