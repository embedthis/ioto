/*
    getters.c.tst - Unit tests for JSON getter functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonGetCloneTest()
{
    Json    *obj;
    char    *result;

    obj = parse("{name: 'Alice', age: 25, score: 95.5}");

    // Test basic jsonGetClone
    result = jsonGetClone(obj, 0, "name", "default");
    tmatch(result, "Alice");
    rFree(result);

    // Test with default value
    result = jsonGetClone(obj, 0, "missing", "default");
    tmatch(result, "default");
    rFree(result);

    // Test with NULL default
    result = jsonGetClone(obj, 0, "missing", NULL);
    ttrue(result == NULL);
    rFree(result);

    // Test nested path
    jsonSet(obj, 0, "profile.city", "NYC", 0);
    result = jsonGetClone(obj, 0, "profile.city", "unknown");
    tmatch(result, "NYC");
    rFree(result);

    jsonFree(obj);
}

static void jsonTypedGettersTest()
{
    Json    *obj;
    bool    boolVal;
    double  doubleVal;
    int     intVal;
    int64   numVal;
    uint64  uintVal;

    obj = parse("{\
        enabled: true, \
        disabled: false, \
        temperature: 98.6, \
        count: 42, \
        bignum: 9223372036854775807, \
        size: '1024kb' \
    }");

    // Test jsonGetBool
    boolVal = jsonGetBool(obj, 0, "enabled", 0);
    ttrue(boolVal == 1);

    boolVal = jsonGetBool(obj, 0, "disabled", 1);
    ttrue(boolVal == 0);

    boolVal = jsonGetBool(obj, 0, "missing", 1);
    ttrue(boolVal == 1);

    // Test jsonGetDouble
    doubleVal = jsonGetDouble(obj, 0, "temperature", 0.0);
    ttrue(doubleVal == 98.6);

    doubleVal = jsonGetDouble(obj, 0, "count", 0.0);
    ttrue(doubleVal == 42.0);

    doubleVal = jsonGetDouble(obj, 0, "missing", 99.9);
    ttrue(doubleVal == 99.9);

    // Test jsonGetInt
    intVal = jsonGetInt(obj, 0, "count", 0);
    ttrue(intVal == 42);

    intVal = jsonGetInt(obj, 0, "missing", 123);
    ttrue(intVal == 123);

    // Test jsonGetNum
    numVal = jsonGetNum(obj, 0, "bignum", 0);
    ttrue(numVal == 9223372036854775807LL);

    numVal = jsonGetNum(obj, 0, "count", 0);
    ttrue(numVal == 42);

    numVal = jsonGetNum(obj, 0, "missing", 999);
    ttrue(numVal == 999);

    // Test jsonGetValue with units
    uintVal = jsonGetValue(obj, 0, "size", "0");
    ttrue(uintVal == 1048576); // 1024 * 1024

    jsonFree(obj);
}

static void jsonDateTest()
{
    Json    *obj;
    Time    dateVal;
    int     rc;

    obj = parse("{}");

    // Test jsonSetDate and jsonGetDate
    rc = jsonSetDate(obj, 0, "created", 1640995200000LL); // 2022-01-01 00:00:00 UTC
    ttrue(rc > 0);

    dateVal = jsonGetDate(obj, 0, "created", 0);
    ttrue(dateVal == 1640995200000LL);

    // Test with default value
    dateVal = jsonGetDate(obj, 0, "missing", 123456789LL);
    ttrue(dateVal == 123456789LL);

    jsonFree(obj);
}

static void jsonValueUnitsTest()
{
    Json    *obj;
    uint64  val;

    obj = parse("{\
        time1: '5min', \
        time2: '2hours', \
        time3: '1day', \
        size1: '512bytes', \
        size2: '2mb', \
        size3: '1gb', \
        unlimited: 'unlimited' \
    }");

    // Test time units
    val = jsonGetValue(obj, 0, "time1", "0");
    ttrue(val == 300); // 5 * 60 seconds

    val = jsonGetValue(obj, 0, "time2", "0");
    ttrue(val == 7200); // 2 * 60 * 60 seconds

    val = jsonGetValue(obj, 0, "time3", "0");
    ttrue(val == 86400); // 24 * 60 * 60 seconds

    // Test byte units
    val = jsonGetValue(obj, 0, "size1", "0");
    ttrue(val == 512);

    val = jsonGetValue(obj, 0, "size2", "0");
    ttrue(val == 2097152); // 2 * 1024 * 1024

    val = jsonGetValue(obj, 0, "size3", "0");
    ttrue(val == 1073741824); // 1024 * 1024 * 1024

    // Test unlimited
    val = jsonGetValue(obj, 0, "unlimited", "0");
    ttrue(val == INT64_MAX);

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonGetCloneTest();
    jsonTypedGettersTest();
    jsonDateTest();
    jsonValueUnitsTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/