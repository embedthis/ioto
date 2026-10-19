/*
    rbtree.c.tst - Stress tests for the red/black tree rebalancing and edge cases

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"
#include    "rb.h"

/*********************************** Locals ***********************************/

#define NODES 512

typedef struct Key {
    char *key;
} Key;

/************************************ Code ************************************/

static int compareKeys(cvoid *n1, cvoid *n2, cvoid *ctx)
{
    return scmp(((Key*) n1)->key, ((Key*) n2)->key);
}

static void freeKey(void *arg, void *data)
{
    Key *kp;

    if (!data) return;
    kp = data;
    rFree(kp->key);
    rFree(kp);
}

static Key *allocKey(int value)
{
    Key  *kp;
    char buf[32];

    kp = rAllocType(Key);
    kp->key = sclone(sfmtbuf(buf, sizeof(buf), "key-%05d", value));
    return kp;
}

/*
    Deterministic shuffle so failures are reproducible
 */
static void shuffle(int *values, int count, uint32 seed)
{
    int    i, j, tmp;
    uint32 state;

    state = seed;
    for (i = count - 1; i > 0; i--) {
        state = state * 1103515245 + 12345;
        j = (int) ((state >> 16) % (uint32) (i + 1));
        tmp = values[i];
        values[i] = values[j];
        values[j] = tmp;
    }
}

/*
    Verify the tree yields every key in ascending order and holds the expected count
 */
static void checkOrdered(RbTree *rb, int expected)
{
    RbNode *node;
    Key    *kp;
    char   *prior;
    int    count, ordered;

    prior = NULL;
    count = 0;
    ordered = 1;
    for (ITERATE_TREE(rb, node)) {
        kp = node->data;
        if (prior && scmp(prior, kp->key) > 0) {
            ordered = 0;
        }
        prior = kp->key;
        count++;
    }
    ttrue(ordered);
    teqi(count, expected);
}

/*
    Insert and remove a large number of nodes so every rebalancing case is exercised
 */
static void testStress(uint32 insertSeed, uint32 removeSeed)
{
    RbTree *rb;
    RbNode *node;
    Key    *kp, *removed, probe;
    char   buf[32];
    int    inserts[NODES], removals[NODES];
    int    i;

    rb = rbAlloc(0, compareKeys, freeKey, 0);
    tnotnull(rb);

    for (i = 0; i < NODES; i++) {
        inserts[i] = i;
        removals[i] = i;
    }
    shuffle(inserts, NODES, insertSeed);
    shuffle(removals, NODES, removeSeed);

    for (i = 0; i < NODES; i++) {
        node = rbInsert(rb, allocKey(inserts[i]));
        tnotnull(node);
    }
    checkOrdered(rb, NODES);

    //  The minimum is maintained as nodes are inserted
    node = rbFirst(rb);
    tnotnull(node);
    kp = node->data;
    tmatch(kp->key, "key-00000");

    for (i = 0; i < NODES; i++) {
        probe.key = sfmtbuf(buf, sizeof(buf), "key-%05d", removals[i]);
        node = rbLookup(rb, &probe, NULL);
        tnotnull(node);
        removed = rbRemove(rb, node, 0);
        tnull(removed);
        tnull(rbLookup(rb, &probe, NULL));
    }
    checkOrdered(rb, 0);
    tnull(rbFirst(rb));

    rbFree(rb);
}

/*
    Removing in ascending and descending order drives the mirrored rebalancing paths
 */
static void testOrderedRemoval(int ascending)
{
    RbTree *rb;
    RbNode *node;
    Key    *removed, probe;
    char   buf[32];
    int    inserts[NODES];
    int    i, value;

    rb = rbAlloc(0, compareKeys, freeKey, 0);
    tnotnull(rb);

    for (i = 0; i < NODES; i++) {
        inserts[i] = i;
    }
    shuffle(inserts, NODES, 0x5eed1234);
    for (i = 0; i < NODES; i++) {
        node = rbInsert(rb, allocKey(inserts[i]));
        tnotnull(node);
    }
    for (i = 0; i < NODES; i++) {
        value = ascending ? i : NODES - 1 - i;
        probe.key = sfmtbuf(buf, sizeof(buf), "key-%05d", value);
        node = rbLookup(rb, &probe, NULL);
        tnotnull(node);
        removed = rbRemove(rb, node, 0);
        tnull(removed);
        if ((i % 64) == 0) {
            checkOrdered(rb, NODES - i - 1);
        }
    }
    rbFree(rb);
}

/*
    Inserting an existing key without RB_DUP replaces the stored data
 */
static void testReplace()
{
    RbTree *rb;
    RbNode *first, *second;
    Key    *kp;

    rb = rbAlloc(0, compareKeys, freeKey, 0);
    tnotnull(rb);

    first = rbInsert(rb, allocKey(1));
    tnotnull(first);

    second = rbInsert(rb, allocKey(1));
    tnotnull(second);

    //  The same node is reused and the old data has been freed
    teqp(first, second);
    kp = second->data;
    tmatch(kp->key, "key-00001");
    checkOrdered(rb, 1);

    rbFree(rb);
}

/*
    With RB_DUP every insert of an equal key adds a node and lookup walks them all
 */
