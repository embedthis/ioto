/*
    limits-getters.tst.c - Getter defaults, integer narrowing, large array indexes and jsonSave cleanup

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void defaultsReturnedExactly(void)
{
    Json *json;

    json = jsonParse("{present: 7}", 0);

    //  Defaults wider than a 16 byte text buffer must come back unchanged
    teqll(jsonGetNum(json, 0, "missing", 10000000000000000LL), 10000000000000000LL);
    teqll(jsonGetNum(json, 0, "missing", -9223372036854775807LL), -9223372036854775807LL);
    teqll(jsonGetDate(json, 0, "missing", 4102444800000LL), 4102444800000LL);
    ttrue(jsonGetDouble(json, 0, "missing", 1e20) == 1e20);
    ttrue(jsonGetDouble(json, 0, "missing", 0.000001234) == 0.000001234);
    teqi(jsonGetInt(json, 0, "missing", -5), -5);

    //  Present values are unaffected
    teqll(jsonGetNum(json, 0, "present", 1), 7);
    teqi(jsonGetInt(json, 0, "present", 1), 7);
    ttrue(jsonGetDouble(json, 0, "present", 1) == 7.0);
    jsonFree(json);
}

static void intSaturates(void)
{
    Json *json;

    json = jsonParse("{big: 5000000000, small: -5000000000, edge: 2147483647}", 0);
    teqi(jsonGetInt(json, 0, "big", 0), MAXINT);
    teqi(jsonGetInt(json, 0, "small", 0), -MAXINT - 1);
    teqi(jsonGetInt(json, 0, "edge", 0), MAXINT);
    jsonFree(json);
}

static void largeArrayIndex(void)
{
    Json  *json;
    cchar *value;

    json = jsonParse("{a: ['zero', 'one', 'two']}", 0);

    //  2^32 + 1 must not alias index 1 on any target width
    ttrue(jsonGet(json, 0, "a[4294967297]", 0) == NULL);
    ttrue(jsonGet(json, 0, "a.4294967297", 0) == NULL);
    ttrue(jsonGet(json, 0, "a[18446744073709551617]", 0) == NULL);
    ttrue(jsonSet(json, 0, "a[4294967297]", "x", 0) < 0);

    value = jsonGet(json, 0, "a[1]", 0);
    tmatch(value, "one");
    teqi((int) jsonGetLength(json, 0, "a"), 3);
    jsonFree(json);
}

static void saveFailureRemovesTemp(void)
{
    Json        *json;
    struct stat info;
    char        path[ME_MAX_FNAME], tmp[ME_MAX_FNAME], *text;

    json = jsonParse("{name: 'value'}", 0);


    sfmtbuf(path, sizeof(path), ".test/save-dir-%d", getpid());
    sfmtbuf(tmp, sizeof(tmp), "%s.tmp", path);
    mkdir(".test", 0755);
    ttrue(mkdir(path, 0755) == 0);
    ttrue(jsonSave(json, 0, 0, path, 0600, 0) < 0);
    ttrue(rAccessFile(tmp, F_OK) < 0);
    ttrue(stat(path, &info) == 0 && S_ISDIR(info.st_mode));
    rmdir(path);

    //  A successful save replaces the file and leaves no temp file
    sfmtbuf(path, sizeof(path), ".test/save-file-%d.json", getpid());
    sfmtbuf(tmp, sizeof(tmp), "%s.tmp", path);
    ttrue(rWriteFile(path, "old", 3, 0600) == 3);
    teqi(jsonSave(json, 0, 0, path, 0600, 0), 0);
    ttrue(rAccessFile(tmp, F_OK) < 0);
    text = rReadFile(path, NULL);
    tcontains(text, "value");
    rFree(text);
    unlink(path);
    jsonFree(json);
}

int main(void)
{
    rInit(0, 0);
    defaultsReturnedExactly();
    intSaturates();
    largeArrayIndex();
    saveFailureRemovesTemp();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/
