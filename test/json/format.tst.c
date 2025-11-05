/*
    format.c.tst - Unit tests for JSON formatting

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonPretty()
{
    Json    *obj;
    char    *text, *s;

    text = "\
{\n\
    shape: {\n\
        color: 'red',\n\
        width: 42,\n\
        visible: true,\n\
        extends: null,\n\
        shading: [1, 7, 14, 23],\n\
    }\n\
}\n";
    obj = parse(text);
    s = jsonToString(obj, 0, 0, JSON_JSON5 | JSON_MULTILINE);
    ttrue(scontains(s, "shape: {") != 0);
    ttrue(scontains(s, "    color: 'red'") != 0);
    ttrue(scontains(s, "    shading: [") != 0);
    ttrue(scontains(s, "    extends: null") != 0);
    ttrue(scontains(s, "    width: 42") != 0);
    jsonFree(obj);
    rFree(s);
}


int main(void)
{
    rInit(0, 0);
    jsonPretty();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
