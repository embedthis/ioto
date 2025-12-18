/*
    app.c - Include the selected app
 */

/********************************** Includes **********************************/

#include "ioto.h"

#define STRINGIFY(x) #x
#define HEADER(x)    STRINGIFY(x)

#ifndef APP
    #define APP unit
#endif

/* *INDENT-OFF* */
#include HEADER(../apps/APP/src/APP.c)
/* *INDENT-ON* */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
