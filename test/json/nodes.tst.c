/*
    nodes.c.tst - Unit tests for JSON node operations

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void jsonNodeTest()
{
    Json        *obj;
    JsonNode    *node, *child;
    int         nodeId;

    obj = parse("{\
        users: [\
            {name: 'Alice', age: 30}, \
            {name: 'Bob', age: 25} \
        ], \
        settings: {\
            theme: 'dark', \
            notifications: true \
        } \
    }");

    // Test jsonGetNode
    node = jsonGetNode(obj, 0, "users");
    ttrue(node != 0);
    ttrue(node->type == JSON_ARRAY);

    node = jsonGetNode(obj, 0, "settings");
    ttrue(node != 0);
    ttrue(node->type == JSON_OBJECT);

    node = jsonGetNode(obj, 0, "users[0]");
    ttrue(node != 0);
    ttrue(node->type == JSON_OBJECT);

    node = jsonGetNode(obj, 0, "users[0].name");
    ttrue(node != 0);
    ttrue(node->type == JSON_STRING);
    tmatch(node->value, "Alice");

    // Test jsonGetNodeId
    nodeId = jsonGetNodeId(obj, node);
    ttrue(nodeId > 0);

    // Verify we can get back to the same node
    JsonNode *sameNode = &obj->nodes[nodeId];
    ttrue(sameNode == node);

    // Test jsonGetChildNode
    nodeId = jsonGetId(obj, 0, "users[0]");
    ttrue(nodeId > 0);

    child = jsonGetChildNode(obj, nodeId, 0); // First child
    ttrue(child != 0);
    tmatch(child->name, "name");

    child = jsonGetChildNode(obj, nodeId, 1); // Second child
    ttrue(child != 0);
    tmatch(child->name, "age");

    child = jsonGetChildNode(obj, nodeId, 2); // Non-existent child
    ttrue(child == 0);

    // Test with array
    nodeId = jsonGetId(obj, 0, "users");
    child = jsonGetChildNode(obj, nodeId, 0); // First array element
    ttrue(child != 0);
    ttrue(child->type == JSON_OBJECT);

    child = jsonGetChildNode(obj, nodeId, 1); // Second array element
    ttrue(child != 0);
    ttrue(child->type == JSON_OBJECT);

    child = jsonGetChildNode(obj, nodeId, 2); // Non-existent element
    ttrue(child == 0);

    jsonFree(obj);
}

static void jsonGetIdTest()
{
    Json    *obj;
    int     id1, id2, id3;

    obj = parse("{\
        level1: {\
            level2: {\
                level3: 'deep value' \
            } \
        }, \
        array: [10, 20, 30] \
    }");

    // Test jsonGetId with nested paths
    id1 = jsonGetId(obj, 0, "level1");
    ttrue(id1 > 0);

    id2 = jsonGetId(obj, 0, "level1.level2");
    ttrue(id2 > 0);
    ttrue(id2 != id1);

    id3 = jsonGetId(obj, 0, "level1.level2.level3");
    ttrue(id3 > 0);
    ttrue(id3 != id2);

    // Test with array indices
    id1 = jsonGetId(obj, 0, "array");
    ttrue(id1 > 0);

    id2 = jsonGetId(obj, 0, "array[0]");
    ttrue(id2 > 0);
    ttrue(id2 != id1);

    id3 = jsonGetId(obj, 0, "array[2]");
    ttrue(id3 > 0);
    ttrue(id3 != id2);

    // Test non-existent paths
    id1 = jsonGetId(obj, 0, "nonexistent");
    ttrue(id1 < 0);

    id1 = jsonGetId(obj, 0, "level1.nonexistent");
    ttrue(id1 < 0);

    id1 = jsonGetId(obj, 0, "array[10]");
    ttrue(id1 < 0);

    jsonFree(obj);
}

static void jsonNodeDirectTest()
{
    Json        *obj;
    JsonNode    *node;

    obj = parse("{test: 'initial'}");

    // Test direct node access and modification
    node = jsonGetNode(obj, 0, "test");
    ttrue(node != 0);
    tmatch(node->value, "initial");

    // Test jsonSetNodeValue
    jsonSetNodeValue(node, "modified", JSON_STRING, 0);
    checkValue(obj, "test", "modified");

    // Test jsonSetNodeType
    jsonSetNodeType(node, JSON_PRIMITIVE);
    ttrue(jsonGetType(obj, 0, "test") == JSON_PRIMITIVE);

    jsonFree(obj);
}

int main(void)
{
    rInit(0, 0);
    jsonNodeTest();
    jsonGetIdTest();
    jsonNodeDirectTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/