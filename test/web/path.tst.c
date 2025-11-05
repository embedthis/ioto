/*
    path.c.tst - Unit tests for web server path management

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static void normalize()
{
    char *path;

    path = webNormalizePath(NULL);
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath(" ");
    tmatch(path, " ");
    rFree(path);

    path = webNormalizePath("/");
    tmatch(path, "/");
    rFree(path);

    path = webNormalizePath("//");
    tmatch(path, "/");
    rFree(path);

    path = webNormalizePath(".");
    tmatch(path, ".");
    rFree(path);

    path = webNormalizePath("..");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("...");
    tmatch(path, "...");
    rFree(path);

    path = webNormalizePath("/index.html");
    tmatch(path, "/index.html");
    rFree(path);

    path = webNormalizePath("index.html");
    tmatch(path, "index.html");
    rFree(path);

    path = webNormalizePath("about/index.html");
    tmatch(path, "about/index.html");
    rFree(path);

    path = webNormalizePath("./about/index.html");
    tmatch(path, "about/index.html");
    rFree(path);

    path = webNormalizePath("about/../index.html");
    tmatch(path, "index.html");
    rFree(path);

    path = webNormalizePath("../about/index.html");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("./index.html");
    tmatch(path, "index.html");
    rFree(path);

    path = webNormalizePath("about/./index.html");
    tmatch(path, "about/index.html");
    rFree(path);

    path = webNormalizePath("/a/");
    tmatch(path, "/a/");
    rFree(path);

    path = webNormalizePath("a/b/..");
    tmatch(path, "a");
    rFree(path);

    path = webNormalizePath("a/b/.");
    tmatch(path, "a/b");
    rFree(path);

    path = webNormalizePath("a/b/./");
    tmatch(path, "a/b/");
    rFree(path);

    path = webNormalizePath("a/b/./");
    tmatch(path, "a/b/");
    rFree(path);

    path = webNormalizePath("a/.");
    tmatch(path, "a");
    rFree(path);

    path = webNormalizePath("./a");
    tmatch(path, "a");
    rFree(path);

    path = webNormalizePath("/../");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("../../");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("../a");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z");
    tmatch(path, "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z");
    rFree(path);
}

static void normalizeExtras()
{
    char *path;

    // More complex traversals
    path = webNormalizePath("a/b/../../c");
    tmatch(path, "c");
    rFree(path);

    path = webNormalizePath("/a/b/../../c");
    tmatch(path, "/c");
    rFree(path);

    path = webNormalizePath("a/../../c");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("a/b/../c/../d");
    tmatch(path, "a/d");
    rFree(path);

    // Edge cases with trailing slashes and dots
    path = webNormalizePath("a/b/../");
    tmatch(path, "a/");
    rFree(path);

    path = webNormalizePath("/a/b/../");
    tmatch(path, "/a/");
    rFree(path);

    path = webNormalizePath("./");
    tmatch(path, ".");
    rFree(path);

    path = webNormalizePath("../");
    tmatch(path, NULL);
    rFree(path);

    // Filenames that look like traversals but are not
    path = webNormalizePath("..a");
    tmatch(path, "..a");
    rFree(path);

    path = webNormalizePath("a..");
    tmatch(path, "a..");
    rFree(path);

    path = webNormalizePath("a/..b/c");
    tmatch(path, "a/..b/c");
    rFree(path);

    // Root edge cases
    path = webNormalizePath("/..");
    tmatch(path, NULL);
    rFree(path);

    path = webNormalizePath("/.");
    tmatch(path, "/");
    rFree(path);
}

static void validatePath()
{
    ttrue(!webValidatePath(NULL));
    ttrue(!webValidatePath(""));
    ttrue(webValidatePath("index.html"));
    ttrue(webValidatePath("about/index.html"));
    ttrue(webValidatePath("about/index.html/"));
    ttrue(webValidatePath("about/../index.html"));
    ttrue(webValidatePath("@@index.html"));
    ttrue(webValidatePath("[index.html]"));

    ttrue(!webValidatePath("  index.html"));
    ttrue(!webValidatePath("index  .html"));
    ttrue(!webValidatePath("index.html  "));
    ttrue(!webValidatePath("<script index.html"));
    ttrue(!webValidatePath("^index.html"));
    webTerm();
}

static void fiberMain(void *data)
{
    if (setup(NULL, NULL)) {
        webInit();
        normalize();
        normalizeExtras();
        validatePath();
        webTerm();
    }
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
