/*
    app.c - Include the selected app
 */

/********************************** Includes **********************************/

#include "ioto.h"
#include "config.h"

#define STRINGIFY(x) #x
#define HEADER(x)    STRINGIFY(x)

/* choose one: -DAPP=demo  (no quotes) */
#ifndef APP
#define APP demo
#endif

/* *INDENT-OFF* */
#include HEADER(../apps/APP/src/APP.c)
/* *INDENT-ON* */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
