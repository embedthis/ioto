/*
    rb-test.tst.c - Unit tests for the RB class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

typedef struct Item {
    cchar *key;                 /* Indexed name of the item. Used as the sort key */
    cchar *value;               /* Text value of the item (JSON string) */
} Item;

static int compareItems(cvoid *n1, cvoid *n2, cvoid *ctx)
{
    Item *d1, *d2;

    assert(n1);
    assert(n2);

    d1 = (Item*) n1;
    d2 = (Item*) n2;

    if (d1->key == NULL) {
        if (d2->key == NULL) {
            return 0;
        } else {
            return -1;
        }
    } else if (d2->key == NULL) {
        return 1;
    }
    return strcmp(d1->key, d2->key);
}

static void freeItem(void *arg, void *item)
{
    if (!item) return;
}

static Item *allocItem(cchar *key, cchar *value)
{
    Item *item;

    item = rAllocType(Item);
    item->key = key;
    item->value = value;
    return item;
}

static RbNode *insertItem(RbTree *rb, cchar *key, cchar *value)
{
    return rbInsert(rb, allocItem(key, value));
}

static void rbAllocTest()
{
    RbTree *rb;

    rb = rbAlloc(0, compareItems, freeItem, 0);
    tnotnull(rb);
    rbFree(rb);
}

static void rbCrudTest()
{
    RbTree *rb;
    RbNode *node;
    Item   *item, *data;

    rb = rbAlloc(0, compareItems, freeItem, 0);
    tnotnull(rb);

    item = rAllocType(Item);
    tnotnull(item);
    item->key = "city";
    item->value = "Paris";

    node = rbLookup(rb, item, NULL);
    tfalse(node);

    node = rbInsert(rb, item);
    tnotnull(node);
    ttrue(node->data == item);

    node = rbLookup(rb, item, NULL);
    tnotnull(node);
    ttrue(node->data == item);

    data = rbRemove(rb, node, 1);
    tnotnull(data);
    ttrue(data == item);

    rFree(item);
    rbFree(rb);
}

#if KEEP
static void printItem(void *data)
{
    Item *item = data;

    print("Item %s = %s", item->key, item->value);
}
#endif

static void rbWalkTest()
{
    RbTree *rb;
    RbNode *node;
    Item   *key, *item;
    int    count, i;
    char   *items[4][2] = {
        { "Paris", "48.8" },
        { "London", "51.5" },
        { "Singapore", "1.35" },
        { "Brisbane", "-27.4" },
    };

    rb = rbAlloc(RB_DUP, compareItems, freeItem, 0);
    tnotnull(rb);

    for (i = 0; i < 4; i++) {
        node = insertItem(rb, items[i][0], items[i][1]);
        tnotnull(node);
        item = node->data;
        tmatch(item->key, items[i][0]);
        tmatch(item->value, items[i][1]);
    }
    // rbPrint(rb, printItem);

    /*
        Walk the tree
     */
    count = 0;
    for (ITERATE_TREE(rb, node)) {
        item = node->data;
        count++;
    }
    teqi(count, 4);


    //  Search for an item
    count = 0;
    key = allocItem("Singapore", NULL);
    for (ITERATE_INDEX(rb, node, key, NULL)) {
        count++;
        item = node->data;
        tmatch(item->key, "Singapore");
        tmatch(item->value, "1.35");
    }
    teqi(count, 1);
    rFree(key);
    rbFree(rb);
}

int main(void)
{
    rInit(0, 0);
    rbAllocTest();
    rbCrudTest();
    rbWalkTest();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
