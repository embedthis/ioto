/*
    dbLib.c -- DB Library Source

    This file is a catenation of all the source code. Amalgamating into a
    single file makes embedding simpler and the resulting application faster,
    by using compiler optimization within the DB library.

    Prepared by: buildLib.sh
 */

#include "db.h"

#if ME_COM_DB


/********* Start of file src/rb.c ************/

/*
    rb.c - Red/black tree

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/


/*********************************** Locals ***********************************/

#define RB_RED   0
#define RB_BLACK 1

#define RB_ROOT(rbt)  (&(rbt)->root)
#define RB_NIL(rbt)   (&(rbt)->nil)
#define RB_FIRST(rbt) ((rbt)->root.left)

/********************************** Forwards **********************************/

static void deleteFixup(RbTree *rbt, RbNode *current);
static void freeNode(RbTree *rbt, RbNode *node);
static void insertFixup(RbTree *rbt, RbNode *current);
static void printTree(RbTree *rbt, RbNode *node, void (*proc)(void*), int depth, char *label);
static void rotateLeft(RbTree*, RbNode*);
static void rotateRight(RbTree*, RbNode*);

#if KEEP
static int checkOrder(RbTree *rbt, RbNode *n, void *min, void *max);
static int checkHeight(RbTree *rbt, RbNode *node);
#endif

/************************************* Code ***********************************/

PUBLIC RbTree *rbAlloc(int flags, RbCompare compare, RbFree free, void *arg)
{
    RbTree *rbt;

    if (!compare) {
        return 0;
    }
    if ((rbt = rAllocType(RbTree)) == 0) {
        return NULL;
    }
    rbt->dup = flags & RB_DUP;
    rbt->compare = compare;
    rbt->free = free;
    rbt->arg = arg;

    // Sentinel nil node
    rbt->nil.left = rbt->nil.right = rbt->nil.parent = RB_NIL(rbt);
    rbt->nil.color = RB_BLACK;
    rbt->nil.data = NULL;

    // Sentinel root node
    rbt->root.left = rbt->root.right = rbt->root.parent = RB_NIL(rbt);
    rbt->root.color = RB_BLACK;
    rbt->root.data = NULL;
    rbt->min = NULL;
    return rbt;
}

PUBLIC void rbFree(RbTree *rbt)
{
    if (rbt) {
        //  SECURITY: Acceptable - This is recursive
        freeNode(rbt, RB_FIRST(rbt));
        rFree(rbt);
    }
}

//  SECURITY: Acceptable - This is recursive
static void freeNode(RbTree *rbt, RbNode *n)
{
    assert(rbt);
    assert(n);

    if (n != RB_NIL(rbt)) {
        freeNode(rbt, n->left);
        freeNode(rbt, n->right);
        if (rbt->free) {
            rbt->free(rbt->arg, n->data);
        }
        rFree(n);
    }
}

PUBLIC RbNode *rbLookup(RbTree *rbt, cvoid *data, void *ctx)
{
    RbNode *p;
    int    cmp;

    if (rbt == 0 || data == 0) {
        return 0;
    }
    for (p = RB_FIRST(rbt); p != RB_NIL(rbt); ) {
        if ((cmp = rbt->compare(data, p->data, ctx)) == 0) {
            //  Found
            return p;
        }
        p = (cmp < 0) ? p->left : p->right;
    }
    return 0;
}

/*
    Find the first node matching the given data item
 */
PUBLIC RbNode *rbLookupFirst(RbTree *rbt, cvoid *data, void *ctx)
{
    RbNode *found, *p;
    int    cmp;

    if (rbt == 0 || data == 0) {
        return 0;
    }
    found = 0;
    for (p = RB_FIRST(rbt); p != RB_NIL(rbt); ) {
        if ((cmp = rbt->compare(data, p->data, ctx)) == 0) {
            found = p;
            p = p->left;
            continue;
        }
        p = (cmp < 0) ? p->left : p->right;
    }
    if (found) {
        return found;
    }
    return 0;
}

/*
    Find the next node matching the given data item after the given node
    It is assumed that node matches the supplied data.
    NOTE: this only examines the next node.
 */
PUBLIC RbNode *rbLookupNext(RbTree *rbt, RbNode *node, cvoid *data, void *ctx)
{
    assert(rbt);
    assert(node);

    node = rbNext(rbt, node);
    if (node && rbt->compare(data, node->data, ctx) == 0) {
        return node;
    }
    return 0;
}

/*
    Return the lexically first node
 */
PUBLIC RbNode *rbFirst(RbTree *rbt)
{
    if (rbt == 0) {
        return 0;
    }
    return rbt->min;
}

PUBLIC RbNode *rbNext(RbTree *rbt, RbNode *node)
{
    RbNode *p;

    if (rbt == 0 || node == 0) {
        return 0;
    }
    p = node->right;

    if (p != RB_NIL(rbt)) {
        // move down until we find it
        for ( ; p->left != RB_NIL(rbt); p = p->left) {
        }
    } else {
        // move up until we find it or hit the root
        for (p = node->parent; node == p->right; node = p, p = p->parent) {
        }
        if (p == RB_ROOT(rbt)) {
            //  Not found
            p = NULL;
        }
    }
    return p;
}

#if KEEP
/*
    Recursive walk. The Safe runtime uses fiber coroutines and it is best to limit the fiber's stack
    size. So this API isn't ideal.
 */
PUBLIC int rbWalk(RbTree *rbt, RbNode *node, int (*func)(void*, void*), void *cookie, RbOrder order)
{
    int err;

    if (node != RB_NIL(rbt)) {
        if (order == RB_PREORDER && (err = func(node->data, cookie)) != 0) {  /* preorder */
            return err;
        }
        if ((err = rbWalk(rbt, node->left, func, cookie, order)) != 0) {      /* left */
            return err;
        }
        if (order == RB_INORDER && (err = func(node->data, cookie)) != 0) {   /* inorder */
            return err;
        }
        if ((err = rbWalk(rbt, node->right, func, cookie, order)) != 0) {     /* right */
            return err;
        }
        if (order == RB_POSTORDER && (err = func(node->data, cookie)) != 0) { /* postorder */
            return err;
        }
    }
    return 0;
}
#endif

static void rotateLeft(RbTree *rbt, RbNode *x)
{
    RbNode *y;

    //  Child
    assert(rbt);
    assert(x);

    y = x->right;

    //  tree x
    x->right = y->left;
    if (x->right != RB_NIL(rbt)) {
        x->right->parent = x;
    }
    // tree y
    y->parent = x->parent;
    if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    // assemble tree x and tree y
    y->left = x;
    x->parent = y;
}

static void rotateRight(RbTree *rbt, RbNode *x)
{
    RbNode *y;

    assert(rbt);
    assert(x);

    //  Child
    y = x->left;

    // tree x
    x->left = y->right;
    if (x->left != RB_NIL(rbt)) {
        x->left->parent = x;
    }

    // tree y
    y->parent = x->parent;
    if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    // assemble tree x and tree y
    y->right = x;
    x->parent = y;
}

/*
    Insert or update
 */
PUBLIC RbNode *rbInsert(RbTree *rbt, void *data)
{
    RbNode *current, *parent;
    RbNode *newNode;
    int    cmp;

    assert(rbt);
    assert(data);

    //  Do binary search to find where it should be

    current = RB_FIRST(rbt);
    parent = RB_ROOT(rbt);

    while (current != RB_NIL(rbt)) {
        cmp = rbt->compare(data, current->data, NULL);

        if (cmp == 0 && !rbt->dup) {
            if (rbt->free) {
                rbt->free(rbt->arg, current->data);
            }
            current->data = data;
            return current;
        }
        parent = current;
        current = cmp < 0 ? current->left : current->right;
    }

    //  Replace the termination NIL pointer with the new node pointer

    current = newNode = (RbNode*) rAllocType(RbNode);
    if (current == NULL) {
        //  Memory error
        return NULL;
    }
    current->left = current->right = RB_NIL(rbt);
    current->parent = parent;
    current->color = RB_RED;
    current->data = data;

    if (parent == RB_ROOT(rbt) || rbt->compare(data, parent->data, NULL) < 0) {
        parent->left = current;
    } else {
        parent->right = current;
    }
    if (rbt->min == NULL || rbt->compare(current->data, rbt->min->data, NULL) < 0) {
        rbt->min = current;
    }
    /*
       insertion into a red-black tree:
            0-children root cluster (parent node is BLACK) becomes 2-children root cluster (new root node)
                paint root node BLACK, and done
            2-children cluster (parent node is BLACK) becomes 3-children cluster
                done
            3-children cluster (parent node is BLACK) becomes 4-children cluster
                done
            3-children cluster (parent node is RED) becomes 4-children cluster
                rotate, and done
            4-children cluster (parent node is RED) splits into 2-children cluster and 3-children cluster
                split, and insert grandparent node into parent cluster
     */
    if (current->parent->color == RB_RED) {
        // insertion into 3-children cluster (parent node is RED)
        // insertion into 4-children cluster (parent node is RED)
        insertFixup(rbt, current);
    } else {
        /*
           insertion into 0-children root cluster (parent node is BLACK)
           insertion into 2-children cluster (parent node is BLACK)
           insertion into 3-children cluster (parent node is BLACK)
         */
    }

    /*
        The root is always BLACK. Insertion into 0-children root cluster or
        insertion into 4-children root cluster require this recoloring.
     */
    RB_FIRST(rbt)->color = RB_BLACK;
    return newNode;
}

/*
    Rebalance after insertion
    RB_ROOT(rbt) is always BLACK, thus never reach beyond RB_FIRST(rbt) after insertFixup, RB_FIRST(rbt) might be RED
 */
static void insertFixup(RbTree *rbt, RbNode *current)
{
    RbNode *uncle;

    assert(rbt);
    assert(current);

    do {
        // current node is RED and parent node is RED

        if (current->parent == current->parent->parent->left) {
            uncle = current->parent->parent->right;
            if (uncle->color == RB_RED) {
                // insertion into 4-children cluster, split
                current->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;

                // send grandparent node up the tree
                current = current->parent->parent;
                current->color = RB_RED;

            } else {
                // insertion into 3-children cluster
                if (current == current->parent->right) {
                    current = current->parent;
                    rotateLeft(rbt, current);
                }
                // 3-children cluster has two representations
                current->parent->color = RB_BLACK;
                current->parent->parent->color = RB_RED;
                rotateRight(rbt, current->parent->parent);
            }

        } else {
            uncle = current->parent->parent->left;
            if (uncle->color == RB_RED) {
                // insertion into 4-children cluster, split
                current->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;

                // send grandparent node up the tree
                current = current->parent->parent;
                current->color = RB_RED;

            } else {
                // insertion into 3-children cluster
                if (current == current->parent->left) {
                    current = current->parent;
                    rotateRight(rbt, current);
                }
                // 3-children cluster has two representations
                current->parent->color = RB_BLACK;
                current->parent->parent->color = RB_RED;
                rotateLeft(rbt, current->parent->parent);
            }
        }
    } while (current->parent->color == RB_RED);
}

PUBLIC void *rbRemove(RbTree *rbt, RbNode *node, int keep)
{
    RbNode *target, *child;
    void   *data;

    assert(rbt);
    assert(node);

    data = node->data;

    // choose node's in-order successor if it has two children

    if (node->left == RB_NIL(rbt) || node->right == RB_NIL(rbt)) {
        target = node;
        if (rbt->min == target) {
            rbt->min = rbNext(rbt, target);                               // deleted, thus min = successor
        }
    } else {
        /*
            The successor is unlinked below and then takes the node's place in the tree, so the
            caller's node pointer is the only one invalidated by this removal.
            The node has a left child, so it cannot be the minimum and min needs no update.
         */
        target = rbNext(rbt, node);                                       // node->right must not be NIL, thus move down
    }
    child = (target->left == RB_NIL(rbt)) ? target->right : target->left; /* child may be NIL */

    /*
       deletion from red-black tree
         4-children cluster (RED target node) becomes 3-children cluster
           done
         3-children cluster (RED target node) becomes 2-children cluster
           done
         3-children cluster (BLACK target node, RED child node) becomes 2-children cluster
           paint child node BLACK, and done

         2-children root cluster (BLACK target node, BLACK child node) becomes 0-children root cluster
           done

         2-children cluster (BLACK target node, 4-children sibling cluster) becomes 3-children cluster
           transfer, and done
         2-children cluster (BLACK target node, 3-children sibling cluster) becomes 2-children cluster
           transfer, and done

         2-children cluster (BLACK target node, 2-children sibling cluster, 3/4-children parent cluster) becomes
            3-children cluster
           fuse, paint parent node BLACK, and done
         2-children cluster (BLACK target node, 2-children sibling cluster, 2-children parent cluster) becomes
            3-children cluster
           fuse, and delete parent node from parent cluster
     */
    if (target->color == RB_BLACK) {
        if (child->color == RB_RED) {
            // deletion from 3-children cluster (BLACK target node, RED child node)
            child->color = RB_BLACK;
        } else if (target == RB_FIRST(rbt)) {
            // deletion from 2-children root cluster (BLACK target node, BLACK child node)
        } else {
            // deletion from 2-children cluster (BLACK target node, ...)
            deleteFixup(rbt, target);
        }
    } else {
        /*
            deletion from 4-children cluster (RED target node)
            deletion from 3-children cluster (RED target node)
         */
    }
    if (child != RB_NIL(rbt)) {
        child->parent = target->parent;
    }
    if (target == target->parent->left) {
        target->parent->left = child;
    } else {
        target->parent->right = child;
    }
    if (target == node) {
        rFree(target);
    } else {
        /*
            Relink the unlinked successor into the node's position. The successor keeps its own
            data, so a node pointer held by the caller remains valid unless it is this node.
         */
        target->left = node->left;
        target->right = node->right;
        target->parent = node->parent;
        target->color = node->color;
        if (target->left != RB_NIL(rbt)) {
            target->left->parent = target;
        }
        if (target->right != RB_NIL(rbt)) {
            target->right->parent = target;
        }
        if (node == node->parent->left) {
            node->parent->left = target;
        } else {
            node->parent->right = target;
        }
        rFree(node);
    }

    // keep or discard data
    if (keep == 0) {
        if (rbt->free) {
            rbt->free(rbt->arg, data);
        }
        data = NULL;
    }
    return data;
}

static void deleteFixup(RbTree *rbt, RbNode *current)
{
    RbNode *sibling;

    assert(rbt);
    assert(current);

    do {
        if (current == current->parent->left) {
            sibling = current->parent->right;

            if (sibling->color == RB_RED) {
                // perform an adjustment (3-children parent cluster has two representations)
                sibling->color = RB_BLACK;
                current->parent->color = RB_RED;
                rotateLeft(rbt, current->parent);
                sibling = current->parent->right;
            }

            // sibling node must be BLACK now

            if (sibling->right->color == RB_BLACK && sibling->left->color == RB_BLACK) {
                // 2-children sibling cluster, fuse by recoloring
                sibling->color = RB_RED;
                if (current->parent->color == RB_RED) { // 3/4-children parent cluster
                    current->parent->color = RB_BLACK;
                    break;                              // goto break
                } else {                                // 2-children parent cluster
                    current = current->parent;          // goto loop
                }

            } else {
                // 3/4-children sibling cluster. perform an adjustment (3-children sibling cluster has two
                // representations)
                if (sibling->right->color == RB_BLACK) {
                    sibling->left->color = RB_BLACK;
                    sibling->color = RB_RED;
                    rotateRight(rbt, sibling);
                    sibling = current->parent->right;
                }
                // transfer by rotation and recoloring
                sibling->color = current->parent->color;
                current->parent->color = RB_BLACK;
                sibling->right->color = RB_BLACK;
                rotateLeft(rbt, current->parent);
                break;
            }
        } else {
            sibling = current->parent->left;

            if (sibling->color == RB_RED) {
                // perform an adjustment (3-children parent cluster has two representations)
                sibling->color = RB_BLACK;
                current->parent->color = RB_RED;
                rotateRight(rbt, current->parent);
                sibling = current->parent->left;
            }

            // sibling node must be RB_BLACK now

            if (sibling->right->color == RB_BLACK && sibling->left->color == RB_BLACK) {
                // 2-children sibling cluster, fuse by recoloring
                sibling->color = RB_RED;
                if (current->parent->color == RB_RED) { // 3/4-children parent cluster
                    current->parent->color = RB_BLACK;
                    break;
                } else {                                // 2-children parent cluster
                    current = current->parent;          // goto loop
                }
            } else {
                // 3/4-children sibling cluster. Perform an adjustment (3-children sibling cluster has two
                // representations)
                if (sibling->left->color == RB_BLACK) {
                    sibling->right->color = RB_BLACK;
                    sibling->color = RB_RED;
                    rotateLeft(rbt, sibling);
                    sibling = current->parent->left;
                }
                // transfer by rotation and recoloring
                sibling->color = current->parent->color;
                current->parent->color = RB_BLACK;
                sibling->left->color = RB_BLACK;
                rotateRight(rbt, current->parent);
                break; /* goto break */
            }
        }
    } while (current != RB_FIRST(rbt));
}

#if KEEP
PUBLIC int rbCheckOrder(RbTree *rbt, void *min, void *max)
{
    return checkOrder(rbt, RB_FIRST(rbt), min, max);
}

static int checkOrder(RbTree *rbt, RbNode *n, void *min, void *max)
{
    if (n == RB_NIL(rbt)) {
        return 1;
    }
    if (rbt->dup) {
        if (rbt->compare(n->data, min, NULL) < 0 || rbt->compare(n->data, max, NULL) > 0) {
            return 0;
        }
    } else if (rbt->compare(n->data, min, NULL) <= 0 || rbt->compare(n->data, max, NULL) >= 0) {
        return 0;
    }
    return checkOrder(rbt, n->left, min, n->data) && checkOrder(rbt, n->right, n->data, max);
}

PUBLIC int rbCheckHeight(RbTree *rbt)
{
    if (RB_ROOT(rbt)->color == RB_RED || RB_FIRST(rbt)->color == RB_RED || RB_NIL(rbt)->color == RB_RED) {
        return 0;
    }
    return checkHeight(rbt, RB_FIRST(rbt));
}

static int checkHeight(RbTree *rbt, RbNode *n)
{
    int lbh, rbh;

    if (n == RB_NIL(rbt)) {
        return 1;
    }
    if (n->color == RB_RED && (n->left->color == RB_RED || n->right->color == RB_RED || n->parent->color == RB_RED)) {
        return 0;
    }
    if ((lbh = checkHeight(rbt, n->left)) == 0) {
        return 0;
    }
    if ((rbh = checkHeight(rbt, n->right)) == 0) {
        return 0;
    }
    if (lbh != rbh) {
        return 0;
    }
    return lbh + (n->color == RB_BLACK ? 1 : 0);
}
#endif

PUBLIC void rbPrint(RbTree *rbt, void (*proc)(void*))
{
    printTree(rbt, RB_FIRST(rbt), proc, 0, "T");
}

static void printTree(RbTree *rbt, RbNode *n, void (*proc)(void*), int depth, char *label)
{
    char path[256];

    if (n == RB_NIL(rbt)) return;

    printTree(rbt, n->left, proc, depth + 1, sfmtbuf(path, sizeof(path), "%sL", label));
    if (label) {
        rPrintf("%d:%s: ", depth, label);
    }
    proc(n->data);
    rPrintf(" (%s)\n\n", n->color == RB_RED ? "red" : "black");
    printTree(rbt, n->right, proc, depth + 1, sfmtbuf(path, sizeof(path), "%sR", label));
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    Portion copyright (c) 2019 xieqing. https://github.com/xieqing - See LICENSE for details.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/dbLib.c ************/

/*
    db.c - In-memory embedded database

    The embedded database is a high performance NoSQL management document database.
    It offers JSON document items with flexible query API with efficient import and
    export of database items. The database uses fast red/black binary search indexes.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "crypt.h"


/************************************ Locals ***********************************/
/*
    API query context
 */
typedef struct Env {
    Db *db;                     /* Database handle */
    RbTree *index;              /* Index to use for this query */
    DbItem search;              /* Key item to search for */
    size_t searchLen;           /* Length of key item */
    DbItem next;                /* Next item to search for */
    DbModel *model;             /* Item schema model */
    Json *props;                /* Prepared properties for the query */
    DbParams *params;           /* Supplied API parameters */
    bool expired;               /* Query met expired items */
    cchar *indexSort;           /* Sort key property name */
    cchar *compare;             /* Compare operation for the query */
    bool mustMatch;             /* Must match properties or where callback */
} Env;

typedef struct DbCallback {
    DbCallbackProc proc;        /* Function callback */
    char *model;                /* Target model - if null, then all models */
    void *arg;                  /* Function argument */
    int events;                 /* Events subscribe to: DB_ON_CHANGE | DB_ON_COMMIT */
    bool removed;               /* Marked for removal - swept once invokeCallbacks is not active */
} DbCallback;

/**
    Database sync change record. One allocated for each mutation to the database.
    Changes implement a buffer cache for database changes.
 */
typedef struct DbChange {
    Db *db;
    DbModel *model;
    DbParams params;            //  Copy of the API params - the caller's may be transient
    cchar *cmd;
    char *key;
    Time due;                   //  When change is due to be sent
} DbChange;

#define SETUP(db, model, props, params, cmd, env) \
        setup(db, model, props, params ? params : &(DbParams) {}, cmd, env)

#define USER_ALLOC        0x1 /* User allocated json */

//  Latest numeric date accepted, 9999-12-31T23:59:59.999Z, so stored dates keep a four digit year
#define DB_MAX_DATE       253402300799999LL

/*
    Record formats. Version 3 files carry a "DB" magic and little endian lengths. Version 2 files
    have no magic and native byte order lengths, so one written on a host of the other byte order
    is read with its lengths swapped.
 */
#define DB_LEGACY_VERSION 2
#define DB_FORMAT_LE      0
#define DB_FORMAT_NATIVE  1
#define DB_FORMAT_SWAPPED 2

/************************************ Forwards *********************************/

static void addContext(Db *db, Json *props);
static DbChange *allocChange(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                             Ticks due);
static DbField *allocField(Db *db, cchar *name, Json *json, int fid);
static DbItem *allocItem(cchar *name, Json *json, char *value);
static DbModel *allocModel(Db *db, cchar *name, cchar *sync, Time delay);
static int applyChange(Db *db, cchar *cmd, cchar *model, cchar *value);
static int applyJournal(Db *db);
static bool checkEnum(DbField *field, cchar *value);
static int checkRequired(Db *db, DbModel *model, Json *json);
static int checkUnique(Db *db, DbModel *model, Json *json, cchar *key, bool bypass);
static void clearItem(DbItem *item);
static void commitChange(Db *db);
static int compareItems(cvoid *d1, cvoid *d2, Env *env);
static void change(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd, cchar *value);
static char *checkSize(Db *db, cchar *key, Json *json);
static int dberror(Db *db, int code, cchar *fmt, ...);
static void freeEnv(Env *env);
static char *snapshotItem(DbItem *item);
static void restoreItem(DbItem *item, char *snapshot);
static void freeChange(Db *db, DbChange *change);
static void freeField(DbField *field);
static ssize uidSize(cchar *generate);
static void freeModel(DbModel *model);
static void freeItem(Db *db, DbItem *node);
static void removeExpiredEvent(Db *db);
static void removeItem(Db *db, RbTree *index, DbModel *model, RbNode *rp, DbParams *params);
static int flushJournal(Db *db);
static RbTree *getIndex(Db *db, DbParams *params);
static cchar *getIndexName(Db *db, DbParams *params);
static cchar *getIndexHash(Db *db, cchar *index);
static cchar *getIndexSort(Db *db, cchar *index);
static void invokeCallbacks(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                            int event);
static void sweepCallbacks(Db *db);
static bool matchItem(RbNode *rp, Json *j1, JsonNode *n1, Json *j2, JsonNode *n2, Env *env);
static int mapTypes(Db *db, DbModel *model, Json *props);
static bool isNumber(cchar *value);
static bool isScalarType(cchar *type);
static int loadData(Db *db, cchar *path);
static int loadIndexes(Db *db);
static int loadModels(Db *db, Json *json);
static int loadSchema(Db *db, cchar *schema);
static int readBlock(Db *db, FILE *fp, RBuf *buf, int format, cchar **block);
static int readSize(Db *db, FILE *fp, int format, size_t *sizep);
static int endOfJournal(Db *db, FILE *fp);
static int readItem(FILE *fp, int format, DbItem **item);
static int readHeader(Db *db, FILE *fp, cchar *what, int *format);
static int readLength(FILE *fp, int format, uint32 *length);
static int writeHeader(FILE *fp);
static int writeLength(FILE *fp, uint32 length);
static int openJournal(Db *db, bool append);
static int recreateJournal(Db *db);
static bool canAppendJournal(Db *db);
static int replaceFile(cchar *temp, cchar *path);
static int saveDb(Db *db);
static void syncDir(cchar *path);
static void selectProperties(Db *db, DbModel *model, Json *props, DbParams *params, cchar *cmd);
static void setDefaults(Db *db, DbModel *model, Json *props);
static int setTemplates(Db *db, DbModel *model, Json *props);
static void setTimestamps(DbModel *model, Json *props, DbParams *params, cchar *cmd);
static Json *toJson(DbItem *item);
static int writeBlock(Db *db, cchar *buf);
static int writeChangeToJournal(Db *db, DbModel *model, DbItem *item, cchar *cmd, cchar *value);
static int writeItem(FILE *fp, DbItem *item);
static int writeSize(Db *db, uint32_t len);

/************************************** Code ***********************************/
/*
    Open database and load data from the given path.
    Schema defines the data model
 */
PUBLIC Db *dbOpen(cchar *path, cchar *schema, int flags)
{
    Db  *db;
    int count;

    if (!path || !*path || !schema || !*schema) {
        return NULL;
    }
    if ((db = rAllocType(Db)) == 0) {
        return 0;
    }
    db->flags = flags;

    if (path) {
        if (flags & DB_OPEN_RESET) {
            dbReset(path);
        }
        //  SECURITY Acceptable: developers responsibility to validate the path
        db->path = sclone(path);
        db->journalPath = sfmt("%s.jnl", db->path);
        db->maxJournalSize = DB_MAX_LOG_SIZE;
        db->maxJournalAge = DB_MAX_LOG_AGE;
    }
    db->callbacks = rAllocList(0, R_DYNAMIC_VALUE);
    db->context = jsonAlloc();
    db->changes = rAllocHash(0, 0);
    if (loadSchema(db, schema) < 0) {
        rError("db", "%s", db->error);
        dbClose(db);
        return 0;
    }
    if (path) {
        if (loadData(db, path) < 0) {
            rError("db", "%s", db->error);
            dbClose(db);
            return 0;
        }
        //  Recover journal data incase sudden shutdown
        if ((count = applyJournal(db)) < 0) {
            rError("db", "%s", db->error);
            dbClose(db);
            return 0;
        }
        if (count > 0 && dbSave(db, NULL) < 0) {
            /*
                The journal holds the only copy of the recovered changes. Keep it and append,
                rather than truncating it and losing what was just recovered.
             */
            rError("db", "Cannot checkpoint recovered journal: %s", db->error);
            if (!(db->flags & DB_READ_ONLY) && openJournal(db, 1) < 0) {
                rError("db", "%s", db->error);
                dbClose(db);
                return 0;
            }
            return db;
        }
        if (!(db->flags & DB_READ_ONLY) && openJournal(db, 0) < 0) {
            rError("db", "%s", db->error);
            dbClose(db);
            return 0;
        }
    }
    return db;
}

PUBLIC void dbClose(Db *db)
{
    DbModel    *model;
    DbCallback *cb;
    DbChange   *change;
    RName      *np;
    int        ci, saved;

    if (!db) {
        return;
    }
    if (db->expireEvent) {
        //  Complete the removals deferred by reads so they are saved
        rStopEvent(db->expireEvent);
        db->expireEvent = 0;
        dbRemoveExpired(db, 1);
        rStopEvent(db->expireEvent);
    }
    rStopEvent(db->journalEvent);
    rStopEvent(db->commitEvent);
    db->journalEvent = 0;
    db->commitEvent = 0;

    if (!(db->flags & DB_READ_ONLY)) {
        //  Perform a complete save of the in-memory database if the journal has data
        saved = 1;
        if (db->journalSize) {
            saved = dbSave(db, NULL) == 0;
        }
        if (db->journal) {
            fclose(db->journal);
            db->journal = 0;
            if (saved) {
                //  Clean shutdown removes the journal
                unlink(db->journalPath);
            } else {
                /*
                    The journal is the only record of these changes. Keep it so the next open
                    can replay them.
                 */
                rError("db", "Cannot save database, retaining journal %s for recovery",
                       db->journalPath);
            }
        }
    }
    for (ITERATE_NAMES(db->changes, np)) {
        change = np->value;
        freeChange(db, change);
    }
    for (ITERATE_NAME_DATA(db->models, np, model)) {
        freeModel(model);
    }
    for (ITERATE_ITEMS(db->callbacks, cb, ci)) {
        rFree(cb->model);
    }
    rFreeList(db->callbacks);
    rFreeHash(db->models);
    rFreeHash(db->changes);
    rbFree(db->primary);
    rFree(db->error);
    rFree(db->journalPath);
    rFree(db->path);
    jsonFree(db->schema);
    jsonFree(db->context);
    rFree(db);
}

/*
    Load the database schema of application models, indexes and data fields
 */
static int loadSchema(Db *db, cchar *schema)
{
    Json     *blend, *json;
    JsonNode *inc;
    cchar    *dir;
    char     dirbuf[ME_MAX_FNAME], *errorMsg, *path, *str;
    int      blendId;

    if (!db || !schema) {
        return R_ERR_BAD_ARGS;
    }
    if ((json = jsonParseFile(schema, &errorMsg, 0)) == 0) {
        dberror(db, R_ERR_CANT_READ, "%s", errorMsg);
        rFree(errorMsg);
        return R_ERR_CANT_READ;
    }
    /*
        Keep (locked) schema to preserve memory that is used in DbModels and DbFields.
        Simplifies memory allocation / freeing.
     */
    db->schema = json;

    blendId = jsonGetId(db->schema, 0, "blend");
    if (blendId >= 0) {
        /*
            Can't iterate blend[] while blending below
         */
        blend = jsonAlloc();
        jsonBlend(blend, 0, 0, db->schema, blendId, 0, 0);

        /*
            Get base directory of the schema. Includes are relative to that directory
         */
        scopy(dirbuf, sizeof(dirbuf), schema);
        dir = rDirname(dirbuf);

        for (ITERATE_JSON(blend, 0, inc, nid)) {
            path = rJoinFile(dir, inc->value);
            if ((json = jsonParseFile(path, &errorMsg, 0)) == 0) {
                dberror(db, R_ERR_CANT_READ, "Cannot parse blended schema %s\n%s", inc->value,
                        errorMsg);
                rFree(errorMsg);
                rFree(path);
                jsonFree(blend);
                jsonFree(db->schema);
                db->schema = 0;
                return R_ERR_CANT_READ;
            }
            if (jsonBlend(db->schema, 0, 0, json, 0, 0, JSON_COMBINE) < 0) {
                dberror(db, R_ERR_CANT_READ, "Cannot blend schema %s\n%s", inc->value, errorMsg);
                rFree(errorMsg);
                jsonFree(json);
                rFree(path);
                jsonFree(blend);
                jsonFree(db->schema);
                db->schema = 0;
                return R_ERR_CANT_READ;
            }

            jsonFree(json);
            rFree(path);
        }
        jsonFree(blend);
    }

    //  Trace the resulting schema
    if (rEmitLog("debug", "setup")) {
        str = jsonToString(db->schema, 0, 0, JSON_HUMAN);
        rDebug("db", "%s", str);
        rFree(str);
    }

    db->timestamps = jsonGet(db->schema, 0, "params.timestamps", NULL) ? 1 : 0;
    db->type = jsonGet(db->schema, 0, "params.typeField", "_type");

    if (loadModels(db, db->schema) < 0) {
        return R_ERR_CANT_LOAD;
    }
    if (loadIndexes(db) < 0) {
        return R_ERR_CANT_LOAD;
    }
    jsonLock(db->schema);
    return 0;
}

/*
   Load schema models from the schema json file
 */
static int loadModels(Db *db, Json *json)
{
    JsonNode *node, *fp;
    DbField  *field;
    DbModel  *model;
    cchar    *enable, *hash, *mem, *sync;
    char     key[80];
    int      sid, delay, period;

    if (!db || !json) {
        return R_ERR_BAD_ARGS;
    }
    db->models = rAllocHash(0, 0);

    for (ITERATE_JSON_KEY(json, 0, "models", node, mid)) {
        sid = jsonGetId(json, 0, sfmtbuf(key, sizeof(key), "process.%s", node->name));
        enable = jsonGet(json, sid, "enable", "both");

        if (smatch(enable, "cloud")) {
            continue;
        }
        sync = jsonGet(json, sid, "sync", "none");

        if ((mem = jsonGet(json, sid, "mem", NULL)) != NULL &&
            (smatch(mem, "true") || smatch(mem, "1"))) {
            delay = DB_INMEM;
        } else {
            delay = (int) stoi(jsonGet(json, sid, "delay", "0"));
            //  DEPRECATE DELAY
            period = (int) stoi(jsonGet(json, sid, "period", "-2"));
            if (period > -2) {
                delay = period;
            }
            delay *= TPS;
        }
        model = allocModel(db, node->name, sync, delay);
        hash = getIndexHash(db, "primary");

        for (ITERATE_JSON(json, node, fp, fid)) {
            if (smatch(fp->name, hash)) {
                continue;
            }
            if ((field = allocField(db, fp->name, json, fid)) == 0) {
                return db->code ? db->code : R_ERR_MEMORY;
            }
            if (!model->expiresField) {
                model->expiresField = field->ttl ? field->name : NULL;
            }
            rAddName(model->fields, fp->name, field, R_TEMPORAL_NAME | R_STATIC_VALUE);
        }
        if (model->sync) {
            if (!rLookupName(model->fields, "updated")) {
                rError("db", "Model %s is missing required 'updated' field for sync to cloud",
                       model->name);
            }
        }
    }
    return 0;
}

/*
    Load primary index. Currently only supporting a primary index.
 */
static int loadIndexes(Db *db)
{
    if (!db) {
        return R_ERR_BAD_ARGS;
    }
    db->primary = rbAlloc(0, (RbCompare) compareItems, (RbFree) freeItem, db);
    return 0;
}

/*
    Load database data from the persistent store.
    Should be very fast with sequential reads.
 */
static int loadData(Db *db, cchar *path)
{
    DbItem *item;
    FILE   *fp;
    char   corrupt[ME_MAX_FNAME];
    int    format, rc;

    if (!db || !path || !*path) {
        return R_ERR_BAD_ARGS;
    }
    if (rAccessFile(path, R_OK) == 0) {
        if ((fp = fopen(path, "r")) == NULL) {
            return dberror(db, R_ERR_CANT_OPEN, "Cannot open %s", path);
        }
        if ((rc = readHeader(db, fp, "database", &format)) < 0) {
            fclose(fp);
            return rc;
        }
        while (1) {
            if ((rc = readItem(fp, format, &item)) < 0) {
                /* Preserve the corrupt file before the next save replaces it with the readable records. */
                fclose(fp);
                sfmtbuf(corrupt, sizeof(corrupt), "%s.corrupt", path);
                if (rCopyFile(path, corrupt, 0600) < 0) {
                    return dberror(db, R_ERR_CANT_WRITE, "Corrupt record in database %s and cannot preserve it as %s",
                                   path, corrupt);
                }
                dberror(db, rc, "Corrupt record in database %s, remaining items are lost, original preserved as %s",
                        path, corrupt);
                rError("db", "%s", db->error);
                return 0;
            }
            if (!item) {
                break;
            }
            rbInsert(db->primary, item);
        }
        fclose(fp);
    }
    return 0;
}

/*
    Save the database to persistent store in the portable binary form.
 */
PUBLIC int dbSave(Db *db, cchar *path)
{
    RbTree *rbt;
    RbNode *rp;
    DbItem *item;
    char   temp[ME_MAX_FNAME];
    FILE   *fp;
    int    rc, skipped;

    if (!db) {
        return R_ERR_BAD_ARGS;
    }
    if (db->flags & DB_READ_ONLY) {
        return 0;
    }
    rbt = db->primary;
    path = path ? path : db->path;
    if (!path) {
        return dberror(db, R_ERR_BAD_ARGS, "No path to save to");
    }

    /*
        Write to temp and then rename incase of an outage while writing
     */
    sfmtbuf(temp, sizeof(temp), "%s.save", path);
    if ((fp = fopen(temp, "w")) == NULL) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot open %s", temp);
    }
    if (writeHeader(fp) < 0) {
        fclose(fp);
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write version to database file: %d", errno);
    }
    skipped = 0;
    for (rp = rbFirst(rbt); rp; rp = rbNext(rbt, rp)) {
        if ((rc = writeItem(fp, rp->data)) < 0) {
            if (rc == R_ERR_WONT_FIT) {
                /*
                    Too large to be read back. Omit it rather than leaving the whole database
                    unsavable, and name it so the fault is actionable.
                 */
                item = rp->data;
                rError("db", "Item \"%s\" is too large to save and has been omitted", item->key);
                skipped++;
                continue;
            }
            fclose(fp);
            return dberror(db, R_ERR_CANT_WRITE, "Cannot save item");
        }
    }
    /*
        The rename below discards the journal's protection, so the new file must be on stable
        storage first, and the rename itself must be durable before the journal is recreated.
     */
    if (rFlushFile(fileno(fp)) < 0) {
        fclose(fp);
        return dberror(db, R_ERR_CANT_WRITE, "Cannot sync database file: %d", errno);
    }
    fclose(fp);

    if (replaceFile(temp, path) < 0) {
        return dberror(db, R_ERR_CANT_WRITE, "Cannot rename save temp file");
    }
    syncDir(path);
    /*
        If the above fails, the journal will still hold a record of all changes.
        A save to another path is an export and leaves the journal untouched.
     */
    if (smatch(path, db->path) && recreateJournal(db) < 0) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot recreate journal file");
    }
    if (skipped) {
        /*
            The save itself succeeded. Record the omission so it is inspectable, but do not
            fail the save: an item this large can be neither stored nor journalled, so keeping
            the journal for it would grow it without bound and never checkpoint.
         */
        dberror(db, R_ERR_WONT_FIT, "Omitted %d oversize item(s) from the database", skipped);
    }
    return 0;
}

