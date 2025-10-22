/*
    auth.c.tst - Unit tests for authentication utilities (basic functionality)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void testBasicAuthInit()
{
    // Test basic web server initialization and cleanup
    // This tests the auth module is properly linked and functional
    webInit();
    webTerm();
}

static void fiberMain(void *arg)
{
    testBasicAuthInit();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */