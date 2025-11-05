
/*
    comments.c.tst - Unit tests for JSON comments

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonComments()
{
    Json    *obj;

    obj = parse("/* Comment */");
    ttrue(obj);
    jsonFree(obj);

    obj = jsonParseString("/* Unclosed comment ", NULL, 0);
    ttrue(obj == 0);

    obj = parse("// Rest of line ");
    ttrue(obj);
    jsonFree(obj);

    obj = parse("/* Leading Comment */ {}");
    ttrue(obj);
    jsonFree(obj);

    obj = parse("{}/* Trailing Comment */");
    ttrue(obj != 0);
    jsonFree(obj);

    obj = parse("{/* Inside Comment */}");
    ttrue(obj != 0);
    jsonFree(obj);

    obj = parse("{/* Before name Comment */ color: 'red'}");
    ttrue(obj != 0);
    checkJson(obj, "{color:'red'}", 0);
    jsonFree(obj);

    obj = parse("{color: /* Before value */ 'red'}");
    ttrue(obj != 0);
    checkJson(obj, "{color:'red'}", 0);
    jsonFree(obj);

    obj = parse("{color: 'red' /* After value */}");
    ttrue(obj != 0);
    checkJson(obj, "{color:'red'}", 0);
    jsonFree(obj);
}


int main(void)
{
    rInit(0, 0);
    jsonComments();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
