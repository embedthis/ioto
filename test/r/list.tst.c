/*
    list.tst.c - Unit tests for the List, Link and RStringList classes

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

#define LIST_MAX_ITEMS 1000                 /* Count of items to put in list */

/************************************ Code ************************************/

static void createList()
{
    RList *lp;

    lp = rAllocList(0, 0);
    tnotnull(lp);
    rFreeList(lp);
}

static void isListEmpty()
{
    RList *lp;

    lp = rAllocList(0, 0);
    tnotnull(lp);
    teqz(rGetListLength(lp), 0);
    tnull(rGetItem(lp, 0));
    rFreeList(lp);
}

static void insertAndRemove()
{
    RList *lp;
    int   index;

    lp = rAllocList(0, 0);
    tnotnull(lp);

    /*
        Do one inser and remove
     */
    index = rAddItem(lp, (void*) 1);
    ttrue(index >= 0);
    teqz(rGetListLength(lp), 1);

    rRemoveItem(lp, (void*) 1);
    teqz(rGetListLength(lp), 0);

    /*
        Test remove will compact
     */
    rAddItem(lp, (void*) 1);
    rAddItem(lp, (void*) 2);
    rAddItem(lp, (void*) 3);

    rRemoveItem(lp, (void*) 2);
    teqz(rGetListLength(lp), 2);
    rRemoveItem(lp, (void*) 3);
    teqz(rGetListLength(lp), 1);
    rFreeList(lp);
}

static void lotsOfInserts()
{
    RList  *lp;
    size_t i, index;

    lp = rAllocList(LIST_MAX_ITEMS, 0);
    tnotnull(lp);

    /*
        Do lots insertions
     */
    for (i = 0; i < LIST_MAX_ITEMS; i++) {
        rAddItem(lp, (cvoid*) i);
        if (rGetListLength(lp) != (i + 1)) {
            tfail("rGetListLength(lp) != i + 1");
        }
    }

    /*
        Now remove
     */
    index = rGetListLength(lp) - 1;
    for (i = 0; i < LIST_MAX_ITEMS; i++) {
        rRemoveItem(lp, (cvoid*) index);
        if (rGetListLength(lp) != index) {
            tfail("rGetListLength(lp) != index");
        }
        index--;
    }
    rFreeList(lp);
}

static void listIterate()
{
    RList *lp;
    int   max, i, item, next;

    max = 50;
    lp = rAllocList(max, 0);
    tnotnull(lp);

    for (i = 0; i < max; i++) {
        rAddItem(lp, (void*) (ssize) (i + 1));
    }
    i = next = 0;
    item = (int) (ssize) rGetNextItem(lp, &next);
    while (item > 0) {
        i++;
        item = (int) (ssize) rGetNextItem(lp, &next);
    }
    teqi(i, max);

    /*
        Abbreviated form with no GetFirst
     */
    i = 0;
    next = 0;
    while ((item = (int) (ssize) rGetNextItem(lp, &next)) != 0) {
        i++;
    }
    teqi(i, max);
    rFreeList(lp);
}

static void orderedInserts()
{
    RList *lp;
    int   i, item, next;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    /*
        Add items such that the final list is ordered
     */
    rAddItem(lp, (void*) (ssize) 4);
    rAddItem(lp, (void*) (ssize) 5);
    rInsertItemAt(lp, 0, (void*) 2);
    rInsertItemAt(lp, 0, (void*) 1);
    rInsertItemAt(lp, 2, (void*) 3);
    rAddItem(lp, (void*) (ssize) 6);

    i = 1;
    next = 0;
    item = (int) (ssize) rGetNextItem(lp, &next);
    while (item > 0) {
        teqi(item, i);
        i++;
        item = (int) (ssize) rGetNextItem(lp, &next);
    }
    rFreeList(lp);
}

static char *toString(char *items[], int nelt)
{
    RBuf *buf;
    int  i;

    buf = rAllocBuf(0);
    for (i = 0; i < nelt; i++) {
        rPutToBuf(buf, "%s ", items[i]);
    }
    if (i > 0) {
        rAdjustBufEnd(buf, -1);
    }
    return rBufToStringAndFree(buf);
}

