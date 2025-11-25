/*
    alloc.c.tst - Unit tests for alloc/free

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void createUrl()
{
    Url    *up;

    up = urlAlloc(URL_NO_LINGER);
    ttrue(up != 0);
    urlFree(up);
}

static void fiberMain(void *data)
{
    createUrl();
    rStop();
}


int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
