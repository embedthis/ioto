/*
    selectors.c.tst - Unit tests for JSON property selectors (.[])

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonSelectors()
{
    Json    *obj;
    cchar   *str;
    char    *s, *text;
    int     rc;

    //  Object get selectors . [] and '*' propery names
    text = "{john: { age: 42}, peter: {age: 23}}";
    obj = parse(text);
    str = jsonGet(obj, 0, "john.age", 0);
    tmatch(str, "42");
    str = jsonGet(obj, 0, "john[age]", 0);
    tmatch(str, "42");
    jsonFree(obj);

    //  Array get selectors
    text = "{users: [{name: 'john', age:42}, {name: 'peter', age:23}]}";
    obj = parse(text);
    str = jsonGet(obj, 0, "users[0].age", 0);
    tmatch(str, "42");
    str = jsonGet(obj, 0, "users.0.age", 0);
    tmatch(str, "42");
    jsonFree(obj);

    //  Set
    text = "{john: { age: 42}, peter: {age: 23}}";
    obj = parse(text);
    rc = jsonSet(obj, 0, "john.age", "43", JSON_PRIMITIVE);
    ttrue(rc >= 0);
    str = jsonGet(obj, 0, "john.age", 0);
    tmatch(str, "43");

    rc = jsonSet(obj, 0, "john[age]", "44", JSON_PRIMITIVE);
    str = jsonGet(obj, 0, "john.age", 0);
    tmatch(str, "44");

    rc = jsonSet(obj, 0, "john['age']", "45", JSON_PRIMITIVE);
    str = jsonGet(obj, 0, "john.age", 0);
    tmatch(str, "45");

    rc = jsonSet(obj, 0, "john[\"age\"]", "46", JSON_PRIMITIVE);
    str = jsonGet(obj, 0, "john.age", 0);
    tmatch(str, "46");

    //  Literal "*" as a propery name
    rc = jsonSet(obj, 0, "john.*", "45", 0);
    str = jsonGet(obj, 0, "john.*", 0);
    tmatch(str, "45");

    rc = jsonSet(obj, 0, "john[*]", "46", JSON_PRIMITIVE);
    str = jsonGet(obj, 0, "john.*", 0);
    tmatch(str, "46");
    jsonFree(obj);

    //  Append new element
    text = "[ 1, 2, 3 ]";
    obj = parse(text);
    s = jsonToString(obj, 0, 0, 0);
    rFree(s);

    rc = jsonSet(obj, 0, "[$]", "4", JSON_PRIMITIVE);
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "[1,2,3,4]");
    jsonFree(obj);
    rFree(s);

    //  Set with wildcards
    text = "{users: [{name: 'john', age:42}, {name: 'peter', age:23}]}";
    obj = parse(text);
    rc = jsonSet(obj, 0, "users[0].age", "43", JSON_PRIMITIVE);
    str = jsonGet(obj, 0, "users[0].age", 0);
    tmatch(str, "43");
    str = jsonGet(obj, 0, "users[1].age", 0);
    tmatch(str, "23");
    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonSelectors();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */