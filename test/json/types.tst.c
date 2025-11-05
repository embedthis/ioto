/*
    types.c.tst - Unit tests for JSON types

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/
static void jsonTypes()
{
    Json    *obj;
    char    *s;

    obj = parse("{ value: 'hello' }");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'hello' }");
    checkJson(obj, "{value:'hello'}", 0);
    jsonFree(obj);

    obj = parse("{ value: true }");
    checkJson(obj, "{value:true}", 0);
    jsonFree(obj);

    obj = parse("{ value: false }");
    checkJson(obj, "{value:false}", 0);
    jsonFree(obj);

    obj = parse("{ value: 42 }");
    checkJson(obj, "{value:42}", 0);
    jsonFree(obj);

    obj = parse("{ value: 3.14159 }");
    checkJson(obj, "{value:3.14159}", 0);
    jsonFree(obj);

    obj = parse("{ value: null }");
    checkJson(obj, "{value:null}", 0);
    jsonFree(obj);

    obj = parse("{ value: undefined }");
    checkJson(obj, "{value:undefined}", 0);
    jsonFree(obj);

    obj = parse("{ value: /abc/ }");
    s = jsonToString(obj, 0, 0, 0);
    tmatch(s, "{value:/abc/}");
    jsonFree(obj);
    rFree(s);

    /*
        Special values as strings
    */
    obj = parse("{ value: 'undefined' }");
    checkJson(obj, "{value:'undefined'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'null' }");
    checkJson(obj, "{value:'null'}", 0);
    jsonFree(obj);

    obj = parse("{ value: '1.2' }");
    checkJson(obj, "{value:'1.2'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'true' }");
    checkJson(obj, "{value:'true'}", 0);
    jsonFree(obj);

    obj = parse("{ value: 'false' }");
    checkJson(obj, "{value:'false'}", 0);
    jsonFree(obj);
}


int main(void)
{
    rInit(0, 0);
    jsonTypes();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
