/*
    aws-s3/sample.c - Send a file to S3
 */

/********************************** Includes **********************************/

#include "ioto.h"

/************************************* Code ***********************************/

PUBLIC int ioStart(void)
{
    if (awsPutFileToS3("us-east-1", "bucket", "README.md", "README.md") < 0) {
        rError("aws-s3", "Cannot put file to S3");
    } else {
        rInfo("aws-s3", "File uploaded");
    }
    //  Exit sample
    rStop();
    return 0;
}

PUBLIC void ioStop(void) {}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
