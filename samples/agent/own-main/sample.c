/*
    own-main/sample.c - Sample using Ioto as a library and provide own main()

    This sample provides its own main().
 */

/********************************** Includes **********************************/

#include "ioto.h"

/************************************* Code ***********************************/

int main(int argc, char **argv)
{
    if (rInit(ioInit, NULL, 0) < 0) {
        fprintf(stderr, "Cannot initialize runtime");
        exit(1);
    }
    //  Block until instructed to exit
    rServiceEvents();

    ioTerm();
    rTerm();
    return 0;
}

/*
    Start processing. This runs in a fiber.
*/
PUBLIC int ioStart()
{
    rInfo("sample", "Hello World\n");
    //  Your code here
    return 0;
}

PUBLIC void ioStop(void) {}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