/*
    Replace atomically so path is never absent.
 */
static int replaceFile(cchar *temp, cchar *path)
{
#if ME_WIN_LIKE
    return MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(temp, path);
#endif
}

static int saveDb(Db *db)
{
    db->journalEvent = 0;
    return dbSave(db, NULL);
}

/*
    Make a rename in the directory holding path durable. Best effort: some file systems
    cannot open or sync a directory, and the data file itself is already synced.
 */
static void syncDir(cchar *path)
{
#if !ME_WIN_LIKE
    char dir[ME_MAX_FNAME];
    int  fd;

    scopy(dir, sizeof(dir), path);
    rDirname(dir);
    if ((fd = open(*dir ? dir : ".", O_RDONLY)) >= 0) {
        rFlushFile(fd);
        close(fd);
    }
#else
    (void) path;
#endif
}

/*
    Common setup for an API call. This verifies the API properties and parameters.
    Residuals saved in the API Env.
 */
static int setup(Db *db, cchar *modelName, Json *props, DbParams *params, cchar *cmd, Env *env)
{
    DbModel  *model;
    DbField  *field;
    JsonNode *prop;
    char     *cp;
    cchar    *keyTemplate, *type;
    RbNode   *existing;
    ssize    prefixLen;
    int      rc;

    if (!db || !cmd || !env || !params) {
        return R_ERR_BAD_ARGS;
    }
    memset(env, 0, sizeof(Env));
    rFree(db->error);
    db->error = 0;

    if (!props) {
        if ((props = jsonAlloc()) == 0) {
            return dberror(db, R_ERR_MEMORY, "Cannot allocate properties");
        }
        jsonSetUserFlags(props, USER_ALLOC);

    } else if (!(props->userFlags & USER_ALLOC)) {
        //  User provided json so clone because the props will be modified
        if ((props = jsonClone(props, 0)) == 0) {

            return dberror(db, R_ERR_BAD_ARGS, "Cannot clone properties");
        }
        jsonSetUserFlags(props, USER_ALLOC);
    }
    if (!modelName) {
        modelName = jsonGet(props, 0, db->type, 0);
    }
    env->db = db;
    env->props = props;
    env->params = params;
    env->index = getIndex(db, params);
    env->indexSort = getIndexSort(db, getIndexName(db, params));
    env->mustMatch = env->params->where ? 1 : 0;
    keyTemplate = NULL;
    prefixLen = 0;

    if (!modelName && (smatch(cmd, "create") || smatch(cmd, "update") || smatch(cmd, "remove"))) {
        //  Reads may span models, but writes require a model even when bypassing sync callbacks.
        rc = dberror(db, R_ERR_BAD_ARGS, "Missing model");
        goto fail;
    }
    if (modelName) {
        if ((model = rLookupName(db->models, modelName)) == 0) {
            rc = dberror(db, R_ERR_BAD_ARGS, "Unknown schema model \"%s\"", modelName);
            goto fail;
        }
        env->model = model;
        type = jsonGet(props, 0, db->type, 0);
        if (type && !smatch(type, model->name)) {
            rc = dberror(db, R_ERR_BAD_ARGS, "Item type does not match requested model");
            goto fail;
        }
        if (model && model->expiresField && !smatch(cmd, "update")) {
            env->mustMatch = 1;
        }
        /*
            Validate properties. This ensures only schema properties are accepeted and it validates
               enum values.
            FUTURE: data validations
         */
        for (ITERATE_JSON(props, 0, prop, ppid)) {
            if ((field = rLookupName(model->fields, prop->name)) == 0) {
                if (smatch(prop->name, getIndexHash(db, "primary"))) {
                    //  Ignore cloud side hash
                    continue;
                }
                rTrace("db", "Unknown property \"%s\" in model \"%s\"", prop->name, modelName);
                continue;
            }
            if (field->enums) {
                if (!checkEnum(field, prop->value)) {
                    rc = dberror(db, R_ERR_BAD_ARGS, "Invalid property \"%s\" value \"%s\"",
                                 prop->name, prop->value);
                    goto fail;
                }
            }
            if (!smatch(prop->name, env->indexSort) && !smatch(cmd, "update")) {
                env->mustMatch = 1;
            }
        }
        //  Add global context properties. These take precedence over API supplied properties.
        addContext(db, props);

        //  On create only, set default property values
        if (smatch(cmd, "create") || (smatch(cmd, "update") && env->params->upsert)) {
            setDefaults(db, model, props);
        }
        if (db->timestamps && smatch(cmd, "create")) {
            // Update timestamps are set after matching
            setTimestamps(model, props, params, cmd);
        }

        //  Compute property values using value templates and all other property values
        if (!jsonGet(props, 0, env->indexSort, 0) &&
            (field = rLookupName(model->fields, env->indexSort)) != 0 &&
            (cp = scontains(field->value, "${")) != 0) {
            keyTemplate = field->value;
            prefixLen = cp - keyTemplate;
        }
        if (setTemplates(db, model, props) < 0) {
            rc = R_ERR_BAD_ARGS;
            goto fail;
        }

        //  Map data types
        if (mapTypes(db, model, props) < 0) {
            rc = R_ERR_BAD_ARGS;
            goto fail;
        }

        //  Select the properties required for this API
        selectProperties(db, model, props, params, cmd);
    }

    //  Determine the primary index search key value
    //  (NOTE: the item.key is the key value, item.value is the record value)
    env->search.key = (char*) jsonGet(env->props, 0, env->indexSort, 0);

    if (smatch(cmd, "find")) {
        if (env->params->next) {
            env->next.key = (char*) env->params->next;
        }

    } else if (jsonGet(env->props, 0, env->indexSort, 0) == 0) {
        //  Check we have a key for non-find operations
        rc = dberror(db, R_ERR_BAD_ARGS, "Missing sort key in properties\n%s",
                     jsonString(env->props, JSON_HUMAN));
        goto fail;
    }

    /*
        Only a schema-generated key with a nonempty literal prefix may use an unresolved
        template for a beginsWith search. Caller-supplied keys must never broaden a query.
     */
    if ((cp = scontains(env->search.key, "${")) != 0) {
        if (prefixLen > 0 && cp - env->search.key >= prefixLen &&
            sncmp(env->search.key, keyTemplate, (size_t) prefixLen) == 0 &&
            (smatch(cmd, "find") || (smatch(cmd, "remove") && env->params->limit))) {
            *cp = '\0';
            env->compare = "begins";
        } else {
            rc = dberror(db, R_ERR_BAD_ARGS, "Incomplete sort key in properties: %s\n%s",
                         env->search.key, jsonString(env->props, JSON_HUMAN));
            goto fail;
        }
    }
    //  Must be after find trims templates
    env->searchLen = slen(env->search.key);

    /*
        A key that readItem would reject must never enter the tree, or the database becomes
        unsavable.
     */
    if (env->searchLen > (DB_MAX_KEY - 1)) {
        rc = dberror(db, R_ERR_WONT_FIT, "Sort key exceeds the maximum of %d bytes",
                     DB_MAX_KEY - 1);
        goto fail;
    }

    //  Upserts must not replace an existing row belonging to another model.
    if (env->model && (smatch(cmd, "create") || smatch(cmd, "update")) &&
        (existing = rbLookup(env->index, &env->search, env)) != 0 &&
        !smatch(dbField(existing->data, db->type), env->model->name)) {
        rc = dberror(db, R_ERR_BAD_ARGS, "Stored item belongs to another model");
        goto fail;
    }

    if (env->params->log) {
        rInfo("db", "Command: \"%s\" Properties:\n%s", cmd, jsonString(env->props, JSON_HUMAN));
    }
    return 0;

fail:
    freeEnv(env);
    return rc;
}

