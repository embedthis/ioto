/*
    blend.c.tst - Unit tests for JSON blend

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void blend(cchar *destText, cchar *srcText, cchar *result, int flags)
{
    Json    *dest, *src;
    char        *s;
    int         rc;

    dest = parse(destText);
    src = parse(srcText);
    rc = jsonBlend(dest, 0, 0, src, 0, 0, flags);
    ttrue(rc == 0);

    s = jsonToString(dest, 0, 0, 0);
    if (!smatch(s, result)) {
        rPrintf("Expected: %s\n", result);
        rPrintf("Actual:   %s\n", s);
    }
    tmatch(s, result);
    jsonFree(dest);
    rFree(s);
    /* Must not free blended src */
}

static void jsonBlendTest()
{
    /*
        Note: default is to append object properies and to treat arrays like primitive types and overwrite them
     */
    blend("{}", 0, "{}", 0);
    blend("{}", "{}", "{}", 0);
    blend("{}", "{enable:true}", "{enable:true}", 0);
    blend("[]", 0, "[]", 0);
    blend("[]", "[]", "[]", 0);
    blend("[1,2]", "[]", "[]", 0);
    blend("[]", "[1,2]", "[1,2]", 0);
    blend("[1,2]", "[3,4]", "[3,4]", 0);
    blend("[1,2]", "[3,4]", "[1,2,3,4]", JSON_APPEND);

    blend("{numbers:[1,2]}", "{numbers:[3,4]}", "{numbers:[3,4]}", 0);

    blend("{}", "{user:{name: 'ralf'}}", "{user:{name:'ralf'}}", 0);
    blend("{}", "{user:{name:'john',age:42}}", "{user:{name:'john',age:42}}", 0);
    blend("{user:{name:'john'}}", "{user:{age:42}}", "{user:{name:'john',age:42}}", 0);

    blend("{enable:true}", "{enable:false}", "{enable:true}", JSON_CCREATE);
    blend("{enable:true}", "{enable:false}", "{enable:false}", 0);
    blend("{user:{}}", "{user:{name:'john'}}", "{user:{name:'john'}}", 0);

    blend("{enable:true}", "{enable:false}", "{enable:false}", JSON_OVERWRITE);
    blend("{enable:true}", "{enable:{color:'blue'}}", "{enable:{color:'blue'}}", JSON_OVERWRITE);

    /*
        Combine prefixes
     */
    blend("{}", "{'numbers':[1,2]}", "{numbers:[1,2]}", JSON_COMBINE);
    blend("{}", "{'-numbers':[1,2]}", "{}", JSON_COMBINE);
    blend("{}", "{'=numbers':[1,2]}", "{numbers:[1,2]}", JSON_COMBINE);
    blend("{}", "{'?numbers':[1,2]}", "{numbers:[1,2]}", JSON_COMBINE);

    blend("{numbers:[1,2]}", "{'+numbers':[3,4]}", "{numbers:[1,2,3,4]}", JSON_COMBINE);
    blend("{numbers:[1,2]}", "{'-numbers':[2,3]}", "{numbers:[1]}", JSON_COMBINE);
    blend("{numbers:[1,2]}", "{'=numbers':[3]}", "{numbers:[3]}", JSON_COMBINE);
    blend("{numbers:[1,2]}", "{'?numbers':[3]}", "{numbers:[1,2]}", JSON_COMBINE);

    blend("{user:{name:'john',age:30}}", "{}", "{user:{name:'john',age:30}}", JSON_COMBINE);
    blend("{user:{name:'john',age:30}}", "{user:{}}", "{user:{name:'john',age:30}}", JSON_COMBINE);
    blend("{user:{name:'john',age:30}}", "{'-user':{}}", "{user:{name:'john',age:30}}", JSON_COMBINE);
    blend("{user:{name:'john',age:30}}", "{'=user':{}}", "{user:{name:'john',age:30}}", JSON_COMBINE);
    blend("{user:{name:'john',age:30}}", "{'?user':{}}", "{user:{name:'john',age:30}}", JSON_COMBINE);

    blend("{user:{name:'john',age:30}}", "{user:{'+name': 'smith'}}", "{user:{name:'john smith',age:30}}", JSON_COMBINE);
    blend("{user:{name:'john',age:30}}", "{user:{'-name': 'john'}}", "{user:{name:'',age:30}}", JSON_COMBINE);

    blend("{user:{}}", "{user:{'?name': 'john'}}", "{user:{name:'john'}}", JSON_COMBINE);
    blend("{}", "{'?user':{'?name': 'john'}}", "{user:{name:'john'}}", JSON_COMBINE);
}

int main(void)
{
    rInit(0, 0);
    jsonBlendTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
