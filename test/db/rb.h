/*
    rb.h - Shim so db's vendored tests compile against the amalgamation

    In the db module, the red/black tree API lives in its own src/rb.h and the tests include it by
    that name. The published db.h is a catenation of rb.h and db.h, so the agent ships the same API
    under one filename and has no rb.h of its own.

    This redirect lets rb.tst.c and rbtree.tst.c be vendored verbatim rather than patched on the
    way in. It is agent-owned: bin/sync-tests.sh never writes it, because db does not track a file
    at this path.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#include    "db.h"

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
