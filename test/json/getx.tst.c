
/*
    getx.c.tst - Unit tests for JSON get tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonGetTest()
{
    Json    *obj;
    char    *text;
    cchar   *s;

    text = "\
{\n\
    colors: ['red', 'white', 'blue'],\n\
    options: { bright: true, loud: false}\n\
}\n";
    obj = parse(text);
    ttrue(obj);
    s = jsonGet(obj, 0, "colors[0]", 0);
    tmatch(s, "red");

    s = jsonGet(obj, 0, "options.bright", 0);
    tmatch(s, "true");
    ttrue(jsonGetBool(obj, 0, "options.bright", 0));

    s = jsonGet(obj, 0, "options['bright']", 0);
    tmatch(s, "true");
    ttrue(jsonGetBool(obj, 0, "options.bright", 0));

    s = jsonGet(obj, 0, "options[\"bright\"]", 0);
    tmatch(s, "true");
    ttrue(jsonGetBool(obj, 0, "options.bright", 0));
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
        colors: ['red', 'white', 'blue'],\n\
        options: { bright: true, loud: false},\n\
        weather: {\n\
            forecast: {\n\
                tomorrow: {\n\
                    temp: 101\n\
                }\n\
            }\n\
        }\n\
    }\n\
}\n";
    obj = parse(text);
    ttrue(obj);
    if (obj) {
        s = jsonGet(obj, 0, "info.updated", 0);
        tmatch(s, "today");

        s = jsonGet(obj, 0, "info.weather.forecast.tomorrow.temp", 0);
        tmatch(s, "101");
        jsonFree(obj);
    }
}

int main(void)
{
    rInit(0, 0);
    jsonGetTest();
    rTerm();
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
