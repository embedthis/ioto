
/*
    iterate.c.tst - Unit tests for JSON iterate macros

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonIterateNodeTest()
{
    Json     *json;
    JsonNode *child, *parent;

    json = parse("{item: {first: 1, second: 2, third: 3}}"); 
    parent = jsonGetNode(json, 0, "item");
    for (ITERATE_JSON(json, parent, child, nid)) {
        if (nid == 2) {
            tmatch(child->name, "first");
        } else if (nid == 3) {
            tmatch(child->name, "second");
        } else if (nid == 4) {
            tmatch(child->name, "third");
        }
    }
}

static void jsonIterateIdTest()
{
    Json     *json;
    JsonNode *child;
    int      pid;

    json = parse("{item: {first: 1, second: 2, third: 3}}"); 
    pid = jsonGetId(json, 0, "item");
    for (ITERATE_JSON_ID(json, pid, child, nid)) {
        if (nid == 2) {
            tmatch(child->name, "first");
        } else if (nid == 3) {
            tmatch(child->name, "second");
        } else if (nid == 4) {
            tmatch(child->name, "third");
        }
    }
}

static void jsonIterateKeyTest()
{
    Json     *json;
    JsonNode *child;

    json = parse("{item: {first: 1, second: 2, third: 3}}"); 
    for (ITERATE_JSON_KEY(json, 0, "item", child, nid)) {
        if (nid == 2) {
            tmatch(child->name, "first");
        } else if (nid == 3) {
            tmatch(child->name, "second");
        } else if (nid == 4) {
            tmatch(child->name, "third");
        }
    }
}

/*
    This test is designed to show debug the iteration checks that the json tree is not mutated during iteration.
static void jsonIterateStabilityTest()
{
    Json     *json;
    JsonNode *child;

    json = parse("{item: {first: 1, second: 2, third: 3}}"); 
    for (ITERATE_JSON_KEY(json, 0, "item", child, nid)) {
        if (nid == 3) {
            tmatch(child->name, "second");
            // Inserting a new node should not affect the iteration
            jsonSetJson(json, 0, "item.first", "{inserted: 999}");
            tmatch(child->name, "inserted");
        }
    }
}
*/

int main(void)
{
    rInit(0, 0);
    jsonIterateNodeTest();
    jsonIterateIdTest();
    jsonIterateKeyTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
