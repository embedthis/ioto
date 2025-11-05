/*
    command.c.tst - Unit tests for the URL command

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "../test.h"

/*********************************** Locals ***********************************/

static cchar    *HTTP;
static cchar    *HTTPS;

/*********************************** Locals ***********************************/

static cchar    *HTTP;
static cchar    *HTTPS;

/************************************ Code ************************************/

static void printable()
{
    char    *output, cmd[128];
    int     status;

    status = rRun(SFMT(cmd, "url -s b --printable %s/index.html", HTTP), &output);
    teq(status, 0);
    tcontains(output, "Hello /index");
    rFree(output);
}

static void jsonItem()
{
    Json    *json;
    char    *output, cmd[128];
    int     status;

    status = rRun(SFMT(cmd, "url -s b %s/test/show '{one: 1}'", HTTP), &output);
    teq(status, 0);
    json = jsonParse(output, 0);
    // jsonPrint(json);
    tmatch(jsonGet(json, 0, "form.one", 0), "1");
    jsonFree(json);
    rFree(output);
}

static void fileItem()
{
    Json    *json;
    char    *output, cmd[128];
    int     status;

    status = rRun(SFMT(cmd, "url -s b %s/test/show @../data/one-line.dat", HTTP), &output);
    teq(status, 0);
    json = jsonParse(output, 0);
    tmatch(jsonGet(json, 0, "body", 0), "one-line");
    jsonFree(json);
    rFree(output);
}

static void redirect()
{
    char    *output, cmd[128];
    int     status;

    // with follow
    status = rRun(SFMT(cmd, "url -s b %s/dir", HTTP), &output);
    teq(status, 0);
    tcontains(output, "dir/index");
    rFree(output);

    // no follow
    status = rRun(SFMT(cmd, "url -H --trace stdout --redirects 0 %s/dir", HTTP), &output);
    teq(status, 0);
    tcontains(output, "301 Redirect");
    rFree(output);
}

static void upload()
{
}

static void fiberMain()
{
    if (setup(&HTTP, &HTTPS)) {
        printable();
        jsonItem();
        fileItem();
        redirect();
        upload();
    }
    rStop();
}

int main(int argc, char **argv)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