static void freeEnv(Env *env)
{
    if (!env) {
        return;
    }
    if (env->props && (env->props->userFlags & USER_ALLOC)) {
        jsonFree(env->props);
        env->props = 0;
    }
    if (env->expired && !env->db->expireEvent) {
        /*
            Defer expiry removal so callbacks cannot free an item being returned by a read.
         */
        env->db->expireEvent = rStartEvent((REventProc) removeExpiredEvent, env->db, 0);
    }
}

static void removeExpiredEvent(Db *db)
{
    db->expireEvent = 0;
    dbRemoveExpired(db, 1);
}

/*
    Create a database item of the given model type.
    Props contains the item properties. Params contains request modifiers.
 */
PUBLIC const DbItem *dbCreate(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    DbItem *item;
    char   *value;

    if (!db) {
        return NULL;
    }
    if (SETUP(db, modelName, props, params, "create", &env) < 0) {
        return 0;
    }
    if (!env.params->upsert && rbLookup(env.index, &env.search, &env)) {
        dberror(db, R_ERR_CANT_CREATE, "Cannot create, item already exists");
        freeEnv(&env);
        return 0;
    }
    if (checkRequired(db, env.model, env.props) < 0) {
        freeEnv(&env);
        return 0;
    }
    if (checkUnique(db, env.model, env.props, env.search.key, env.params->bypass) < 0) {
        freeEnv(&env);
        return 0;
    }
    setTimestamps(env.model, env.props, env.params, "create");
    if ((value = checkSize(db, env.search.key, env.props)) == 0) {
        freeEnv(&env);
        return 0;
    }
    //  AllocItem will assume ownership of the env.props JSON
    item = allocItem(env.search.key, env.props, 0);
    env.props = 0;
    if (!item) {
        dberror(db, R_ERR_MEMORY, "Cannot allocate item");
        rFree(value);
        freeEnv(&env);
        return 0;
    }
    rbInsert(env.index, item);
    change(db, env.model, item, params, env.params->upsert ? "upsert" : "create", value);
    rFree(value);

    if (env.params->log) {
        rInfo("db", "Create result:\n%s", jsonString(item->json, JSON_HUMAN));
    }
    freeEnv(&env);
    return item;
}

/*
    Get a database item of the given model type.
    Props contains item properties to search for. This should include the item key (or key
       ingredients).
    Params contains request modifiers.
 */
PUBLIC const DbItem *dbGet(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "get", &env) < 0) {
        return 0;
    }
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if ((!env.model || smatch(dbField(item, db->type), env.model->name)) &&
            (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env))) {
            if (env.params->log) {
                dbPrintItem(item);
            }
            freeEnv(&env);
            return item;
        }
    }
    freeEnv(&env);
    return 0;
}

/*
    Get a field from an item. This returns a direct (const) reference into the data.
 */
PUBLIC cchar *dbGetField(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db || !modelName || !fieldName) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "get", &env) < 0) {
        return 0;
    }
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if ((!env.model || smatch(dbField(item, db->type), env.model->name)) &&
            (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env))) {
            freeEnv(&env);
            return jsonGet(toJson(item), 0, fieldName, 0);
        }
    }
    freeEnv(&env);
    return 0;
}

PUBLIC bool dbGetBool(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params, bool defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    if (value) {
        return smatch(value, "true");
    }
    return defaultValue;
}

PUBLIC Time dbGetDate(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params, Time defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    if (value) {
        return rParseIsoDate(value);
    }
    return defaultValue;
}

PUBLIC double dbGetDouble(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params,
                          double defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    if (value) {
        return stod(value);
    }
    return defaultValue;
}

PUBLIC int64 dbGetNum(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params, int64 defaultValue)
{
    const DbItem *item;

    if (!db || !modelName || !fieldName) {
        return defaultValue;
    }
    if ((item = dbGet(db, modelName, props, params)) == 0) {
        return defaultValue;
    }

    return jsonGetNum(toJson((DbItem*) item), 0, fieldName, defaultValue);
}

PUBLIC cchar *dbGetString(Db *db, cchar *modelName, cchar *fieldName, Json *props, DbParams *params,
                          cchar *defaultValue)
{
    cchar *value;

    value = dbGetField(db, modelName, fieldName, props, params);
    return value ? value : defaultValue;
}

/*
    Find matching items of the given model type.
    Props contains item properties to search for. This should include the item key (or key
       ingredients).
    Params contains request modifiers.
 */
PUBLIC RList *dbFind(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;
    RList  *list;
    cchar  *compare;
    int    count, limit;

    if (!db) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "find", &env) < 0) {
        return 0;
    }
    limit = env.params->limit ? env.params->limit : MAXINT;
    if ((list = rAllocList(0, 0)) == 0) {
        freeEnv(&env);
        return 0;
    }
    count = 0;

    if (env.params->next) {
        /*
            Lookup the exact last item and then step forward and match with the search key.
            The pagination token is a complete sort key, so suspend any beginsWith comparison.
         */
        compare = env.compare;
        env.compare = 0;
        rp = rbLookupFirst(env.index, &env.next, &env);
        env.compare = compare;
        if (rp) {
            if (env.search.key) {
                rp = rbLookupNext(env.index, rp, &env.search, &env);
            } else {
                rp = rbNext(env.index, rp);
            }
        }
    } else if (env.search.key) {
        rp = rbLookupFirst(env.index, &env.search, &env);
    } else {
        rp = rbFirst(db->primary);
    }
    while (rp) {
        item = rp->data;
        if ((!env.model || smatch(dbField(item, db->type), env.model->name)) &&
            (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env))) {
            rPushItem(list, item);
            if (++count >= limit) {
                break;
            }
        }
        if (env.search.key) {
            rp = rbLookupNext(env.index, rp, &env.search, &env);
        } else {
            rp = rbNext(db->primary, rp);
        }
    }
    if (env.params->log) {
        dbPrintList(list);
    }
    freeEnv(&env);
    return list;
}

/*
    Find one database item.
 */
PUBLIC const DbItem *dbFindOne(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;

    if (!db) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "find", &env) < 0) {
        return 0;
    }
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if ((!env.model || smatch(dbField(item, db->type), env.model->name)) &&
            (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env))) {
            freeEnv(&env);
            return item;
        }
    }
    freeEnv(&env);
    return 0;
}

/*
    Remove and item.
    If limit > 0, then can do remove without specifying SK and will remove one or more matching
       items
 */
PUBLIC int dbRemove(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    RList  *keys;
    DbItem *item, *search, lookup;
    char   *key;
    int    count, index, limit;

    if (!db || !props) {
        return R_ERR_BAD_ARGS;
    }
    if (SETUP(db, modelName, props, params, "remove", &env) < 0) {
        return db->code;
    }
    limit = env.params->limit ? env.params->limit : 1;
    search = &env.search;

    /*
        Collect keys first; removal callbacks can free other index nodes.
     */
    keys = rAllocList(0, R_TEMPORAL_VALUE);
    for (rp = rbLookupFirst(env.index, search, &env); rp; rp = rbLookupNext(env.index, rp, search, &env)) {
        item = rp->data;
        if ((!env.model || smatch(dbField(item, db->type), env.model->name)) &&
            (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env))) {
            rAddItem(keys, item->key);
            if (rGetListLength(keys) >= limit) {
                break;
            }
        }
    }
    /*
        Look up again because earlier callbacks may have removed the item.
     */
    count = 0;
    for (ITERATE_ITEMS(keys, key, index)) {
        lookup.key = key;
        if ((rp = rbLookup(env.index, &lookup, NULL)) != 0) {
            removeItem(db, env.index, env.model, rp, params);
            count++;
        }
    }
    rFreeList(keys);
    freeEnv(&env);
    if (count) {
        flushJournal(db);
    }
    return count;
}

/*
    Remove expired items, explicitly or after reads schedule cleanup.
 */
PUBLIC int dbRemoveExpired(Db *db, bool notify)
{
    Env     env;
    RbNode  *rp, *next;
    DbModel *model;
    DbItem  *item;
    RName   *np;
    Json    *json, *allocJson;
    cchar   *expires;
    char    *now;
    int     count, rc;

    if (!db) {
        return R_ERR_BAD_ARGS;
    }
    now = rGetIsoDate(rGetTime());
    db->servicing = 1;
    count = 0;
    rc = 0;

    for (ITERATE_NAME_DATA(db->models, np, model)) {
        if (!model->expiresField) continue;
        if ((rc = SETUP(db, model->name, NULL, NULL, "find", &env)) < 0) {
            break;
        }
again:
        for (rp = rbLookupFirst(env.index, &env.search, &env); rp; rp = next) {
            next = rbLookupNext(env.index, rp, &env.search, &env);
            item = rp->data;
            //  Do this rather than using dbJson so we don't convert nodes to json unnecessarily
            allocJson = 0;
            json = item->json ? item->json : (allocJson = jsonParse(item->value, 0));
            expires = jsonGet(json, 0, model->expiresField, 0);
            if (expires && scmp(expires, now) <= 0) {
                if (rEmitLog("trace", "db")) {
                    rTrace("db", "Remove expired item:\n%s", jsonString(json, JSON_HUMAN));
                }
                if (allocJson) {
                    jsonFree(allocJson);
                }
                if (notify) {
                    //  Unlink before notifying so a callback cannot reach the item being removed
                    removeItem(db, db->primary, model, rp, NULL);
                } else {
                    rbRemove(db->primary, rp, 0);
                }
                count++;
                goto again;
            }
            if (allocJson) {
                jsonFree(allocJson);
            }
        }
        freeEnv(&env);
    }
    db->servicing = 0;
    rFree(now);

    if (rc < 0) {
        return rc;
    }
    /*
        A save deferred while servicing is taken now, as flushJournal re-evaluates the same
        conditions that deferred it.
     */
    if (flushJournal(db) < 0) {
        return R_ERR_CANT_WRITE;
    }
    return count;
}

/*
    Convert all json back to a string rep
 */
PUBLIC void dbCompact(Db *db)
{
    RbNode *rp;
    DbItem *item;

    for (rp = rbFirst(db->primary); rp; rp = rbNext(db->primary, rp)) {
        item = rp->data;
        if (item->json) {
            item->value = jsonToString(item->json, 0, 0, 0);
            item->allocatedValue = 1;
            jsonFree(item->json);
            item->json = 0;
        }
    }
}

/*
    Update a field in an item.
    Warning: caller must provide props to uniquely identify the item
    Value may be null to remove the property.
 */
PUBLIC const DbItem *dbSetField(Db *db, cchar *modelName, cchar *fieldName, cchar *value,
                                Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;
    Json   *json;
    char   *str, *snapshot;

    if (!db || !modelName || !fieldName) {
        return 0;
    }
    if (SETUP(db, modelName, props, params, "update", &env) < 0) {
        return 0;
    }
    if (smatch(fieldName, db->type) || smatch(fieldName, getIndexSort(db, "primary")) ||
        smatch(fieldName, getIndexHash(db, "primary"))) {
        freeEnv(&env);
        dberror(db, R_ERR_BAD_ARGS, "Cannot set an item's key or type field");
        return 0;
    }
    item = 0;
    snapshot = 0;

    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if ((!env.model || smatch(dbField(item, db->type), env.model->name)) &&
            (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env))) {
            break;
        }
        item = 0;
    }
    if (item) {
        //  Modified in place so references into the item stay valid, and undone if too large
        snapshot = snapshotItem(item);
        json = toJson(item);
    } else if (env.params->upsert) {
        json = env.props;
    } else {
        dberror(db, R_ERR_NOT_READY, "Cannot set field, item does not exist");
        freeEnv(&env);
        return 0;
    }
    if (value == NULL) {
        jsonRemove(json, 0, fieldName);
    } else {
        jsonSet(json, 0, fieldName, value, 0);
    }
    if (!item || value) {
        setTimestamps(env.model, json, env.params, "update");
    }
    if (checkRequired(db, env.model, json) < 0 ||
        checkUnique(db, env.model, json, env.search.key, env.params->bypass) < 0) {
        if (item) {
            restoreItem(item, snapshot);
        }
        freeEnv(&env);
        return 0;
    }
    if ((str = checkSize(db, env.search.key, json)) == 0) {
        if (item) {
            restoreItem(item, snapshot);
        }
        freeEnv(&env);
        return 0;
    }
    rFree(snapshot);
    if (!item) {
        if ((item = allocItem(env.search.key, env.props, 0)) == 0) {
            dberror(db, R_ERR_MEMORY, "Cannot allocate item");
            rFree(str);
            freeEnv(&env);
            return 0;
        }
        rbInsert(env.index, item);
    }
    change(db, env.model, item, params, env.params->upsert ? "upsert" : "update", str);
    rFree(str);
    freeEnv(&env);
    return item;
}

