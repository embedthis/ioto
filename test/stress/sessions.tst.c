/*
    sessions.tst.c - Logins past the session limit, and what the table does about it

    A soak logs out after every login, so it never presses the limit. This does: every login is
    a fresh authenticated session that is never released, more of them than web.limits.sessions
    allows, and the count is scaled by depth. The table must refuse rather than grow - an
    authenticated session cannot be evicted for a newcomer, so once the table is full the
    login is refused - and a single logout must make room for exactly one more.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

/*********************************** Locals ***********************************/

//  Logins per depth. All exceed the limit; the higher depths exceed it many times over.
static const int COUNTS[] = { 25, 40, 60, 100, 200, 400, 800, 1600, 3200, 6400 };

//  web.limits.sessions in test/utils/agent/web.json5
#define SESSION_LIMIT 20

/************************************ Code ************************************/

static void testSessionLimit(void)
{
    Stats stats;
    char  *cookie, *first, *header, url[256];
    int   depth, count, i, accepted, refused, peak, baseline;

    depth = tdepth();
    depth = depth < 0 ? 0 : (depth > 9 ? 9 : depth);
    count = COUNTS[depth];

    ttrue(getStats(&stats));
    baseline = stats.sessions;
    tinfo("  %d sessions live before the run, limit %d, %d logins at depth %d", baseline, SESSION_LIMIT, count, tdepth());

    accepted = refused = 0;
    peak = baseline;
    first = NULL;
    for (i = 0; i < count; i++) {
        if ((cookie = login("guest", FIXTURE_PASSWORD)) != 0) {
            accepted++;
            if (!first) {
                first = cookie;
            } else {
                rFree(cookie);
            }
        } else {
            refused++;
        }
        if ((i % 10) == 9 && getStats(&stats) && stats.sessions > peak) {
            peak = stats.sessions;
        }
    }
    ttrue(getStats(&stats));
    if (stats.sessions > peak) {
        peak = stats.sessions;
    }
    tinfo("  %d accepted, %d refused, peak %d sessions", accepted, refused, peak);

    //  The table filled to the limit and then refused every further login
    ttrue(accepted == SESSION_LIMIT - baseline);
    ttrue(refused == count - accepted);
    ttrue(peak <= SESSION_LIMIT);
    ttrue(stats.sessions == SESSION_LIMIT);

    //  Releasing one session makes room for exactly one more
    ttrue(first != NULL);
    header = cookieHeader(first);
    ttrue(expect("POST", SFMT(url, "%s/api/user/logout", HTTP), NULL, 0, header, 302));
    ttrue(getStats(&stats));
    ttrue(stats.sessions == SESSION_LIMIT - 1);
    cookie = login("guest", FIXTURE_PASSWORD);
    ttrue(cookie != NULL);
    rFree(cookie);
    ttrue(login("guest", FIXTURE_PASSWORD) == NULL);
    ttrue(getStats(&stats));
    ttrue(stats.sessions == SESSION_LIMIT);
    rFree(header);
    rFree(first);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testSessionLimit();
    }
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
