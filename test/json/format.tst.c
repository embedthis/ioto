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

/*
    Vary padding to put closing brackets at every buffer boundary; ASan detects overruns.
 */
static void sweepCompact(int flags, int indent)
{
    Json *json;
    char *pad, *s;
    int  n;

    jsonSetIndent(indent);
    for (n = 0; n < 600; n++) {
        json = parse("{k:'', a:[1], o:{p:[2,3]}, e:'${a}', f:'${o}'}");
        pad = rAlloc((size_t) n + 1);
        memset(pad, 'x', (size_t) n);
        pad[n] = '\0';
        jsonSet(json, 0, "k", pad, JSON_STRING);
        s = jsonToString(json, 0, 0, flags);
        ttrue(s != 0);
        rFree(s);
        rFree(pad);
        jsonFree(json);
    }
    jsonSetIndent(JSON_DEFAULT_INDENT);
}

static void jsonCompactBoundsTest()
{
    sweepCompact(JSON_COMPACT, JSON_DEFAULT_INDENT);
    sweepCompact(JSON_JSON | JSON_COMPACT, JSON_DEFAULT_INDENT);
    sweepCompact(JSON_HUMAN, 0);
    sweepCompact(JSON_HUMAN | JSON_EXPAND, JSON_DEFAULT_INDENT);
}


static void jsonCompactStringsTest()
{
    Json *json, *back;
    char *s;
    int  flags[] = { JSON_HUMAN, JSON_JSON | JSON_COMPACT, JSON_JSON | JSON_MULTILINE | JSON_COMPACT };
    int  i;

    json = parse("{a:{b:'x   y', c:'p\\n\\nq', d:\"it's  \\\"q\\\"  \"}}");
    tnotnull(json);
    for (i = 0; i < (int) (sizeof(flags) / sizeof(flags[0])); i++) {
        s = jsonToString(json, 0, 0, flags[i]);
        back = parse(s);
        tnotnull(back);
        checkValue(back, "a.b", "x   y");
        checkValue(back, "a.c", "p\n\nq");
        checkValue(back, "a.d", "it's  \"q\"  ");
        jsonFree(back);
        rFree(s);
    }
    jsonFree(json);
}

int main(void)
{
    rInit(0, 0);
    jsonPretty();
    jsonCompactBoundsTest();
    jsonCompactStringsTest();
    rTerm();
    return 0;
}


/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