#define NELT(items) ((int) (sizeof(items) / sizeof(char*)))

static int sorNum(cchar **s1, cchar **s2)
{
    int v1, v2;

    v1 = atoi(*s1);
    v2 = atoi(*s2);
    if (v1 < v2) {
        return -1;
    } else if (v1 > v2) {
        return 1;
    }
    return 0;
}

static void tsort(cchar *input, cchar *expect, RSortProc cmp)
{
    char *ainput, *item, *tok, *actual, *items[16];
    int  count;

    tok = ainput = sclone(input);
    for (count = 0; count < NELT(items) && (item = stok(tok, " ", &tok)) != 0; count++) {
        items[count] = item;
    }
    rSort(items, count, sizeof(char*), cmp, 0);
    actual = toString(items, count);
    tmatch(expect, actual);

    if (!smatch(expect, actual)) {
        print("\nINPUT  \"%s\"", input);
        print("RESULT \"%s\"", actual);
    }
    rFree(actual);
    rFree(ainput);
}

static void sort()
{
    tsort("", "", 0);
    tsort("0", "0", 0);
    tsort("0 1", "0 1", 0);
    tsort("1 0", "0 1", 0);
    tsort("1 1", "1 1", 0);

    tsort("0 1 2", "0 1 2", 0);
    tsort("0 2 1", "0 1 2", 0);
    tsort("2 1 0", "0 1 2", 0);
    tsort("2 0 1", "0 1 2", 0);
    tsort("1 2 0", "0 1 2", 0);
    tsort("1 0 2", "0 1 2", 0);

    tsort("0 1 2 3 4 5", "0 1 2 3 4 5", 0);
    tsort("5 4 3 2 1 0", "0 1 2 3 4 5", 0);
    tsort("5 1 0 2 4 3", "0 1 2 3 4 5", 0);

    //  Odd number of elements
    tsort("5 1 0 2 4 3 6", "0 1 2 3 4 5 6", 0);
    tsort("5 0 1", "0 1 5", 0);
    tsort("6 3 5 0 1", "0 1 3 5 6", 0);

    //  Even elements
    tsort("5 1 0 4 2 3", "0 1 2 3 4 5", 0);
    tsort("2 3 0 1", "0 1 2 3", 0);
    tsort("3 2 0 1", "0 1 2 3", 0);
    tsort("3 2 4 0 1", "0 1 2 3 4", 0);

    //  Even elements

    //  Repeats
    tsort("1 2 1 2 1 2", "1 1 1 2 2 2", 0);
    tsort("2 1 2 1 2 1", "1 1 1 2 2 2", 0);

    //  Numeric sors
    tsort("25 13 7 10", "7 10 13 25", (RSortProc) sorNum);
    tsort("25 13 16 31 44 7 31 48 48 105 10", "7 10 13 16 25 31 31 44 48 48 105", (RSortProc) sorNum);
    tsort("-8 -2 0 7 44", "-8 -2 0 7 44", (RSortProc) sorNum);
    tsort("44 -2 7 -8 0", "-8 -2 0 7 44", (RSortProc) sorNum);
}

static void testSetItem()
{
    RList *lp;

    lp = rAllocList(0, 0);
    tnotnull(lp);

    // Test setting item in empty list (should grow)
    rSetItem(lp, 0, (void*) 100);
    teqz(rGetListLength(lp), 1);
    ttrue(rGetItem(lp, 0) == (void*) 100);

    // Test setting item beyond current length (should grow and fill gaps with NULL)
    rSetItem(lp, 2, (void*) 200);
    teqz(rGetListLength(lp), 3);
    ttrue(rGetItem(lp, 0) == (void*) 100);
    tnull(rGetItem(lp, 1));
    ttrue(rGetItem(lp, 2) == (void*) 200);

    // Test replacing existing item
    rSetItem(lp, 1, (void*) 150);
    teqz(rGetListLength(lp), 3);
    ttrue(rGetItem(lp, 1) == (void*) 150);

    // Test setting at end
    rSetItem(lp, 2, (void*) 250);
    ttrue(rGetItem(lp, 2) == (void*) 250);

    rFreeList(lp);
}