static void testDuplicates()
{
    RbTree *rb;
    RbNode *node;
    Key    probe;
    int    count, i;

    rb = rbAlloc(RB_DUP, compareKeys, freeKey, 0);
    tnotnull(rb);

    for (i = 0; i < 8; i++) {
        node = rbInsert(rb, allocKey(3));
        tnotnull(node);
    }
    node = rbInsert(rb, allocKey(1));
    tnotnull(node);
    node = rbInsert(rb, allocKey(9));
    tnotnull(node);
    checkOrdered(rb, 10);

    probe.key = "key-00003";
    count = 0;
    for (ITERATE_INDEX(rb, node, &probe, NULL)) {
        count++;
    }
    teqi(count, 8);

    //  rbLookupNext stops at the first node with a different key
    node = rbLookupFirst(rb, &probe, NULL);
    tnotnull(node);
    for (i = 0; i < 7; i++) {
        node = rbLookupNext(rb, node, &probe, NULL);
        tnotnull(node);
    }
    node = rbLookupNext(rb, node, &probe, NULL);
    tnull(node);

    rbFree(rb);
}

/*
    A node pointer held across a removal of a different node stays valid.
    Callers step through the tree with a pre-computed successor while removing, so a removal
    must only ever invalidate the node that was passed to it.
 */
static void testRemoveWhileWalking()
{
    RbTree *rb;
    RbNode *node, *next;
    Key    *kp, *removed;
    int    inserts[NODES];
    int    count, i;

    rb = rbAlloc(0, compareKeys, freeKey, 0);
    tnotnull(rb);

    for (i = 0; i < NODES; i++) {
        inserts[i] = i;
    }
    shuffle(inserts, NODES, 0xfeed4321);
    for (i = 0; i < NODES; i++) {
        node = rbInsert(rb, allocKey(inserts[i]));
        tnotnull(node);
    }

    //  Walk the whole tree removing every second node, holding the successor across the removal
    count = 0;
    for (node = rbFirst(rb); node; node = next) {
        next = rbNext(rb, node);
        if ((count++ % 2) == 0) {
            removed = rbRemove(rb, node, 0);
            tnull(removed);
        }
    }
    teqi(count, NODES);
    checkOrdered(rb, NODES / 2);

    //  The surviving nodes are the odd numbered keys
    node = rbFirst(rb);
    tnotnull(node);
    kp = node->data;
    tmatch(kp->key, "key-00001");

    rbFree(rb);
}

/*
    The data may be retained by the caller instead of being freed by the tree
 */
static void testKeepData()
{
    RbTree *rb;
    RbNode *node;
    Key    *kp, probe;

    rb = rbAlloc(0, compareKeys, freeKey, 0);
    tnotnull(rb);
    node = rbInsert(rb, allocKey(5));
    tnotnull(node);

    probe.key = "key-00005";
    node = rbLookup(rb, &probe, NULL);
    tnotnull(node);

    kp = rbRemove(rb, node, 1);
    tnotnull(kp);
    tmatch(kp->key, "key-00005");
    freeKey(NULL, kp);

    rbFree(rb);
}

/*
    Bad and empty arguments are tolerated
 */
static void testEdgeCases()
{
    RbTree *rb;
    RbNode *node;
    Key    probe;

    rb = rbAlloc(0, NULL, NULL, 0);
    tnull(rb);

    rb = rbAlloc(0, compareKeys, freeKey, 0);
    tnotnull(rb);

    probe.key = "key-00001";
    tnull(rbLookup(NULL, &probe, NULL));
    tnull(rbLookup(rb, NULL, NULL));
    tnull(rbLookupFirst(NULL, &probe, NULL));
    tnull(rbLookupFirst(rb, NULL, NULL));
    tnull(rbFirst(NULL));
    tnull(rbNext(NULL, NULL));
    tnull(rbNext(rb, NULL));

    //  An empty tree has no first node and no matches
    tnull(rbFirst(rb));
    tnull(rbLookup(rb, &probe, NULL));
    tnull(rbLookupFirst(rb, &probe, NULL));

    //  A single node tree steps to nothing
    node = rbInsert(rb, allocKey(1));
    tnotnull(node);
    node = rbFirst(rb);
    tnotnull(node);
    tnull(rbNext(rb, node));

    rbFree(rb);
    rbFree(NULL);
}

static void printKey(void *data)
{
    Key *kp;

    kp = data;
    rPrintf("%s", kp ? kp->key : "null");
}

/*
    The tree printer walks both subtrees
 */
static void testPrint()
{
    RbTree *rb;
    RbNode *node;
    int    i;

    rb = rbAlloc(0, compareKeys, freeKey, 0);
    tnotnull(rb);
    for (i = 0; i < 7; i++) {
        node = rbInsert(rb, allocKey(i));
        tnotnull(node);
    }
    rbPrint(rb, printKey);
    rbFree(rb);
}

int main(void)
{
    rInit(0, 0);
    rSetLog("stdout:all,!debug,!trace:all,!mbedtls", 0, 1);

    testStress(0x12345678, 0x87654321);
    testStress(0xdeadbeef, 0x0badf00d);
    testOrderedRemoval(1);
    testOrderedRemoval(0);
    testReplace();
    testDuplicates();
    testRemoveWhileWalking();
    testKeepData();
    testEdgeCases();
    testPrint();

    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