PUBLIC const DbItem *dbSetBool(Db *db, cchar *modelName, cchar *fieldName, bool value, Json *props, DbParams *params)
{
    return dbSetField(db, modelName, fieldName, value ? "true" : "false", props, params);
}

PUBLIC const DbItem *dbSetDouble(Db *db, cchar *modelName, cchar *fieldName, double value, Json *props,
                                 DbParams *params)
{
    char buf[32];

    rSnprintf(buf, sizeof(buf), "%f", value);
    return dbSetField(db, modelName, fieldName, buf, props, params);
}

PUBLIC const DbItem *dbSetDate(Db *db, cchar *modelName, cchar *fieldName, Time when, Json *props, DbParams *params)
{
    char         *value;
    const DbItem *item;

    value = rGetIsoDate(when);
    item = dbSetField(db, modelName, fieldName, value, props, params);
    rFree(value);
    return item;
}

PUBLIC const DbItem *dbSetNum(Db *db, cchar *modelName, cchar *fieldName, int64 value, Json *props, DbParams *params)
{
    char buf[32];

    return dbSetField(db, modelName, fieldName, sitosbuf(buf, sizeof(buf), value, 10), props, params);
}

PUBLIC const DbItem *dbSetString(Db *db, cchar *modelName, cchar *fieldName, cchar *value, Json *props,
                                 DbParams *params)
{
    return dbSetField(db, modelName, fieldName, value, props, params);
}


/*
    Update an item
 */
PUBLIC const DbItem *dbUpdate(Db *db, cchar *modelName, Json *props, DbParams *params)
{
    Env    env;
    RbNode *rp;
    DbItem *item;
    Json   *json;
    char   *value, *snapshot;

    if (!db) {
        return NULL;
    }
    if (!props) {
        dberror(db, R_ERR_BAD_ARGS, "Cannot update, bad properties");
        return 0;
    }
    if (SETUP(db, modelName, props, params, "update", &env) < 0) {
        return 0;
    }
    item = 0;
    for (ITERATE_INDEX(env.index, rp, &env.search, &env)) {
        item = rp->data;
        if ((!env.model || smatch(dbField(item, db->type), env.model->name)) &&
            (!env.mustMatch || matchItem(rp, env.props, 0, toJson(item), 0, &env))) {
            break;
        }
        item = 0;
    }
    setTimestamps(env.model, env.props, env.params, "update");

    if (item) {
        if (env.params->upsert) {
            if (checkRequired(db, env.model, env.props) < 0 ||
                checkUnique(db, env.model, env.props, env.search.key, env.params->bypass) < 0) {
                freeEnv(&env);
                return 0;
            }
            if ((value = checkSize(db, env.search.key, env.props)) == 0) {
                freeEnv(&env);
                return 0;
            }
            env.props->userFlags &= (uchar) ~USER_ALLOC;
            clearItem(item);
            item->json = env.props;
        } else {
            /*
                Preserve existing properties that are not being updated. The blend is in place
                so that references into the item stay valid, and is undone if the result is
                too large to store.
             */
            snapshot = snapshotItem(item);
            json = toJson(item);
            if (jsonBlend(json, 0, 0, env.props, 0, 0, JSON_REMOVE_UNDEF) < 0) {
                dberror(db, R_ERR_MEMORY, "Cannot update, merged item is too large");
                restoreItem(item, snapshot);
                freeEnv(&env);
                return 0;
            }
            if (checkRequired(db, env.model, json) < 0 ||
                checkUnique(db, env.model, json, env.search.key, env.params->bypass) < 0) {
                restoreItem(item, snapshot);
                freeEnv(&env);
                return 0;
            }
            if ((value = checkSize(db, env.search.key, json)) == 0) {
                restoreItem(item, snapshot);
                freeEnv(&env);
                return 0;
            }
            rFree(snapshot);
        }

    } else {
        if (!env.params->upsert) {
            dberror(db, R_ERR_CANT_FIND, "Cannot update, item does not exist");
            freeEnv(&env);
            return 0;
        }
        if (checkRequired(db, env.model, env.props) < 0 ||
            checkUnique(db, env.model, env.props, env.search.key, env.params->bypass) < 0) {
            freeEnv(&env);
            return 0;
        }
        if ((value = checkSize(db, env.search.key, env.props)) == 0) {
            freeEnv(&env);
            return 0;
        }
        if ((item = allocItem(env.search.key, env.props, 0)) != 0) {
            rbInsert(env.index, item);
        }
    }
    change(db, env.model, item, params, env.params->upsert ? "upsert" : "update", value);
    rFree(value);
    freeEnv(&env);
    return item;
}

PUBLIC cchar *dbField(const DbItem *item, cchar *fieldName)
{
    if (!item || !fieldName) {
        return 0;
    }
    return jsonGet(toJson((DbItem*) item), 0, fieldName, 0);
}

PUBLIC double dbFieldDouble(const DbItem *item, cchar *fieldName)
{
    return stof(dbField(item, fieldName));
}

PUBLIC int64 dbFieldNumber(const DbItem *item, cchar *fieldName)
{
    if (!item || !fieldName) {
        return 0;
    }

    return jsonGetNum(toJson((DbItem*) item), 0, fieldName, 0);
}

PUBLIC bool dbFieldBool(const DbItem *item, cchar *fieldName)
{
    cchar *value;

    value = dbField(item, fieldName);
    return (smatch(value, "true") || smatch(value, "1")) ? 1 : 0;
}

PUBLIC Time dbFieldDate(const DbItem *item, cchar *fieldName)
{
    cchar *when;

    if ((when = dbField(item, fieldName)) == NULL) {
        return 0;
    }
    return rParseIsoDate(when);
}

PUBLIC char *dbListToString(RList *items)
{
    const DbItem *item;
    RBuf         *buf;
    int          index;

    if (!items) {
        return NULL;
    }
    buf = rAllocBuf(0);
    rPutCharToBuf(buf, '[');
    for (ITERATE_ITEMS(items, item, index)) {
        rPutStringToBuf(buf, jsonToString(toJson((DbItem*) item), 0, 0, 0));
        rPutCharToBuf(buf, ',');
    }
    if (index > 0) {
        //  Drop the trailing comma. An empty list has none to drop.
        rAdjustBufEnd(buf, -1);
    }
    rPutCharToBuf(buf, ']');
    return rBufToStringAndFree(buf);
}

static void printItem(DbItem *item)
{
    char *value;

    if (!item) return;
    value = jsonToString(toJson(item), 0, 0, JSON_HUMAN);
    value[slen(value) - 1] = '\0';
    rPrintf("%s", value);
    rFree(value);
}

PUBLIC void dbPrintTree(Db *db)
{
    rbPrint(db->primary, (void*) printItem);
}

PUBLIC void dbPrintItem(const DbItem *item)
{
    char *value;

    if (item) {
        value = jsonToString(toJson((DbItem*) item), 0, 0, JSON_HUMAN);
        rPrintf("%s: %s\n", item->key, value);
        rFree(value);
    } else {
        rPrintf("Item not defined\n");
    }
}

PUBLIC void dbPrintList(RList *list)
{
    const DbItem *item;
    char         *value;
    int          index;

    for (ITERATE_ITEMS(list, item, index)) {
        value = jsonToString(toJson((DbItem*) item), 0, 0, JSON_HUMAN);
        rPrintf("    %s: %s\n", item->key, value);
        rFree(value);
    }

}

PUBLIC void dbPrintProperties(Json *props)
{
    rPrintf("Properties\n%s\n", jsonToString(props, 0, 0, JSON_HUMAN));
}

PUBLIC void dbPrint(Db *db)
{
    RbNode *rp;

    for (rp = rbFirst(db->primary); rp; rp = rbNext(db->primary, rp)) {
        dbPrintItem(rp->data);
    }
}

/*
    This will convert an item value to JSON for queries and will create item.json and zero item.value
 */
static Json *toJson(DbItem *item)
{
    if (!item) {
        return 0;
    }
    if (!item->json) {
        item->json = jsonParse(item->value, 0);
        if (item->allocatedValue) {
            rFree(item->value);
        }
        item->value = 0;
    }
    return item->json;
}

/*
    Public function to get the JSON object for an item. Because this returns a reference to
    the internal data, we return a const pointer.
    Force a const DbItem so callers don't need to cast returns from dbGet etc.
    Caller must not modify or free the return result.
 */
PUBLIC const Json *dbJson(const DbItem *citem)
{
    return toJson((DbItem*) citem);
}

PUBLIC cchar *dbString(const DbItem *citem, int flags)
{
    DbItem *item;

    item = (DbItem*) citem;
    if (item->json) {
        if (item->allocatedValue) {
            rFree(item->value);
        }
        item->value = jsonToString(item->json, 0, NULL, flags);
        item->allocatedValue = 1;
    }
    return item->value;
}

static int getTypeFromSchema(cchar *type)
{
    if (smatch(type, "array")) {
        return JSON_ARRAY;
    } else if (smatch(type, "object")) {
        return JSON_OBJECT;
    } else if (smatch(type, "string")) {
        return JSON_STRING;
    } else {
        return JSON_PRIMITIVE;
    }
}

/*
    For create() set the default properties or generated values
 */
static void setDefaults(Db *db, DbModel *model, Json *props)
{
    RName   *np;
    DbField *field;
    char    *value;
    ssize   size;

    if (!db || !model || !props) {
        return;
    }
    for (ITERATE_NAME_DATA(model->fields, np, field)) {
        if (field->def || field->generate) {
            if (jsonGet(props, 0, np->name, 0) != 0) {
                continue;
            }
            if (field->def) {
                jsonSet(props, 0, np->name, field->def, getTypeFromSchema(field->type));

            } else if (field->generate) {
                if (smatch(field->generate, "ulid")) {
                    value = dbGetULID(rGetTime());
                    jsonSet(props, 0, np->name, (void*) value, JSON_STRING);
                    rFree(value);
                } else if (smatch(field->generate, "uid")) {
                    value = dbGetUID(10);
                    jsonSet(props, 0, np->name, (void*) value, JSON_STRING);
                    rFree(value);
                } else if ((size = uidSize(field->generate)) > 0) {
                    value = dbGetUID((size_t) size);
                    jsonSet(props, 0, np->name, (void*) value, JSON_STRING);
                    rFree(value);
                }
            }
        }
    }
}

static void setTimestamps(DbModel *model, Json *props, DbParams *params, cchar *cmd)
{
    char *value;

    if (params && params->bypass) {
        /*
            Preserve journal timestamps so replayed removals match their items.
         */
        return;
    }
    value = 0;

    if (smatch(cmd, "create")) {
        if (rLookupName(model->fields, "created")) {
            value = rGetIsoDate(rGetTime());
            jsonSet(props, 0, "created", (void*) value, 0);
        }
        if (rLookupName(model->fields, "updated")) {
            if (!value) {
                value = rGetIsoDate(rGetTime());
            }
            jsonSet(props, 0, "updated", (void*) value, 0);
        }
    } else {
        if (rLookupName(model->fields, "updated")) {
            value = rGetIsoDate(rGetTime());
            jsonSet(props, 0, "updated", (void*) value, 0);
        }
    }
    rFree(value);
}

/*
    Set the value of fields using value templates
 */
static int setTemplates(Db *db, DbModel *model, Json *props)
{
    RName   *np;
    DbField *field;
    char    *value, *name;
    cchar   *supplied, *cp, *end, *part;
    bool    key;

    if (!db || !model || !props) {
        return R_ERR_BAD_ARGS;
    }
    for (ITERATE_NAME_DATA(model->fields, np, field)) {
        if (!field->value) {
            continue;
        }
        key = smatch(np->name, getIndexSort(db, "primary")) ||
              smatch(np->name, getIndexHash(db, "primary"));
        supplied = jsonGet(props, 0, np->name, 0);
        if (key) {
            //  Do not allow key ingredients to inject hash separators or template expressions.
            for (cp = field->value; (cp = scontains(cp, "${")) != 0; cp = end + 1) {
                if ((end = strchr(cp + 2, '}')) == 0) {
                    return dberror(db, R_ERR_BAD_ARGS, "Invalid key template");
                }
                name = snclone(cp + 2, (size_t) (end - cp - 2));
                part = jsonGet(props, 0, name, 0);
                rFree(name);
                if (part && (scontains(part, "${") || (strchr(field->value, '#') && strchr(part, '#')))) {
                    return dberror(db, R_ERR_BAD_ARGS, "Invalid key template ingredient");
                }
            }
        }
        if (key || !supplied) {
            value = jsonTemplate(props, field->value, 1);
            if (key && supplied && (scontains(value, "${") || !smatch(supplied, value))) {
                rFree(value);
                return dberror(db, R_ERR_BAD_ARGS, "Supplied key does not match schema template");
            }
            jsonSet(props, 0, np->name, value, 0);
            rFree(value);
        }
    }
    return 0;
}

/*
    Map and validate data types
 */
static int mapTypes(Db *db, DbModel *model, Json *props)
{
    DbField  *field;
    JsonNode *prop;
    cchar    *value;
    char     *date, *end;
    Time     when;

    if (!db || !model || !props) {
        return R_ERR_BAD_ARGS;
    }
again:
    for (ITERATE_JSON(props, 0, prop, ppid)) {
        field = rLookupName(model->fields, prop->name);
        if (!field) {
            //  Unknown field. Maybe context that does not apply for this model
            jsonRemove(props, ppid, 0);
            //  Can't iterate and mutate
            goto again;
        }
        if (prop->type == JSON_PRIMITIVE && smatch(prop->value, "undefined")) {
            continue;
        }
        if ((prop->type == JSON_OBJECT || prop->type == JSON_ARRAY) && isScalarType(field->type)) {
            return dberror(db, R_ERR_BAD_ARGS, "Invalid %s in property \"%s\": not a scalar value",
                           field->type, prop->name);
        }
        if (smatch(field->type, "date")) {
            if (sfnumber(prop->value)) {
                //  Milliseconds since the epoch
                if (sparseUint(prop->value, 10, DB_MAX_DATE, &end, &when) < 0 || *end) {
                    return dberror(db, R_ERR_BAD_ARGS, "Invalid date in property \"%s\": %s",
                                   prop->name, prop->value);
                }
            } else if ((when = rParseIsoDate(prop->value)) < 0) {
                return dberror(db, R_ERR_BAD_ARGS, "Invalid date in property \"%s\": %s",
                               prop->name, prop->value);
            }
            //  Store the canonical UTC form so date comparisons can be lexicographic
            date = rGetIsoDate(when);
            if (rParseIsoDate(date) < 0) {
                rFree(date);
                return dberror(db, R_ERR_BAD_ARGS, "Invalid date in property \"%s\": %s",
                               prop->name, prop->value);
            }
            jsonSetNodeValue(prop, date, JSON_STRING, JSON_PASS_VALUE);

        } else if (smatch(field->type, "boolean")) {
            if (smatch(prop->value, "true") || smatch(prop->value, "1")) {
                value = "true";
            } else if (smatch(prop->value, "false") || smatch(prop->value, "0")) {
                value = "false";
            } else {
                return dberror(db, R_ERR_BAD_ARGS, "Invalid boolean in property \"%s\": %s",
                               prop->name, prop->value);
            }
            jsonSetNodeValue(prop, value, JSON_PRIMITIVE, 0);

        } else if (smatch(field->type, "number")) {
            if (!isNumber(prop->value)) {
                return dberror(db, R_ERR_BAD_ARGS, "Invalid numeric in property \"%s\": %s",
                               prop->name, prop->value);
            }
            if (prop->type != JSON_PRIMITIVE) {
                jsonSetNodeType(prop, JSON_PRIMITIVE);
            }

        } else if (smatch(field->type, "string")) {
            if (prop->type != JSON_STRING) {
                jsonSetNodeType(prop, JSON_STRING);
            }
        }
    }
    return 0;
}

static bool isScalarType(cchar *type)
{
    return smatch(type, "boolean") || smatch(type, "date") || smatch(type, "number") || smatch(type, "string");
}

/*
    Validate JSON numbers; reject true, false, and null before parsing.
 */
static bool isNumber(cchar *value)
{
    Json *json;
    bool result;

    if (!value || (*value != '-' && !isdigit((uchar) * value))) {
        return 0;
    }
    json = jsonParse(value, JSON_STRICT_PARSE);
    result = json && jsonGetType(json, 0, NULL) == JSON_PRIMITIVE;
    jsonFree(json);
    return result;
}

/*
    Select the properties required for the API
 */
static void selectProperties(Db *db, DbModel *model, Json *props, DbParams *params, cchar *cmd)
{
    JsonNode *prop;
    DbField  *field;

    for (ITERATE_JSON(props, 0, prop, ppid)) {
        field = rLookupName(model->fields, prop->name);
        if (!field) {
            jsonRemove(props, ppid, 0);
        }
    }
    if (smatch(cmd, "create") || (smatch(cmd, "update") && params->upsert)) {
        jsonSet(props, 0, db->type, (char*) model->name, 0);
    }
}

/*
    Match an item in n1 against a target in n2.
    Env provides the comparison options
 */
static bool matchItem(RbNode *rp, Json *j1, JsonNode *n1, Json *j2, JsonNode *n2, Env *env)
{
    JsonNode *c1, *c2;
    cchar    *expires;
    char     *now;
    int      index, nid, rc;

    if (j2->count == 0) {
        return 0;
    }
    if (n2 == 0) {
        n2 = jsonGetNode(j2, 0, 0);
    }
    if (j1->count > 0) {
        if (n1 == 0) {
            n1 = jsonGetNode(j1, 0, 0);
        }
        /*
            Match given properties
         */
        index = 0;
        for (ITERATE_JSON(j1, n1, c1, cid)) {
            if (n1->type == JSON_ARRAY) {
                //  Array elements are unnamed, so they must be matched by position
                c2 = jsonGetChildNode(j2, jsonGetNodeId(j2, n2), index++);
            } else {
                if (smatch(c1->name, env->indexSort)) {
                    //  Already been done via the lookup
                    continue;
                }
                c2 = jsonGetNode(j2, jsonGetNodeId(j2, n2), c1->name);
            }
            if (!c2) return 0;

            rc = scmp(c1->value, c2->value);
            if (rc != 0) return 0;

            if (c1->type == JSON_OBJECT || c1->type == JSON_ARRAY) {
                if (c1->type != c2->type || !matchItem(rp, j1, c1, j2, c2, env)) {
                    return 0;
                }
            }
        }
        if (n1->type == JSON_ARRAY && jsonGetChildNode(j2, jsonGetNodeId(j2, n2), index) != 0) {
            //  The item's array has more elements than the array being matched
            return 0;
        }
    }
    /*
        Invoke where expression callbacks
     */
    rc = 1;
    if (env->params->where) {
        nid = jsonGetNodeId(j2, n2);
        rc = (env->params->where)(j2, nid, env->params->arg);
    }
    if (rc && env->model->expiresField) {
        expires = jsonGet(j2, 0, env->model->expiresField, 0);
        now = rGetIsoDate(rGetTime());
        if (expires && scmp(expires, now) <= 0) {
            env->expired = 1;
            rc = 0;
        }
        rFree(now);
    }
    return rc;
}

static DbModel *allocModel(Db *db, cchar *name, cchar *sync, Time delay)
{
    DbModel *model;
    DbField *field;

    if (!db || !name) {
        return NULL;
    }
    if ((model = rAllocType(DbModel)) == 0) {
        return 0;
    }
    model->name = name;
    model->sync = (smatch(sync, "both") || smatch(sync, "up") || smatch(sync, "down")) ? 1 : 0;
    model->delay = delay;
    model->fields = rAllocHash(0, 0);

    /*
        Add type field to the model
     */
    field = rAllocType(DbField);
    field->name = sclone(name);
    field->hidden = 1;
    rAddName(model->fields, db->type, field, R_TEMPORAL_NAME | R_STATIC_VALUE);

    rAddName(db->models, model->name, model, 0);
    return model;
}

static void freeModel(DbModel *model)
{
    RName   *fp;
    DbField *field;

    if (model) {
        for (ITERATE_NAME_DATA(model->fields, fp, field)) {
            freeField(field);
        }
        rFreeHash(model->fields);
        rFree(model);
    }
}

PUBLIC DbModel *dbGetModel(Db *db, cchar *name)
{
    return rLookupName(db->models, name);
}

PUBLIC DbModel *dbGetItemModel(Db *db, CDbItem *item)
{
    cchar *modelName;

    modelName = dbField(item, db->type);
    return rLookupName(db->models, modelName);
}

/*
    Allocate a model field from the OneTable schema
 */
static DbField *allocField(Db *db, cchar *name, Json *json, int fid)
{
    DbField  *field;
    JsonNode *enums, *child;

    if (!name || !json || fid < 0) {
        return NULL;
    }
    if (jsonGetNode(json, fid, "validate") || jsonGetNode(json, fid, "crypt")) {
        dberror(db, R_ERR_BAD_ARGS,
                "Field \"%s\" declares unsupported schema attribute \"%s\"", name,
                jsonGetNode(json, fid, "validate") ? "validate" : "crypt");
        return NULL;
    }
    if (jsonGetNode(json, fid, "map")) {
        //  A storage-name mapping for the cloud. Items are stored under their field name here.
        rInfo("db", "Field \"%s\" schema attribute \"map\" is not supported and is ignored", name);
    }
    if ((field = rAllocType(DbField)) == 0) {
        return 0;
    }
    //  Memory is preserved in the schema - so can use jsonGet references
    field->name = sclone(name);
    field->def = jsonGet(json, fid, "default", 0);
    field->generate = jsonGet(json, fid, "generate", 0);
    if (sstarts(field->generate, "uid(") && uidSize(field->generate) < 0) {
        dberror(db, R_ERR_BAD_ARGS, "Field \"%s\" has an invalid generate length \"%s\"", name,
                field->generate);
        freeField(field);
        return NULL;
    }
    field->hidden = jsonGetBool(json, fid, "hidden", smatch(name, "pk") || smatch(name, "sk"));
    field->required = jsonGetBool(json, fid, "required", 0);
    field->type = jsonGet(json, fid, "type", 0);
    field->value = jsonGet(json, fid, "value", 0);
    field->ttl = jsonGetBool(json, fid, "ttl", 0);
    field->unique = jsonGetBool(json, fid, "unique", 0);

    if ((enums = jsonGetNode(json, fid, "enum")) != 0) {
        /*
            Compare decoded enum values so embedded quotes remain part of the value.
         */
        field->enums = rAllocList(0, R_TEMPORAL_VALUE);
        for (ITERATE_JSON(json, enums, child, cid)) {
            rAddItem(field->enums, child->value);
        }
    }
    return field;
}

/*
    Length of a "uid(N)" generator, from 1 to the maximum key length. Negative if malformed.
 */
static ssize uidSize(cchar *generate)
{
    char  *end;
    int64 size;

    if (!sstarts(generate, "uid(")) {
        return R_ERR_BAD_ARGS;
    }
    if (sparseUint(&generate[4], 10, DB_MAX_KEY, &end, &size) < 0 || !smatch(end, ")") || size == 0) {
        return R_ERR_BAD_ARGS;
    }
    return (ssize) size;
}

static void freeField(DbField *field)
{
    if (field) {
        rFreeList(field->enums);
        rFree(field->name);
        rFree(field);
    }
}

/*
    Assume ownership of the value and the json.
    The "json" and "value" may be null.
    If both are set, "json" takes precedence and may be more current.
 */
static DbItem *allocItem(cchar *key, Json *json, char *value)
{
    DbItem *item;

    if ((item = rAllocType(DbItem)) == 0) {
        return 0;
    }
    if (json) {
        if (json->userFlags & USER_ALLOC) {
            //  Cleanup before insertion into the RB tree
            json->userFlags &= (uchar) ~USER_ALLOC;
        } else {
            /*
                Clone caller-owned props before storing them; deeply nested trees may fail to clone.
             */
            if ((json = jsonClone(json, 0)) == 0) {
                rFree(item);
                return 0;
            }
        }
        item->json = json;
    } else {
        item->value = value;
        item->allocatedValue = 1;
    }
    item->key = sclone(key);
    item->allocatedName = 1;
    return item;
}

/*
    Unlink before notifying, then free after callbacks. change() journals the removal.
 */
static void removeItem(Db *db, RbTree *index, DbModel *model, RbNode *rp, DbParams *params)
{
    DbItem *item;

    item = rbRemove(index, rp, 1);
    change(db, model, item, params, "remove", NULL);
    freeItem(db, item);
}

static void freeItem(Db *db, DbItem *item)
{
    (void) db;

    if (!item) return;

    if (item->allocatedName) {
        rFree(item->key);
        item->key = 0;
    }
    clearItem(item);
    rFree(item);
}

/*
    Validate required fields in the merged item.
 */
static int checkRequired(Db *db, DbModel *model, Json *json)
{
    RName   *np;
    DbField *field;

    if (!model) {
        return 0;
    }
    for (ITERATE_NAMES(model->fields, np)) {
        field = np->value;
        if (field->required && jsonGet(json, 0, np->name, 0) == 0) {
            return dberror(db, R_ERR_BAD_STATE, "Missing required property \"%s\"", np->name);
        }
    }
    return 0;
}

/*
    Check unique fields against other items. Trusted journal replay bypasses this check.
 */
static int checkUnique(Db *db, DbModel *model, Json *json, cchar *key, bool bypass)
{
    RName   *np;
    DbField *field;
    RbNode  *rp;
    DbItem  *item;
    cchar   *value, *existing;

    if (bypass || !model) {
        return 0;
    }
    for (ITERATE_NAMES(model->fields, np)) {
        field = np->value;
        if (!field->unique) {
            continue;
        }
        if ((value = jsonGet(json, 0, np->name, 0)) == 0) {
            continue;
        }
        for (rp = rbFirst(db->primary); rp; rp = rbNext(db->primary, rp)) {
            item = rp->data;
            if (smatch(item->key, key) || !smatch(dbField(item, db->type), model->name)) {
                continue;
            }
            existing = dbField(item, np->name);
            if (smatch(existing, value)) {
                return dberror(db, R_ERR_CANT_CREATE, "Value \"%s\" for unique field \"%s\" already exists",
                               value, np->name);
            }
        }
    }
    return 0;
}

/*
    Serialise an item value and refuse one that the store and journal cannot hold.
    Return the serialised value which the caller must free, or NULL with the error set.
 */
static char *checkSize(Db *db, cchar *key, Json *json)
{
    char *value;

    value = jsonToString(json, 0, 0, 0);
    if (slen(value) > DB_MAX_ITEM) {
        rFree(value);
        dberror(db, R_ERR_WONT_FIT, "Item \"%s\" exceeds the maximum size of %d bytes", key, DB_MAX_ITEM);
        return 0;
    }
    return value;
}

/*
    Take the serialised value of an item before it is modified in place, so that a rejected
    modification can be undone. The caller frees the snapshot or hands it to restoreItem.
 */
static char *snapshotItem(DbItem *item)
{
    char *snapshot;

    if (item->json) {
        return jsonToString(item->json, 0, 0, 0);
    }
    //  toJson discards the value when it parses it, so keep it as the snapshot
    snapshot = item->value;
    item->json = jsonParse(snapshot, 0);
    item->value = 0;
    item->allocatedValue = 0;
    return snapshot;
}

static void restoreItem(DbItem *item, char *snapshot)
{
    clearItem(item);
    item->value = snapshot;
    item->allocatedValue = 1;
}

static void clearItem(DbItem *item)
{
    if (item) {
        if (item->allocatedValue) {
            rFree(item->value);
            item->value = 0;
        }
        jsonFree(item->json);
        item->json = 0;
    }
}

/*
    Get the name of the index to use from the params.
 */
static cchar *getIndexName(Db *db, DbParams *params)
{
    (void) db;

    return (params && params->index) ? params->index : "primary";
}

/*
    Get the index to use for this API call. Currently only supports a primary index.
    Defaults to the primary if params is null.
 */