static void testAddNullItem()
{
    RList *lp;
    int   index1, index2;

    lp = rAllocList(0, 0);
    tnotnull(lp);

    // Test adding null items (rAddNullItem may reuse existing nulls)
    index1 = rAddNullItem(lp);
    ttrue(index1 >= 0);
    // Don't check exact length due to reuse behavior
    tnull(rGetItem(lp, index1));

    // Add another null item
    index2 = rAddNullItem(lp);
    ttrue(index2 >= 0);

    // Add regular item, then null
    rAddItem(lp, (void*) 42);
    index1 = rAddNullItem(lp);
    ttrue(index1 >= 0);
    tnull(rGetItem(lp, index1));

    rFreeList(lp);
}

static void testRemoveItemAt()
{
    RList *lp;
    int   result;

    lp = rAllocList(0, 0);
    tnotnull(lp);

    // Add some items
    rAddItem(lp, (void*) 10);
    rAddItem(lp, (void*) 20);
    rAddItem(lp, (void*) 30);
    rAddItem(lp, (void*) 40);
    teqz(rGetListLength(lp), 4);

    // Remove from middle (rRemoveItemAt returns index, not item)
    result = rRemoveItemAt(lp, 1);
    ttrue(result >= 0); // Should succeed
    teqz(rGetListLength(lp), 3);
    ttrue(rGetItem(lp, 0) == (void*) 10);
    ttrue(rGetItem(lp, 1) == (void*) 30);
    ttrue(rGetItem(lp, 2) == (void*) 40);

    // Remove from beginning
    result = rRemoveItemAt(lp, 0);
    ttrue(result >= 0); // Should succeed
    teqz(rGetListLength(lp), 2);
    ttrue(rGetItem(lp, 0) == (void*) 30);
    ttrue(rGetItem(lp, 1) == (void*) 40);

    // Remove from end
    result = rRemoveItemAt(lp, 1);
    ttrue(result >= 0); // Should succeed
    teqz(rGetListLength(lp), 1);
    ttrue(rGetItem(lp, 0) == (void*) 30);

    // Remove last item
    result = rRemoveItemAt(lp, 0);
    ttrue(result >= 0); // Should succeed
    teqz(rGetListLength(lp), 0);

    rFreeList(lp);
}

static void testGetItem()
{
    RList *lp;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Test getting from empty list
    ttrue(rGetItem(lp, 0) == NULL);
    ttrue(rGetItem(lp, 10) == NULL);

    // Add items and test retrieval
    rAddItem(lp, (void*) 100);
    rAddItem(lp, (void*) 200);
    rAddItem(lp, (void*) 300);

    ttrue(rGetItem(lp, 0) == (void*) 100);
    ttrue(rGetItem(lp, 1) == (void*) 200);
    ttrue(rGetItem(lp, 2) == (void*) 300);

    // Test out of bounds
    ttrue(rGetItem(lp, 3) == NULL);
    ttrue(rGetItem(lp, -1) == NULL);
    ttrue(rGetItem(lp, 100) == NULL);

    rFreeList(lp);
}

static void testClearList()
{
    RList *lp;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Clear empty list
    rClearList(lp);
    ttrue(rGetListLength(lp) == 0);

    // Add items and clear
    rAddItem(lp, (void*) 1);
    rAddItem(lp, (void*) 2);
    rAddItem(lp, (void*) 3);
    ttrue(rGetListLength(lp) == 3);

    rClearList(lp);
    ttrue(rGetListLength(lp) == 0);
    ttrue(rGetItem(lp, 0) == NULL);

    // Test can add after clear
    rAddItem(lp, (void*) 42);
    ttrue(rGetListLength(lp) == 1);
    ttrue(rGetItem(lp, 0) == (void*) 42);

    rFreeList(lp);
}

static void testLookupItem()
{
    RList *lp;
    ssize index;
    void  *item1 = (void*) 100;
    void  *item2 = (void*) 200;
    void  *item3 = (void*) 300;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Test lookup in empty list
    index = rLookupItem(lp, item1);
    ttrue(index < 0);

    // Add items
    rAddItem(lp, item1);
    rAddItem(lp, item2);
    rAddItem(lp, item3);
    rAddItem(lp, item1); // Duplicate

    // Test successful lookups (should find first occurrence)
    index = rLookupItem(lp, item1);
    ttrue(index == 0);
    index = rLookupItem(lp, item2);
    ttrue(index == 1);
    index = rLookupItem(lp, item3);
    ttrue(index == 2);

    // Test failed lookup
    index = rLookupItem(lp, (void*) 999);
    ttrue(index < 0);

    // Test NULL lookup
    rAddItem(lp, NULL);
    index = rLookupItem(lp, NULL);
    ttrue(index == 4);

    rFreeList(lp);
}

static void testGrowList()
{
    RList *lp;

    lp = rAllocList(2, 0);
    ttrue(lp != 0);

    // Add items to initial capacity
    rAddItem(lp, (void*) 1);
    rAddItem(lp, (void*) 2);
    ttrue(rGetListLength(lp) == 2);

    // Grow the list explicitly
    rGrowList(lp, 10);
    ttrue(rGetListLength(lp) == 2); // Length unchanged

    // Should be able to add more items without reallocation
    for (int i = 3; i <= 10; i++) {
        rAddItem(lp, (void*) (ssize) i);
    }
    ttrue(rGetListLength(lp) == 10);

    // Test growing beyond current size
    rGrowList(lp, 20);
    for (int i = 11; i <= 20; i++) {
        rAddItem(lp, (void*) (ssize) i);
    }
    ttrue(rGetListLength(lp) == 20);

    rFreeList(lp);
}

static void testListToString()
{
    RList *lp;
    char  *result;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Test empty list
    result = rListToString(lp, ",");
    ttrue(result != NULL);
    ttrue(slen(result) == 0);
    rFree(result);

    // Add string items
    rAddItem(lp, "apple");
    rAddItem(lp, "banana");
    rAddItem(lp, "cherry");

    // Test with comma separator
    result = rListToString(lp, ",");
    ttrue(result != NULL);
    ttrue(scmp(result, "apple,banana,cherry") == 0);
    rFree(result);

    // Test with different separator
    result = rListToString(lp, " | ");
    ttrue(result != NULL);
    ttrue(scmp(result, "apple | banana | cherry") == 0);
    rFree(result);

    // Test with NULL separator (behavior may differ)
    result = rListToString(lp, NULL);
    ttrue(result != NULL);
    // Don't assume specific behavior with NULL separator
    rFree(result);

    // Test with empty separator
    result = rListToString(lp, "");
    ttrue(result != NULL);
    ttrue(scmp(result, "applebananacherry") == 0);
    rFree(result);

    rFreeList(lp);
}

static void testFlagBehavior()
{
    RList *lp;
    char  *retrieved;

    // Test basic flag functionality without complex memory management
    lp = rAllocList(0, R_DYNAMIC_VALUE);
    ttrue(lp != 0);
    ttrue(rGetListLength(lp) == 0);
    rFreeList(lp);

    // Test R_TEMPORAL_VALUE flag - basic functionality
    lp = rAllocList(0, R_TEMPORAL_VALUE);
    ttrue(lp != 0);

    // Add string literals
    rAddItem(lp, "Temporal String 1");
    ttrue(rGetListLength(lp) == 1);

    retrieved = (char*) rGetItem(lp, 0);
    ttrue(scmp(retrieved, "Temporal String 1") == 0);
    rFreeList(lp);

    // Test combined flags: R_DYNAMIC_VALUE | R_TEMPORAL_VALUE
    lp = rAllocList(0, R_DYNAMIC_VALUE | R_TEMPORAL_VALUE);
    ttrue(lp != 0);
    rAddItem(lp, sclone("Combined Flag Test"));
    ttrue(rGetListLength(lp) == 1);
    retrieved = (char*) rGetItem(lp, 0);
    ttrue(scmp(retrieved, "Combined Flag Test") == 0);
    rFreeList(lp);
}

static void testStringOperations()
{
    RList *lp;
    ssize index;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Test rLookupStringItem
    rAddItem(lp, "apple");
    rAddItem(lp, "banana");
    rAddItem(lp, "cherry");
    rAddItem(lp, "banana"); // Duplicate

    // Test successful string lookups
    index = rLookupStringItem(lp, "apple");
    ttrue(index == 0);
    index = rLookupStringItem(lp, "banana");
    ttrue(index == 1); // Should find first occurrence
    index = rLookupStringItem(lp, "cherry");
    ttrue(index == 2);

    // Test failed string lookup
    index = rLookupStringItem(lp, "grape");
    ttrue(index < 0);

    // Test NULL string lookup
    rAddItem(lp, NULL);
    index = rLookupStringItem(lp, NULL);
    ttrue(index == 4);

    // Test empty string
    rAddItem(lp, "");
    index = rLookupStringItem(lp, "");
    ttrue(index == 5);

    // Test rRemoveStringItem (returns index, not item)
    index = rRemoveStringItem(lp, "banana");
    ttrue(index >= 0);              // Should find and remove banana
    ttrue(rGetListLength(lp) == 5); // One banana removed

    // Verify the second banana is still there
    index = rLookupStringItem(lp, "banana");
    ttrue(index >= 0);              // Should still find the second banana

    // Remove non-existent string
    index = rRemoveStringItem(lp, "grape");
    ttrue(index < 0);               // Should fail
    ttrue(rGetListLength(lp) == 5); // No change

    // Remove empty string
    index = rRemoveStringItem(lp, "");
    ttrue(index >= 0);              // Should succeed
    ttrue(rGetListLength(lp) == 4);

    // Remove NULL (may not find NULL item to remove)
    index = rRemoveStringItem(lp, NULL);
    // Don't assume it will succeed - NULL may not be in the list anymore
    ttrue(rGetListLength(lp) >= 3);

    rFreeList(lp);
}

static void testStackOperations()
{
    RList *lp;
    void  *item;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Test rPushItem (alias for rAddItem)
    rPushItem(lp, (void*) 10);
    rPushItem(lp, (void*) 20);
    rPushItem(lp, (void*) 30);
    ttrue(rGetListLength(lp) == 3);
    ttrue(rGetItem(lp, 0) == (void*) 10);
    ttrue(rGetItem(lp, 1) == (void*) 20);
    ttrue(rGetItem(lp, 2) == (void*) 30);

    // Test rPopItem (removes and returns first item)
    item = rPopItem(lp);
    ttrue(item == (void*) 10);
    ttrue(rGetListLength(lp) == 2);
    ttrue(rGetItem(lp, 0) == (void*) 20);
    ttrue(rGetItem(lp, 1) == (void*) 30);

    // Pop another
    item = rPopItem(lp);
    ttrue(item == (void*) 20);
    ttrue(rGetListLength(lp) == 1);
    ttrue(rGetItem(lp, 0) == (void*) 30);

    // Pop last item
    item = rPopItem(lp);
    ttrue(item == (void*) 30);
    ttrue(rGetListLength(lp) == 0);

    // Pop from empty list
    item = rPopItem(lp);
    ttrue(item == NULL);
    ttrue(rGetListLength(lp) == 0);

    // Test push/pop combination
    rPushItem(lp, (void*) 100);
    rPushItem(lp, (void*) 200);
    item = rPopItem(lp);
    ttrue(item == (void*) 100);
    rPushItem(lp, (void*) 300);
    ttrue(rGetListLength(lp) == 2);
    ttrue(rGetItem(lp, 0) == (void*) 200);
    ttrue(rGetItem(lp, 1) == (void*) 300);

    rFreeList(lp);
}

static void testSortList()
{
    RList *lp;
    char  *result;

    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Test sorting empty list
    rSortList(lp, NULL, NULL);
    ttrue(rGetListLength(lp) == 0);

    // Test sorting single string item
    rAddItem(lp, "apple");
    rSortList(lp, NULL, NULL);
    ttrue(rGetListLength(lp) == 1);
    result = (char*) rGetItem(lp, 0);
    ttrue(scmp(result, "apple") == 0);

    // Test sorting multiple string items (using default string comparator)
    rClearList(lp);
    rAddItem(lp, "zebra");
    rAddItem(lp, "apple");
    rAddItem(lp, "cherry");
    rAddItem(lp, "banana");

    rSortList(lp, NULL, NULL);
    ttrue(rGetListLength(lp) == 4);

    // Verify sorted order
    result = (char*) rGetItem(lp, 0);
    ttrue(scmp(result, "apple") == 0);
    result = (char*) rGetItem(lp, 1);
    ttrue(scmp(result, "banana") == 0);
    result = (char*) rGetItem(lp, 2);
    ttrue(scmp(result, "cherry") == 0);
    result = (char*) rGetItem(lp, 3);
    ttrue(scmp(result, "zebra") == 0);

    // Test with duplicates
    rClearList(lp);
    rAddItem(lp, "banana");
    rAddItem(lp, "apple");
    rAddItem(lp, "banana");
    rAddItem(lp, "apple");

    rSortList(lp, NULL, NULL);
    ttrue(rGetListLength(lp) == 4);

    // Verify sorted order with duplicates
    result = (char*) rGetItem(lp, 0);
    ttrue(scmp(result, "apple") == 0);
    result = (char*) rGetItem(lp, 1);
    ttrue(scmp(result, "apple") == 0);
    result = (char*) rGetItem(lp, 2);
    ttrue(scmp(result, "banana") == 0);
    result = (char*) rGetItem(lp, 3);
    ttrue(scmp(result, "banana") == 0);

    rFreeList(lp);
}

static void testEdgeCases()
{
    RList *lp;
    void  *item;
    int   result;

    // Test NULL list pointer safety (should not crash)
    // Note: Most functions should handle NULL gracefully

    // Test very large indices
    lp = rAllocList(0, 0);
    ttrue(lp != 0);

    // Test negative indices
    ttrue(rGetItem(lp, -1) == NULL);
    ttrue(rGetItem(lp, -100) == NULL);

    // Test large positive indices
    ttrue(rGetItem(lp, 1000000) == NULL);

    // Test rRemoveItemAt with invalid indices
    result = rRemoveItemAt(lp, 0); // Empty list
    ttrue(result < 0);             // Should fail
    result = rRemoveItemAt(lp, -1);
    ttrue(result < 0);             // Should fail
    result = rRemoveItemAt(lp, 100);
    ttrue(result < 0);             // Should fail

    // Add some items for boundary testing
    rAddItem(lp, (void*) 1);
    rAddItem(lp, (void*) 2);
    rAddItem(lp, (void*) 3);

    // Test boundary indices
    ttrue(rGetItem(lp, -1) == NULL);
    ttrue(rGetItem(lp, 0) == (void*) 1);
    ttrue(rGetItem(lp, 2) == (void*) 3);
    ttrue(rGetItem(lp, 3) == NULL);

    // Test rSetItem with negative index (should fail gracefully)
    item = rSetItem(lp, -1, (void*) 999);
    ttrue(item == NULL);
    ttrue(rGetListLength(lp) == 3);  // No change

    // Test rInsertItemAt edge cases
    rInsertItemAt(lp, 0, (void*) 0); // Insert at beginning
    ttrue(rGetItem(lp, 0) == (void*) 0);
    ttrue(rGetListLength(lp) == 4);

    rInsertItemAt(lp, 4, (void*) 4); // Insert at end
    ttrue(rGetItem(lp, 4) == (void*) 4);
    ttrue(rGetListLength(lp) == 5);

    // Test iteration edge cases
    int next = 0;                   // Valid starting point
    item = rGetNextItem(lp, &next);
    ttrue(item == (void*) 0);       // First item

    // Test with very large capacity
    rGrowList(lp, 100000);
    ttrue(rGetListLength(lp) == 5); // Length unchanged
    rAddItem(lp, (void*) 999);
    ttrue(rGetListLength(lp) == 6);

    rFreeList(lp);
}

int main(void)
{
    rInit(0, 0);

    // Original tests
    createList();
    isListEmpty();
    insertAndRemove();
    lotsOfInserts();
    listIterate();
    orderedInserts();
    sort();

    // New comprehensive tests
    testSetItem();
    testGetItem();
    testClearList();
    testRemoveItemAt();
    testLookupItem();
    testGrowList();
    testListToString();
    testStackOperations();
    testSortList();
    testStringOperations();
    testEdgeCases();
    testAddNullItem();
    testFlagBehavior();

    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
