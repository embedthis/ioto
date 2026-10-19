/*
    route-trim.tst.c - A route "trim" rewrite must not confer authority

    A route that declares "trim" rewrites the request path after its own role gate has run, and
    every later stage - the file handler's document root join and the action dispatch - uses the
    rewritten value. A public trim route was therefore an unauthenticated rewrite into any other
    route's namespace. These tests drive both handler paths through the public /pub/ and /pubf/
    trim routes and require the protected target's refusal, while the shipped /trace/ and /stream/
    trim routes must keep working for their own namespaces.

    Also covers action dispatch: a short prefix registered first must not answer for a longer
    action registered after it, and a prefix must match on whole path segments.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

//  A line unique to site/admin/secret.html: its presence proves the document was served
#define ADMIN_SECRET "you have admin privileges"

/************************************ Code ************************************/

/*
    Fetch a path from the shared server and check the status. Returns the response body, or "".
 */
static cchar *get(Url *up, cchar *path, int expected, cchar *why)
{
    char url[256];
    int  status;

    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, path), NULL, 0, NULL);
    teqi(status, expected, "%s", why);
    return urlGetResponse(up) ? urlGetResponse(up) : "";
}

/*
    The file handler joins the document root with the rewritten path, so a public trim route must
    not reach a role-protected document tree.
 */
static void testTrimIntoProtectedFiles(void)
{
    Url   *up;
    cchar *body;

    up = urlAlloc(0);
    body = get(up, "/admin/secret.html", 401, "The admin route challenges an anonymous request");
    ttrue(!scontains(body, ADMIN_SECRET), "The admin document must not be served");

    body = get(up, "/pubf/admin/secret.html", 401,
               "A public trim route must not reach the admin document tree");
    ttrue(!scontains(body, ADMIN_SECRET), "The admin document must not be served through a trim route");
    urlFree(up);
}

/*
    The action dispatch runs over the rewritten path against the global action list, so a public
    trim route must not reach an action in a role-protected namespace.
 */
static void testTrimIntoProtectedActions(void)
{
    Url *up;

    up = urlAlloc(0);
    get(up, "/test/admin/show", 401, "The admin action route challenges an anonymous request");
    get(up, "/pub/test/admin/show", 401, "A public trim route must not reach an admin action");
    urlFree(up);
}

/*
    A trim into an unprotected namespace is the feature working as documented and must be untouched.
 */
static void testTrimIntoPublicNamespace(void)
{
    Url   *up;
    cchar *body;

    up = urlAlloc(0);
    get(up, "/pubf/index.html", 200, "A trim into the public document root still serves");
    body = get(up, "/pub/test/success", 200, "A trim into a public action namespace still runs");
    tcontains(body, "success");
    urlFree(up);
}

/*
    The trim routes shipped in the test configuration must keep serving their own namespaces.
 */
static void testShippedTrimRoutes(void)
{
    Url   *up;
    cchar *body;

    up = urlAlloc(0);
    get(up, "/trace/index.html", 200, "The /trace/ trim route still serves the document root");
    body = get(up, "/stream/test/success", 200, "The /stream/ trim route still reaches its action");
    tcontains(body, "success");
    urlFree(up);
}

/*
    Action dispatch selects the longest matching prefix on whole path segments. "/test/shadow" is
    registered before "/test/shadow/deep", so a first-match dispatch would answer for both.
 */
static void testActionShadowing(void)
{
    Url   *up;
    cchar *body;

    up = urlAlloc(0);
    body = get(up, "/test/shadow", 200, "The short action answers for its own path");
    tcontains(body, "success");

    body = get(up, "/test/shadow/deep", 200, "The longer action must not be shadowed");
    ttrue(!scontains(body, "success"), "A shorter prefix must not answer for a longer action");
    tcontains(body, "route");

    get(up, "/test/shadowy", 404, "An action prefix must match on whole path segments");
    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testTrimIntoProtectedFiles();
        testTrimIntoProtectedActions();
        testTrimIntoPublicNamespace();
        testShippedTrimRoutes();
        testActionShadowing();
    }
    rFree(HTTP);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
