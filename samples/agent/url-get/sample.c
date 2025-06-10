/*
    url-get/sample.c - Get a file from www.example.com

    This sample demonstrates the urlGet API to fetch a URL from example.com.
 */

/********************************** Includes **********************************/

#include "url.h"

/************************************* Code ***********************************/

PUBLIC int ioStart(void)
{
    char   *data;

    data = urlGet("http://www.example.com/", NULL);
    printf("Got response:\n%s\n", data);
    rFree(data);

    //  Exit sample
    rStop();
    return 0;
}

PUBLIC void ioStop(void) {}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
