/*
    memory.c.tst - Unit tests for JSON memory management and locking

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonMemoryTest()
{
    Json    *obj, *clone;
    int     rc;

    // Test jsonAlloc and basic operations
    obj = jsonAlloc();
    ttrue(obj != 0);
    ttrue(obj->nodes);
    ttrue(obj->count == 0);

    // Should be able to use allocated object for parsing
    jsonFree(obj);

    // Test jsonClone
    obj = parse("{name: 'test', values: [1, 2, 3], nested: {a: true}}");
    clone = jsonClone(obj, 0);
    ttrue(clone != 0);

    // Verify clone is independent
    checkValue(clone, "name", "test");
    checkValue(clone, "values[1]", "2");
    checkValue(clone, "nested.a", "true");

    // Modify original, clone should be unchanged
    rc = jsonSet(obj, 0, "name", "modified", 0);
    ttrue(rc > 0);
    checkValue(obj, "name", "modified");
    checkValue(clone, "name", "test");

    jsonFree(obj);
    jsonFree(clone);
}

static void jsonLockTest()
{
    Json    *obj;
    cchar   *ref1, *ref2;
    int     rc;

    obj = parse("{name: 'John', age: 30}");

    // Test references before locking
    ref1 = jsonGet(obj, 0, "name", 0);
    tmatch(ref1, "John");

    // Lock the object
    jsonLock(obj);

    // References should remain stable after locking
    ref2 = jsonGet(obj, 0, "name", 0);
    ttrue(ref1 == ref2); // Same pointer

    // Modifications should be blocked while locked
    rc = jsonSet(obj, 0, "name", "Jane", 0);
    ttrue(rc < 0); // Should fail
    checkValue(obj, "name", "John"); // Unchanged

    // Unlock and try modification again
    jsonUnlock(obj);
    rc = jsonSet(obj, 0, "name", "Jane", 0);
    ttrue(rc > 0); // Should succeed
    checkValue(obj, "name", "Jane");

    jsonFree(obj);
}

static void jsonUserFlagsTest()
{
    Json    *obj;
    int     flags;

    obj = parse("{}");

    // Test initial user flags
    flags = jsonGetUserFlags(obj);
    ttrue(flags == 0);

    // Set user flags
    jsonSetUserFlags(obj, 0x55);
    flags = jsonGetUserFlags(obj);
    ttrue(flags == 0x55);

    // Update user flags
    jsonSetUserFlags(obj, 0xAA);
    flags = jsonGetUserFlags(obj);
    ttrue(flags == 0xAA);

    // Test flag boundaries (8-bit field)
    jsonSetUserFlags(obj, 0xFF);
    flags = jsonGetUserFlags(obj);
    ttrue(flags == 0xFF);

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonMemoryTest();
    jsonLockTest();
    jsonUserFlagsTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/