/*
    fileio.c.tst - Unit tests for JSON file I/O operations

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonFileTest()
{
    Json    *obj, *loaded;
    char    *error, *path;
    int     rc, pid;

    pid = getpid();

    // Test jsonParseFile with valid file
    path = sfmt("test_output_%d.json", pid);
    obj = parse("{name: 'John', age: 30, active: true}");
    rc = jsonSave(obj, 0, 0, path, 0644, JSON_JSON);
    ttrue(rc == 0);

    loaded = jsonParseFile(path, &error, 0);
    ttrue(loaded != 0);
    ttrue(error == 0);
    checkValue(loaded, "name", "John");
    checkValue(loaded, "age", "30");
    checkValue(loaded, "active", "true");
    jsonFree(obj);
    jsonFree(loaded);
    rFree(path);

    // Test jsonParseFile with non-existent file
    path = sfmt("non_existent_%d.json", pid);
    loaded = jsonParseFile(path, &error, 0);
    ttrue(loaded == 0);
    ttrue(error != 0);
    rFree(error);
    rFree(path);

    // Test jsonSave with different formats
    obj = parse("{colors: ['red', 'green', 'blue'], settings: {debug: true}}");

    // Save in JSON5 format
    path = sfmt("test_json5_%d.json", pid);
    rc = jsonSave(obj, 0, 0, path, 0644, JSON_JSON5 | JSON_MULTILINE);
    ttrue(rc == 0);
    rFree(path);

    // Save in strict JSON format
    path = sfmt("test_strict_%d.json", pid);
    rc = jsonSave(obj, 0, 0, path, 0644, JSON_JSON);
    ttrue(rc == 0);
    rFree(path);

    // Load and verify JSON5 file
    path = sfmt("test_json5_%d.json", pid);
    loaded = jsonParseFile(path, &error, 0);
    ttrue(loaded != 0);
    ttrue(error == 0);
    checkValue(loaded, "colors[0]", "red");
    checkValue(loaded, "settings.debug", "true");
    jsonFree(loaded);
    rFree(path);

    // Load and verify strict JSON file
    path = sfmt("test_strict_%d.json", pid);
    loaded = jsonParseFile(path, &error, 0);
    ttrue(loaded != 0);
    ttrue(error == 0);
    checkValue(loaded, "colors[1]", "green");
    checkValue(loaded, "settings.debug", "true");
    jsonFree(loaded);
    jsonFree(obj);
    rFree(path);

    // Test saving partial tree
    obj = parse("{user: {name: 'Alice', profile: {age: 25, city: 'NYC'}}}");
    path = sfmt("test_partial_%d.json", pid);
    rc = jsonSave(obj, 0, "user.profile", path, 0644, JSON_JSON);
    ttrue(rc == 0);

    loaded = jsonParseFile(path, &error, 0);
    ttrue(loaded != 0);
    checkValue(loaded, "age", "25");
    checkValue(loaded, "city", "NYC");
    jsonFree(obj);
    jsonFree(loaded);

    // Clean up test files
    remove(path);
    rFree(path);
    path = sfmt("test_output_%d.json", pid);
    remove(path);
    rFree(path);
    path = sfmt("test_json5_%d.json", pid);
    remove(path);
    rFree(path);
    path = sfmt("test_strict_%d.json", pid);
    remove(path);
    rFree(path);
}

int main(void)
{
    rInit(0, 0);
    jsonFileTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/