static RbTree *getIndex(Db *db, DbParams *params)
{
    (void) params;

    return db->primary;
}

/*
    Get the hash key index field name
 */
static cchar *getIndexHash(Db *db, cchar *index)
{
    char key[160];

    sfmtbuf(key, sizeof(key), "indexes.%s.hash", index);
    return jsonGet(db->schema, 0, key, 0);
}

/*
    Get the sort key index field name
 */
static cchar *getIndexSort(Db *db, cchar *index)
{
    char key[160];

    sfmtbuf(key, sizeof(key), "indexes.%s.sort", index);
    return jsonGet(db->schema, 0, key, 0);
}

PUBLIC cchar *dbGetSortKey(Db *db)
{
    return getIndexSort(db, getIndexName(db, NULL));
}

PUBLIC Json *dbStringToJson(cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    Json    *json;

    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    json = jsonParseKeep(buf, 0);
    if (json) {
        json->userFlags |= USER_ALLOC;
    }
    va_end(ap);
    return json;
}

/*
    Convert an array of properties into a json object to be used as props.
 */
PUBLIC Json *dbPropsToJson(cchar *props[])
{
    Json *json;
    int  i;

    json = jsonAlloc();
    for (i = 0; props[i]; i += 2) {
        if (props[i] && props[i + 1]) {
            jsonSet(json, 0, props[i], (void*) props[i + 1], 0);
        }
    }
    json->userFlags |= USER_ALLOC;
    return json;
}

/*
    Read an item from the on-disk database store (not the journal)
 */
static int readItem(FILE *fp, int format, DbItem **item)
{
    uint32_t length;
    char     key[DB_MAX_KEY];
    char     *data;

    if (!fp || !item) {
        return R_ERR_BAD_ARGS;
    }
    *item = 0;
    if (readLength(fp, format, &length) < 0) {
        //  EOF
        return 0;
    }
    if (length > (DB_MAX_KEY - 1)) {
        return R_ERR_BAD_STATE;
    }
    if (fread(key, (size_t) length, 1, fp) != 1) {
        return R_ERR_CANT_READ;
    }
    key[length] = '\0';

    if (readLength(fp, format, &length) < 0) {
        return R_ERR_CANT_READ;
    }
    if (length > DB_MAX_ITEM) {
        return R_ERR_BAD_STATE;
    }
    if ((data = rAlloc((size_t) length + 1)) == 0) {
        return R_ERR_MEMORY;
    }
    if (fread(data, (size_t) length, 1, fp) != 1) {
        return R_ERR_CANT_READ;
    }
    data[length] = '\0';
    if ((*item = allocItem(key, 0, data)) == 0) {
        rFree(data);
        return R_ERR_MEMORY;
    }
    return 0;
}

/*
    Persist an item to the on-disk store
 */
static int writeItem(FILE *fp, DbItem *item)
{
    char     *value;
    uint32_t keyLen, length;
    int      rc;

    /*
        Refuse to write a record that readItem would reject. Writing one silently truncates the
        database at that item on the next load. Both lengths are checked before anything is
        written, because a half written record corrupts the store just as badly.
     */
    keyLen = (uint32_t) slen(item->key);
    if (keyLen == 0 || keyLen > (DB_MAX_KEY - 1)) {
        return R_ERR_WONT_FIT;
    }
    value = item->json ? jsonToString(item->json, 0, 0, 0) : item->value;
    length = (uint32_t) slen(value);
    if (length == 0 || length > DB_MAX_ITEM) {
        if (value != item->value) {
            rFree(value);
        }
        return R_ERR_WONT_FIT;
    }
    rc = 0;
    if (writeLength(fp, keyLen) < 0 ||
        fwrite(item->key, (size_t) keyLen, 1, fp) != 1 ||
        writeLength(fp, length) < 0 ||
        fwrite(value, (size_t) length, 1, fp) != 1) {
        rc = R_ERR_CANT_WRITE;
    }
    if (value != item->value) {
        rFree(value);
    }
    return rc;
}

/*
    Get the persistency delay. API params override model delay.
 */
static Ticks getDelay(Db *db, DbModel *model, DbParams *params)
{
    Ticks delay;

    (void) db;

    delay = model->delay;
    if (params) {
        if (params->mem) {
            delay = DB_INMEM;
        } else if (params->delay) {
            delay = params->delay == DB_NODELAY ? 0 : params->delay;
        }
    }
    return delay;
}

/*
    React to a change. This handles persistency to the journal including delayed commits.
 */
static void change(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd, cchar *value)
{
    DbChange *change;
    Ticks    delay, when;
    int      events;

    if (params && params->bypass) {
        /*
            Journal replay. The change is already journalled and was notified before the
            restart, so it must neither be re-journalled via a delayed commit nor re-notified.
         */
        return;
    }
    delay = getDelay(db, model, params);
    if (delay > 0 && smatch(cmd, "remove")) {
        /*
            Journal removals before the caller frees the item.
         */
        delay = 0;
    }
    if (delay > 0) {
        /*
            Create a change record and schedule.
         */
        when = rGetTicks() + delay;
        change = rLookupName(db->changes, item->key);
        if (!change) {
            if ((change = allocChange(db, model, item, params, cmd, when)) == 0) {
                return;
            }
        } else if (when < change->due) {
            change->due = when;
        }
        item->delayed = 1;
        if (change->due < db->due || !db->due) {
            db->due = change->due;
            if (db->commitEvent) {
                rStopEvent(db->commitEvent);
            }
            db->commitEvent = rStartEvent((REventProc) commitChange, db, delay);
        }
        events = DB_ON_CHANGE;

    } else {
        if (delay != DB_INMEM && db->journal) {
            /*
                No delay, write to the journal unless in-mem
             */
            if (writeChangeToJournal(db, model, item, cmd, value) == 0 && item->delayed) {
                //  Discard the pending delayed change now that the item has been written
                if ((change = rLookupName(db->changes, item->key)) != 0) {
                    freeChange(db, change);
                }
                item->delayed = 0;
            }
        }
        events = DB_ON_CHANGE | DB_ON_COMMIT;
    }
    invokeCallbacks(db, model, item, params, cmd, events);
}

/*
    This commits due changes to the journal
 */
static void commitChange(Db *db)
{
    DbChange *change;
    DbItem   search, *item;
    DbModel  *model;
    DbParams params;
    RbNode   *rp;
    RList    *due;
    RName    *np;
    Time     nextDue, now;
    cchar    *cmd;
    char     *key;
    int      index;

    now = rGetTicks();

    /*
        Collect keys first, then recheck each change because callbacks may alter the pending set.
     */
    if ((due = rAllocList(0, R_TEMPORAL_VALUE)) == 0) {
        return;
    }
    for (ITERATE_NAME_DATA(db->changes, np, change)) {
        if (change->due <= now) {
            rPushItem(due, change->key);
        }
    }
    for (ITERATE_ITEMS(due, key, index)) {
        if ((change = rLookupName(db->changes, key)) == 0 || change->due > now) {
            continue;
        }
        model = change->model;
        params = change->params;
        cmd = change->cmd;
        /*
            Discard before notifying so callbacks can schedule a new change for the same item.
         */
        freeChange(db, change);

        search.key = key;
        if ((rp = rbLookup(db->primary, &search, NULL)) != 0) {
            item = rp->data;
            if (writeChangeToJournal(db, model, item, cmd, NULL) != 0) {
                // continue
            }
            item->delayed = 0;
            invokeCallbacks(db, model, item, &params, cmd, DB_ON_COMMIT);
        }
    }
    rFreeList(due);

    /*
        Reschedule from what remains, which must be computed after the callbacks have run.
        A callback that adds a delayed change cannot schedule its own event, because db->due
        is still set to this deadline and its change is not earlier. Computing nextDue before
        the callbacks would strand those changes with no timer.
     */
    nextDue = MAXINT64;
    for (ITERATE_NAME_DATA(db->changes, np, change)) {
        nextDue = min(nextDue, change->due);
    }
    if (nextDue == MAXINT64) {
        db->due = 0;
        db->commitEvent = 0;
    } else {
        db->due = nextDue;
        db->commitEvent = rStartEvent((REventProc) commitChange, db, max(nextDue - now, 0));
    }
}

/*
    Write a changed item to the journal and handle journal resets
    Errors are handled by low level I/O routines setting db->journalError
 */
static int writeChangeToJournal(Db *db, DbModel *model, DbItem *item, cchar *cmd, cchar *value)
{
    char *allocated;
    int  bufsize;

    //  Journal will be unset when booting and applying prior journal
    if (!db->journal) return 0;

    /*
        Compute the size of the change record as three null terminated strings.
        The string lengths are not read into the buffer. The caller may supply the value it has
        already serialised.
     */
    allocated = 0;
    if (!value) {
        value = item->json ? (allocated = jsonToString(item->json, 0, 0, 0)) : item->value;
    }

    /*
        readSize and readBlock refuse a record or block this large, so journalling one would
        make the next replay reject the journal as corrupt.
     */
    bufsize = (int) (slen(cmd) + slen(model->name) + slen(value) + 3);
    if (slen(value) > DB_MAX_ITEM || slen(item->key) > (DB_MAX_KEY - 1) || bufsize > DB_MAX_RECORD) {
        rError("db", "Item \"%s\" is too large to journal and has been omitted", item->key);
        rFree(allocated);
        return R_ERR_WONT_FIT;
    }

    //  These set journalError on errors - handled below
    writeSize(db, (uint32_t) bufsize);
    writeBlock(db, cmd);
    writeBlock(db, model->name);
    writeBlock(db, value);
    rFree(allocated);

    if (fflush(db->journal) < 0 || rFlushFile(fileno(db->journal)) < 0) {
        dberror(db, R_ERR_CANT_WRITE, "Cannot flush journal: %d", errno);
        db->journalError = 1;
    }
    return flushJournal(db);
}

static int flushJournal(Db *db)
{
    /*
        Save the journal to the database if it is full or if there is an error. While servicing,
        the save is deferred to the flushJournal call that ends the service pass.
     */
    if (db->journalError || db->journalSize >= db->maxJournalSize ||
        (rGetTicks() - db->journalCreated) >= db->maxJournalAge) {
        if (!db->servicing && dbSave(db, NULL) < 0) {
            return R_ERR_CANT_WRITE;
        }

    } else if (!db->journalEvent) {
        db->journalEvent = rStartEvent((REventProc) saveDb, db, db->maxJournalAge);
    }
    return db->journalError ? R_ERR_CANT_WRITE : 0;
}

static int writeSize(Db *db, uint32_t len)
{
    if (writeLength(db->journal, len) < 0) {
        db->journalError = 1;
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write to db journal file");
    }
    return 0;
}

/*
    Write the string including the trailing null
 */
static int writeBlock(Db *db, cchar *buf)
{
    uint32_t len;

    len = (uint32_t) (slen(buf) + 1);
    if (writeLength(db->journal, len) < 0) {
        db->journalError = 1;
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write to db journal file");
    }
    if (fwrite(buf, (size_t) len, 1, db->journal) != 1) {
        db->journalError = 1;
        return dberror(db, R_ERR_CANT_WRITE, "Cannot write to db journal file");
    }
    db->journalSize += sizeof(len) + (size_t) len;
    return 0;
}

/*
    Recreate the journal. Will close journal if already open.
 */
static int recreateJournal(Db *db)
{
    return openJournal(db, 0);
}

/*
    Open the journal. If append, preserve any existing records and add to them. Otherwise
    start a fresh journal. Append is used when a checkpoint save has failed and the journal
    is still the only record of those changes.
 */
static int openJournal(Db *db, bool append)
{
    struct stat sbuf;

    if (db->journal) {
        fclose(db->journal);
        db->journal = 0;
    }
    if (append && stat(db->journalPath, &sbuf) == 0 && sbuf.st_size > 0 && canAppendJournal(db)) {
        if ((db->journal = fopen(db->journalPath, "a")) == NULL) {
            return dberror(db, R_ERR_CANT_OPEN, "Cannot open database journal %s, errno %d",
                           db->journalPath, errno);
        }
        db->journalSize = (size_t) sbuf.st_size;
    } else {
        if ((db->journal = fopen(db->journalPath, "w")) == NULL) {
            return dberror(db, R_ERR_CANT_OPEN, "Cannot open database journal %s, errno %d",
                           db->journalPath, errno);
        }
        if (writeHeader(db->journal) < 0) {
            return dberror(db, R_ERR_CANT_WRITE, "Cannot write version to db journal file");
        }
        db->journalSize = 0;
    }
    db->journalCreated = rGetTicks();
    db->journalError = 0;
    return 0;
}

/*
    Apply the journal of changes to the database state
    Return the count of applied records, 0 if none, negative for errors.
    A journal that ends mid record is a torn write from a crash: the readable prefix is applied
    and the incomplete tail discarded. A record that could never have been written is corruption:
    the replay stops with an error so the caller does not truncate the journal over unread data.
 */
static int applyJournal(Db *db)
{
    struct stat sbuf;
    FILE        *fp;
    RBuf        *buf;
    cchar       *cmd, *model, *value;
    size_t      bufsize;
    int         count, format, rc;

    if (stat(db->journalPath, &sbuf) < 0 || sbuf.st_size == 0) {
        return 0;
    }
    if ((fp = fopen(db->journalPath, "r")) == NULL) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot open database journal %s, errno %d", db->journalPath, errno);
    }
    if ((rc = readHeader(db, fp, "database journal", &format)) < 0) {
        fclose(fp);
        return rc;
    }
    buf = rAllocBuf(ME_BUFSIZE);
    count = 0;
    while ((rc = readSize(db, fp, format, &bufsize)) > 0) {
        //  The buffer is flushed after each record, so it only needs room for the largest record
        if (rReserveBufSpace(buf, bufsize) < 0) {
            rc = dberror(db, R_ERR_MEMORY, "Cannot allocate database journal record of %d bytes",
                         (int) bufsize);
            break;
        }
        if ((rc = readBlock(db, fp, buf, format, &cmd)) <= 0 ||
            (rc = readBlock(db, fp, buf, format, &model)) <= 0 ||
            (rc = readBlock(db, fp, buf, format, &value)) <= 0) {
            break;
        }
        if (applyChange(db, cmd, model, value) < 0) {
            rc = R_ERR_CANT_READ;
            break;
        }
        count++;
        rFlushBuf(buf);
    }
    rFreeBuf(buf);
    fclose(fp);
    return rc < 0 ? rc : count;
}

/*
    Read the size of the next change record. Return 1 if a record follows, 0 at the end of the
    journal and a negative status if the size is corrupt.
 */
