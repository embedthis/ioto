

/*
    upload.c.tst - Unit tests for file upload

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

static int upload(cchar *srcPath);

/************************************ Code ************************************/

static void uploadBasic(void)
{
    Url   *up;
    RList *files;
    RHash *forms;
    Json  *json;
    char  url[128];
    int   rc;

    up = urlAlloc(0);
    files = rAllocList(0, 0);
    rAddItem(files, "site/data/test1.txt");

    forms = rAllocHash(0, 0);
    rAddName(forms, "color", "blue", 0);

    rc = urlStart(up, "POST", SFMT(url, "%s/test/upload", HTTP));
    teqi(rc, 0);

    rc = urlUpload(up, files, forms, NULL);
    teqi(rc, 0);
    urlFinalize(up);
    teqi(urlGetStatus(up), 200);

    json = urlGetJsonResponse(up);
    jsonPrint(json);
    tmatch(jsonGet(json, 0, "uploads[0].clientFilename", 0), "test1.txt");

    jsonFree(json);
    rFree(forms);
    rFree(files);
    urlFree(up);
}

static void uploadBig(void)
{
    upload("site/size/100K.txt");
}

static int upload(cchar *srcPath)
{
    Url   *up;
    RList *files;
    Json  *json;
    cchar *destPath;
    ssize srcSize, destSize;
    FILE  *fp1, *fp2;
    char  url[128];
    int   rc, ch1, ch2;
    bool  match;

    match = 1;
    up = urlAlloc(0);
    files = rAllocList(0, 0);
    rAddItem(files, srcPath);

    if ((rc = urlStart(up, "POST", SFMT(url, "%s/test/upload", HTTP))) != 0) {
        urlFree(up);
        return rc;
    }
    teqi(rc, 0);

    if ((rc = urlUpload(up, files, NULL, NULL)) != 0) {
        urlFree(up);
        return 0;
    }
    teqi(rc, 0);
    urlFinalize(up);
    if (urlGetStatus(up) != 200) {
        teqi(urlGetStatus(up), 200);
        urlFree(up);
        return R_ERR_BAD_STATE;
    }

    if ((json = urlGetJsonResponse(up)) == NULL) {
        urlFree(up);
        return R_ERR_BAD_STATE;
    }
    tnotnull(json);
    tmatch(jsonGet(json, 0, "uploads[0].clientFilename", 0), rBasename(srcPath));
    srcSize = rGetFileSize(srcPath);
    teqz(srcSize, jsonGetInt(json, 0, "uploads[0].size", 0));

    /*
        Verify
     */
    destPath = rJoinFile("./tmp", rBasename(srcPath));
    destSize = rGetFileSize(destPath);
    teqz(srcSize, destSize);

    fp1 = fopen(srcPath, "r");
    fp2 = fopen(destPath, "r");
    tnotnull(fp1);
    tnotnull(fp2);

    while ((ch1 = fgetc(fp1)) != EOF) {
        ch2 = fgetc(fp2);
        if (ch1 != ch2) {
            match = 0;
            break;
        }
    }
    if (ch1 != EOF || fgetc(fp2) != EOF) {
        match = 0;
    }
    ttrue(match);
    unlink(destPath);

    fclose(fp1);
    fclose(fp2);
    jsonFree(json);
    rFree(files);
    urlFree(up);

    if (!match) {
        return R_ERR_BAD_STATE;
    }
    return 0;
}

static char *createFile(ssize size)
{
    FILE *fp;
    char *path;
    int  i;
    char c;

    path = rGetTempFile("./tmp", "upload");
    if ((fp = fopen(path, "wb")) == NULL) {
        tnotnull(fp);
        return 0;
    }
    c = 0;
    for (i = 0; i < size; i++) {
        fputc(c, fp);
    }
    fclose(fp);
    return path;
}


static void uploadBoundary(void)
{
    char *path;
    int  i;

    //  Test boundary conditions with smaller file sizes to avoid timeout on Windows
    for (i = 0; i < 4 * 1024; i += rand() % 149) {
        if ((path = createFile(i)) == NULL) {
            break;
        }
        if (upload(path) < 0) {
            twrite("Cant upload %s", path);
            break;
        }
        rFree(path);
    }
}


static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        uploadBasic();
        uploadBig();
        uploadBoundary();
    }
    rFree(HTTP);
    rFree(HTTPS);
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
