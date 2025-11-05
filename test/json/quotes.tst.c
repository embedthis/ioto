/*
    strings.c.tst - Unit tests for JSON strings

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonQuotes()
{
    Json    *obj;
    char    *text, *s;

    text = "{ \"key\": 42}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_QUOTE_KEYS | JSON_SINGLE_QUOTES);
    tmatch(s, "{'key':42}");
    jsonFree(obj);
    rFree(s);

    text = "{ 'key': 42}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"key\":42}");
    jsonFree(obj);
    rFree(s);

    text = "{ key: \"one \\\"two\"}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"key\":\"one \\\"two\"}");
    jsonFree(obj);
    rFree(s);

    text = "{ 'key \"word': 42}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"key \\\"word\":42}");
    jsonFree(obj);
    rFree(s);

    text = "{ 'key\r': 42}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"key\r\":42}");
    jsonFree(obj);
    rFree(s);

    text = "{ key: 'one\ntwo\n'}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"key\":\"one\ntwo\n\"}");
    jsonFree(obj);
    rFree(s);

    text = "{ name: 'Peter O\\'Tool'}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"name\":\"Peter O'Tool\"}");
    jsonFree(obj);
    rFree(s);

    text = "{ name: 'Peter O\\\"Tool'}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"name\":\"Peter O\\\"Tool\"}");
    jsonFree(obj);
    rFree(s);

    text = "{ name: 'Peter O\"Tool'}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_DOUBLE_QUOTES | JSON_QUOTE_KEYS);
    tmatch(s, "{\"name\":\"Peter O\\\"Tool\"}");
    jsonFree(obj);
    rFree(s);
}

static void jsonSingle()
{
    Json    *obj;
    char        *text, *s;

    text = "{ key: 'one\ntwo'}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"key\":\"one\\ntwo\"}");
    jsonFree(obj);
    rFree(s);

    text = "{ 'key\r': 42}";
    obj = parse(text);
    ttrue(obj != 0);
    s = jsonToString(obj, 0, 0, JSON_JSON);
    tmatch(s, "{\"key\\r\":42}");
    jsonFree(obj);
    rFree(s);
}

int main(void)
{
    rInit(0, 0);
    jsonQuotes();
    jsonSingle();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