static int readSize(Db *db, FILE *fp, int format, size_t *sizep)
{
    uint32_t len;

    *sizep = 0;
    if (readLength(fp, format, &len) < 0) {
        return endOfJournal(db, fp);
    }
    //  writeChangeToJournal cannot write a record of this size, so the journal is corrupt
    if (len == 0 || len > DB_MAX_RECORD) {
        return dberror(db, R_ERR_CANT_READ, "Corrupt database journal record size %u", len);
    }
    *sizep = (size_t) len;
    return 1;
}

/*
    Read one null terminated string block of a change record. Return 1 if a block was read,
    0 if the journal ends mid record and a negative status if the block is corrupt.
 */
static int readBlock(Db *db, FILE *fp, RBuf *buf, int format, cchar **block)
{
    uint32_t len;

    *block = 0;
    //  The length includes a trailing null. Don't read the length into the buffer.
    if (readLength(fp, format, &len) < 0) {
        return endOfJournal(db, fp);
    }
    /*
        The record size grew the buffer, so a block that does not fit the remaining space is
        lying about its length. Never read past the buffer.
     */
    if (len == 0 || len > DB_MAX_ITEM || (size_t) len > rGetBufSpace(buf)) {
        return dberror(db, R_ERR_CANT_READ, "Corrupt database journal block length %u", len);
    }
    if (fread(rGetBufStart(buf), (size_t) len, 1, fp) != 1) {
        return endOfJournal(db, fp);
    }
    *block = rGetBufStart(buf);
    rAdjustBufEnd(buf, (ssize) len);
    rAdjustBufStart(buf, (ssize) rGetBufLength(buf));
    return 1;
}

/*
    A short read at the end of the journal is an incomplete record from an unclean shutdown.
    That change was never committed, so replay stops without error. A read error is reported.
 */
static int endOfJournal(Db *db, FILE *fp)
{
    if (ferror(fp)) {
        return dberror(db, R_ERR_CANT_READ, "Cannot read database journal %s, errno %d",
                       db->journalPath, errno);
    }
    return 0;
}

/*
    Read a database or journal file header and determine its record format.
 */
static int readHeader(Db *db, FILE *fp, cchar *what, int *format)
{
    uchar  buf[2];
    uint16 version;

    *format = DB_FORMAT_NATIVE;
    if (fread(buf, sizeof(buf), 1, fp) != 1) {
        return dberror(db, R_ERR_CANT_OPEN, "Cannot read %s header", what);
    }
    if (buf[0] == 'D' && buf[1] == 'B') {
        if (fread(buf, sizeof(buf), 1, fp) != 1) {
            return dberror(db, R_ERR_CANT_OPEN, "Cannot read %s header", what);
        }
        version = (uint16) (buf[0] | (buf[1] << 8));
        if (version != DB_VERSION) {
            return dberror(db, R_ERR_CANT_OPEN, "Incorrect %s version %d", what, version);
        }
        *format = DB_FORMAT_LE;
        return 0;
    }
    //  No magic. A legacy file holds the version in the byte order of the host that wrote it.
    memcpy(&version, buf, sizeof(version));
    if (version == DB_LEGACY_VERSION) {
        *format = DB_FORMAT_NATIVE;
    } else if (version == (DB_LEGACY_VERSION << 8)) {
        *format = DB_FORMAT_SWAPPED;
    } else {
        return dberror(db, R_ERR_CANT_OPEN, "Incorrect %s version %d", what, version);
    }
    return 0;
}

static int writeHeader(FILE *fp)
{
    uchar buf[4];

    buf[0] = 'D';
    buf[1] = 'B';
    buf[2] = DB_VERSION & 0xFF;
    buf[3] = (DB_VERSION >> 8) & 0xFF;
    return fwrite(buf, sizeof(buf), 1, fp) == 1 ? 0 : R_ERR_CANT_WRITE;
}

static int readLength(FILE *fp, int format, uint32 *length)
{
    uchar  buf[4];
    uint32 len;

    if (fread(buf, sizeof(buf), 1, fp) != 1) {
        return R_ERR_CANT_READ;
    }
    if (format == DB_FORMAT_LE) {
        len = (uint32) buf[0] | ((uint32) buf[1] << 8) | ((uint32) buf[2] << 16) | ((uint32) buf[3] << 24);
    } else {
        memcpy(&len, buf, sizeof(len));
        if (format == DB_FORMAT_SWAPPED) {
            len = (len << 24) | ((len & 0xFF00) << 8) | ((len >> 8) & 0xFF00) | (len >> 24);
        }
    }
    *length = len;
    return 0;
}

static int writeLength(FILE *fp, uint32 length)
{
    uchar buf[4];

    buf[0] = length & 0xFF;
    buf[1] = (length >> 8) & 0xFF;
    buf[2] = (length >> 16) & 0xFF;
    buf[3] = (length >> 24) & 0xFF;
    return fwrite(buf, sizeof(buf), 1, fp) == 1 ? 0 : R_ERR_CANT_WRITE;
}

/*
    New records are little endian, so they can only follow records of the same byte order.
    A legacy journal on a big endian host is recreated instead. Its records were replayed and
    are in memory, but they have no durable record until the next save succeeds.
 */
static bool canAppendJournal(Db *db)
{
    FILE   *fp;
    uint16 native;
    int    format, rc;

    if ((fp = fopen(db->journalPath, "r")) == NULL) {
        return 0;
    }
    rc = readHeader(db, fp, "database journal", &format);
    fclose(fp);
    if (rc < 0) {
        return 0;
    }
    native = 1;
    if (format == DB_FORMAT_LE || (format == DB_FORMAT_NATIVE && *(uchar*) &native == 1)) {
        return 1;
    }
    rError("db", "Cannot append to the legacy journal %s, recreating it", db->journalPath);
    return 0;
}

static int applyChange(Db *db, cchar *cmd, cchar *model, cchar *value)
{
    Json *json;

    if ((json = jsonParse(value, 0)) == 0) {
        return dberror(db, R_ERR_CANT_READ, "Cannot parse json from journal file");
    }
    if (json->count == 0) {
        jsonFree(json);
        return dberror(db, R_ERR_CANT_READ, "Empty json from journal file");
    }
    if (sends(cmd, "create")) {
        dbCreate(db, model, json, DB_PARAMS(.bypass = 1));

    } else if (sends(cmd, "remove")) {
        dbRemove(db, model, json, DB_PARAMS(.bypass = 1));

    } else if (sends(cmd, "update")) {
        dbUpdate(db, model, json, DB_PARAMS(.bypass = 1));

    } else if (sends(cmd, "upsert")) {
        dbUpdate(db, model, json, DB_PARAMS(.bypass = 1, .upsert = 1));
    }
    jsonFree(json);
    return 0;
}

PUBLIC void dbAddContext(Db *db, cchar *name, cchar *value)
{
    jsonSet(db->context, 0, name, value, 0);
}

PUBLIC void dbAddCallback(Db *db, DbCallbackProc proc, cchar *model, void *arg, int events)
{
    DbCallback *cb;

    if ((cb = rAllocType(DbCallback)) == 0) {
        rError("db", "Cannot add callback");
        return;
    }
    cb->proc = proc;
    cb->arg = arg;
    cb->model = model ? sclone(model) : NULL;
    cb->events = events;
    rAddItem(db->callbacks, cb);
}

PUBLIC void dbRemoveCallback(Db *db, DbCallbackProc proc, cchar *model, void *arg)
{
    DbCallback *cb;
    int        ci;

    for (ITERATE_ITEMS(db->callbacks, cb, ci)) {
        if (!cb->removed && cb->proc == proc && cb->arg == arg &&
            ((model == NULL && cb->model == NULL) || (model && cb->model && smatch(model, cb->model)))) {
            /*
                Defer removal until the outermost callback iteration completes.
             */
            cb->removed = 1;
            if (db->callbackDepth == 0) {
                sweepCallbacks(db);
            }
            break;
        }
    }
}

static void invokeCallbacks(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                            int event)
{
    DbCallback *cb;
    int        ci;

    db->callbackDepth++;
    for (ITERATE_ITEMS(db->callbacks, cb, ci)) {
        if (cb->removed) {
            continue;
        }
        if (!cb->model || smatch(cb->model, model->name)) {
            if (cb->events & event) {
                cb->proc(cb->arg, db, model, item, params, cmd, event);
            }
        }
    }
    if (--db->callbackDepth == 0) {
        sweepCallbacks(db);
    }
}

/*
    Compact removed callbacks only when no invocation is active.
 */
static void sweepCallbacks(Db *db)
{
    DbCallback *cb;
    int        ci;

    for (ci = db->callbacks->length - 1; ci >= 0; ci--) {
        cb = R_GET_ITEM(db->callbacks, ci);
        if (cb->removed) {
            rFree(cb->model);
            rRemoveItemAt(db->callbacks, ci);
        }
    }
}

PUBLIC void dbSetJournalParams(Db *db, Ticks delay, size_t maxSize)
{
    db->maxJournalAge = delay;
    db->maxJournalSize = maxSize;
}

static void addContext(Db *db, Json *props)
{
    jsonBlend(props, 0, 0, db->context, 0, 0, 0);
}

/*
    Compare the sort key of an item.
    This is the Red/black index comparision callback.
    Env contains the API environment - (beginsWith).
 */
static int compareItems(cvoid *n1, cvoid *n2, Env *env)
{
    DbItem *d1, *d2;

    if (!n1 || !n2) {
        return R_ERR_BAD_ARGS;
    }
    d1 = (DbItem*) n1;
    d2 = (DbItem*) n2;

    if (d1->key == NULL) {
        if (d2->key == NULL) {
            return 0;
        } else {
            return -1;
        }
    } else if (d2->key == NULL) {
        return 1;
    }
    if (env && env->compare && smatch(env->compare, "begins")) {
        //  The d1->key length was precomputed in searchLen
        return sncmp(d1->key, d2->key, env->searchLen);
    }
    return scmp(d1->key, d2->key);
}

PUBLIC cchar *dbGetError(Db *db)
{
    if (!db) {
        return NULL;
    }
    return db->error;
}

/*
    Check a value is one of the valid enum values
 */
static bool checkEnum(DbField *field, cchar *value)
{
    cchar *enumValue;
    int   next;

    if (!field || !field->enums || !value) {
        return 0;
    }
    for (ITERATE_ITEMS(field->enums, enumValue, next)) {
        if (smatch(enumValue, value)) {
            return 1;
        }
    }
    return 0;
}

static cchar *LETTERS = "0123456789ABCDEFGHJKMNPQRSTVWXYZZ";

/*
    Generate a ULID identifier. Universal sortable ID.
 */
PUBLIC char *dbGetULID(Time when)
{
    char bytes[16 + 1], time[10 + 1];
    Time mark;
    int  i, index, length, mod;

    length = (int) slen(LETTERS) - 1;
    if (cryptGetRandomBytes((uchar*) bytes, sizeof(bytes), 1) < 0) {
        return 0;
    }
    for (i = 0; i < (int) sizeof(bytes) - 1; i++) {
        index = (int) floor((uchar) bytes[i] * length / 0xFF);
        bytes[i] = LETTERS[index];
    }
    bytes[sizeof(bytes) - 1] = '\0';

    mark = when;
    for (i = sizeof(time) - 2; i >= 0; i--) {
        mod = (int) (mark % length);
        time[i] = LETTERS[mod];
        mark = (mark - mod) / length;
    }
    time[sizeof(time) - 1] = '\0';
    return sjoin(time, bytes, 0);
}

PUBLIC char *dbGetUID(size_t size)
{
    return cryptID(size);
}

/*
    Load data from a JSON file -- Useful for development and initial migrations
 */
PUBLIC int dbLoadData(Db *db, cchar *path)
{
    Json *json;
    char *errorMsg;
    int  rc;

    if ((json = jsonParseFile(path, &errorMsg, 0)) == 0) {
        dberror(db, R_ERR_CANT_READ, "%s", errorMsg);
        rFree(errorMsg);
        return R_ERR_CANT_READ;
    }
    rc = dbLoadDataItems(db, json, NULL);
    jsonFree(json);
    return rc;
}

PUBLIC int dbLoadDataItems(Db *db, Json *json, JsonNode *parent)
{
    Json     *props;
    JsonNode *item, *model;
    char     *str;
    int      rc;

    for (ITERATE_JSON(json, parent, model, mid)) {
        for (ITERATE_JSON(json, model, item, id)) {
            str = jsonToString(json, id, 0, 0);
            props = jsonParse(str, 0);
            rc = 0;
            if (!dbFindOne(db, model->name, props, NULL) && dbCreate(db, model->name, props, NULL) == 0) {
                rc = dberror(db, R_ERR_CANT_COMPLETE, "Cannot create item for %s. %s", str, db->error);
            }
            jsonFree(props);
            rFree(str);
            if (rc < 0) {
                return rc;
            }
        }
    }
    return 0;
}

PUBLIC void dbReset(cchar *path)
{
    char pbuf[ME_MAX_FNAME];

    unlink(path);
    unlink(sfmtbuf(pbuf, sizeof(pbuf), "%s.jnl", path));
}

PUBLIC cchar *dbType(Db *db)
{
    return db->type;
}

PUBLIC cchar *dbNext(Db *db, RList *list)
{
    (void) db;

    if (list->length == 0) {
        return 0;
    }
    return dbField(list->items[list->length - 1], "sk");
}

static DbChange *allocChange(Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                             Ticks due)
{
    DbChange *change;

    if ((change = rAllocType(DbChange)) == 0) {
        return 0;
    }
    change->db = db;
    change->model = model;
    if (params) {
        change->params = *params;
    }
    change->cmd = cmd;
    change->due = due;
    /*
        Index by the change's own key. The item may be removed and freed while the change is
        still pending.
     */
    change->key = sclone(item->key);
    rAddName(db->changes, change->key, change, R_STATIC_NAME);
    return change;
}

static void freeChange(Db *db, DbChange *change)
{
    rRemoveName(db->changes, change->key);
    rFree(change->key);
    rFree(change);
}

/*
    Set and log an error. Emits a message at the "trace" level.
 */
static int dberror(Db *db, int code, cchar *fmt, ...)
{
    va_list ap;
    char    *msg;

    if (!db || !code || !fmt) {
        return R_ERR_BAD_ARGS;
    }
    va_start(ap, fmt);
    msg = sfmtv(fmt, ap);
    db->code = code;

    rFree(db->error);
    db->error = msg;

    rTrace("db", "%s", msg);
    va_end(ap);
    return code;
}

#else
void dummyDb(){}
#endif /* ME_COM_DB */
