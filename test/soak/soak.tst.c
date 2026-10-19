/*
    soak.tst.c - A sustained mixed workload through the assembled agent, and what its resource
    use does across it

    Every other test in the suite links one module into the test binary or starts the web
    command. This is the only place the database, web server, url client and the unit app's
    actions run together as one process for a duration, which is the only place their
    interaction can leak. What it catches is the class of defect that reaches the operating
    system: a descriptor never closed, a session or journal entry never reaped, a buffer that
    grows per request. Those are the ones that take a device down after a week.

    WHAT IS AND IS NOT MEASURED. The figures are the operating system's view of the process -
    resident set and open descriptors, reported by the unit app's stats fixture from task_info
    and /proc rather than scraped from ps, so the soak runs the same on every platform and inside
    sandboxes that refuse ps - plus two the agent alone has: the size of the database and its
    journal, and the live session count. It cannot see inside the R allocator, and a clean result
    is not a statement that the agent does not leak. test/web/leak records the same limit from the
    other side for the web module alone. The agent spawns no processes, so unlike the Appweb and
    GoAhead soaks there is no child count.

    WHY BOUNDED GROWTH AND NOT ZERO. Caches fill, the session table populates, the journal sits
    anywhere below its checkpoint size, and an allocator's high-water mark rises and stays risen.
    All are correct and all make the figures grow, then plateau. A zero-growth assertion would be
    flaky and would eventually be "fixed" by deleting it. A ratio is a real threshold: it fails on
    a leak and passes on a warm-up.

    The raw figures are printed every run, whatever the verdict. A pass that reports nothing is
    not evidence, and evidence is what this test exists to produce.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

/*********************************** Locals ***********************************/

/*
    Seconds of workload per depth. Depth 0 has to stay inside the ordinary suite's budget, so it
    is a smoke test of the harness rather than a soak; the real runs are depth 3 and above,
    invoked by hand. The plateau this is looking for takes at least a minute to establish.
 */
static const int SECONDS[] = { 3, 10, 30, 60, 120, 240, 480, 900, 1800, 3600 };

/*
    Resident memory may grow by this much across the run before it is called a leak. Generous
    on purpose: the first requests populate caches and the session table, and the allocator's
    high-water mark never comes back down. A real leak under this workload is not subtle - it is
    linear in request count, and at these rates that is orders of magnitude, not 60%.
 */
#define RSS_GROWTH_LIMIT 1.6

//  Descriptors must not grow at all beyond a small allowance for pooled connections
#define FD_GROWTH_LIMIT 24

/*
    The database plus its journal may sit anywhere below the checkpoint size at either sample,
    so the allowance is twice database.maxJournalSize (50k in test/utils/agent/ioto.json5).
 */
#define DB_GROWTH_LIMIT (100 * 1024)

//  web.limits.sessions in test/utils/agent/web.json5. Every round logs out, so the table stays flat.
#define SESSION_LIMIT 20

/*
    The rejected login costs a full password check. Issuing it every round would weigh the round
    toward bcrypt, so it runs every REJECT_EVERY rounds.
 */
#define REJECT_EVERY 10

#define UPLOAD_FILE "state/site/size/10K.txt"

typedef struct Sample {
    int64 rssKb;
    int   fds;
    int   sessions;
    int64 dbBytes;
} Sample;

typedef struct Arm {
    cchar *name;
    bool  (*run)(void);
    int   failures;
} Arm;

static bool wsEchoed;

/************************************ Code ************************************/

static bool sample(Sample *s)
{
    Stats stats;

    if (!getStats(&stats)) {
        return 0;
    }
    s->rssKb = stats.rssKb;
    s->fds = stats.fds;
    s->sessions = stats.sessions;
    s->dbBytes = fileSize("state/db/device.db") + fileSize("state/db/device.db.jnl");
    return 1;
}

static void show(cchar *label, Sample *s)
{
    tinfo("  %-8s rss %.1f MB, %d descriptors, %d sessions, db %lld bytes", label, s->rssKb / 1024.0, s->fds,
          s->sessions, (long long) s->dbBytes);
}

/*
    A document must arrive whole. The byte count is compared with the file on disk, so a
    response that is truncated after its headers still fails.
 */
static bool fetchDocument(cchar *base, cchar *path, cchar *file)
{
    char  url[256];
    int64 bytes;
    int   status;

    bytes = fetchBytes(SFMT(url, "%s%s", base, path), &status);
    if (status != 200 || bytes != fileSize(file)) {
        tinfo("  GET %s answered %d with %lld of %lld bytes", url, status, (long long) bytes, (long long) fileSize(file));
        return 0;
    }
    return 1;
}

static bool armStatic(void)
{
    return fetchDocument(HTTP, "/index.html", "state/site/index.html");
}

static bool armLarge(void)
{
    return fetchDocument(HTTP, "/size/1M.txt", "state/site/size/1M.txt");
}

static bool armAction(void)
{
    char url[256];

    return expect("GET", SFMT(url, "%s/api/public/stats", HTTP), NULL, 0, NULL, 200);
}

/*
    Log in, make a request the session authorizes, log out. The logout is what keeps the session
    table flat across the run; without it every round would add a session until the limit refused
    the login.
 */
static bool armLogin(void)
{
    Url   *up;
    cchar *body;
    char  url[256], *cookie, *header;
    bool  ok;

    if ((cookie = login("guest", FIXTURE_PASSWORD)) == 0) {
        tinfo("  login as guest did not establish a session");
        return 0;
    }
    header = cookieHeader(cookie);
    up = urlAlloc(0);
    ok = urlFetch(up, "GET", SFMT(url, "%s/api/user/whoami", HTTP), NULL, 0, "%s", header) == 200;
    body = urlGetResponse(up);
    ok = ok && body && scontains(body, "\"username\":\"guest\"") != 0;
    urlFree(up);
    if (!ok) {
        tinfo("  the session did not authenticate /api/user/whoami");
    }
    ok = expect("POST", SFMT(url, "%s/api/user/logout", HTTP), NULL, 0, header, 302) && ok;
    rFree(header);
    rFree(cookie);
    return ok;
}

static bool armReject(void)
{
    char  url[256];
    cchar *body = "username=guest&password=wrong";

    return expect("POST", SFMT(url, "%s/api/public/login", HTTP), body, slen(body),
                  "Content-Type: application/x-www-form-urlencoded\r\n", 401);
}

static bool armChurn(void)
{
    Url   *up;
    cchar *body;
    char  url[256];
    bool  ok;

    up = urlAlloc(0);
    ok = urlFetch(up, "GET", SFMT(url, "%s/api/public/churn", HTTP), NULL, 0, NULL) == 200;
    body = urlGetResponse(up);
    ok = ok && jsonNumber(body, "count") > 0;
    urlFree(up);
    return ok;
}

/*
    A multipart upload through the filter. The filter stages a temp file per part and removes it
    when the request completes, so a staging path that leaks shows up as descriptor growth.
 */
static bool armUpload(void)
{
    Url   *up;
    RList *files;
    RHash *forms;
    cchar *body;
    char  url[256];
    bool  ok;

    files = rAllocList(0, 0);
    rAddItem(files, UPLOAD_FILE);
    forms = rAllocHash(0, 0);
    rAddName(forms, "arm", "upload", 0);

    up = urlAlloc(0);
    ok = urlStart(up, "POST", SFMT(url, "%s/api/public/upload", HTTP)) == 0 && urlUpload(up, files, forms, NULL) == 0 &&
         urlFinalize(up) == 0 && urlGetStatus(up) == 200;
    body = urlGetResponse(up);
    ok = ok && body && scontains(body, "FILE ") != 0 && scontains(body, "VAR arm=upload") != 0;
    if (!ok) {
        tinfo("  upload answered %d", urlGetStatus(up));
    }
    urlFree(up);
    rFree(forms);
    rFree(files);
    return ok;
}

static void wsEvent(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    if (event == WS_EVENT_OPEN) {
        webSocketSend(ws, "soak");
    } else if (event == WS_EVENT_MESSAGE) {
        wsEchoed = (len == 4 && sncmp(data, "soak", 4) == 0);
        webSocketSendClose(ws, WS_STATUS_OK, "done");
    }
}

static bool armWebSocket(void)
{
    char url[256];

    wsEchoed = 0;
    if (urlWebSocket(SFMT(url, "ws%s/api/public/echo", &HTTP[4]), wsEvent, NULL, NULL) < 0) {
        tinfo("  WebSocket upgrade to /api/public/echo failed");
        return 0;
    }
    return wsEchoed;
}

static bool armTls(void)
{
    return fetchDocument(HTTPS, "/index.html", "state/site/index.html");
}

static bool armRedirect(void)
{
    char url[256];

    return expect("GET", SFMT(url, "%s/old", HTTP), NULL, 0, NULL, 302);
}

static bool armNotFound(void)
{
    char url[256];

    return expect("GET", SFMT(url, "%s/no-such-page.html", HTTP), NULL, 0, NULL, 404);
}

/*
    The mix. Every arm reaches a different subsystem: the file handler, the file handler with a
    body many buffers long, an action, the session table and database through a login, the
    rejected-login path, the database and journal through a write, the upload filter, the
    WebSocket upgrade, the TLS stack, the redirect table and the error path. A soak that only
    fetched index.html would prove the accept loop does not leak and nothing else.
 */
static Arm arms[] = {
    { "static", armStatic, 0 },
    { "large", armLarge, 0 },
    { "action", armAction, 0 },
    { "login", armLogin, 0 },
    { "reject", armReject, 0 },
    { "churn", armChurn, 0 },
    { "upload", armUpload, 0 },
    { "websocket", armWebSocket, 0 },
    { "tls", armTls, 0 },
    { "redirect", armRedirect, 0 },
    { "notfound", armNotFound, 0 },
    { NULL, NULL, 0 },
};

static void soak(void)
{
    Sample before, after;
    Ticks  started, duration;
    double rssRatio;
    int    depth, i, rounds, issued, failed, fdGrowth;
    int64  dbGrowth;

    depth = tdepth();
    if (depth < 0) {
        depth = 0;
    } else if (depth > 9) {
        depth = 9;
    }
    duration = SECONDS[depth] * TPS;

    /*
        Whether the process can be measured at all, decided before any workload is issued. A
        platform the fixture cannot report on returns zero, and asserting on zeros would report a
        leak-free agent on a machine that measured nothing.
     */
    if (!sample(&before)) {
        tskip("soak skipped: the agent's /api/public/stats fixture did not answer");
        return;
    }
    if (before.rssKb <= 0) {
        tskip("soak skipped: this platform cannot report the agent's resident set");
        return;
    }

    /*
        Warm up before the baseline, so caches, the session table and the Churn table are
        populated on both sides of the measurement and their filling is not counted as growth
     */
    for (i = 0; i < 20; i++) {
        armStatic();
        armLarge();
        armChurn();
        armLogin();
    }
    if (!sample(&before)) {
        tfail("cannot sample the agent after warm-up");
        return;
    }
    show("start", &before);

    rounds = issued = failed = 0;
    started = rGetTicks();

    while (rGetTicks() - started < duration) {
        for (i = 0; arms[i].name; i++) {
            if (arms[i].run == armReject && (rounds % REJECT_EVERY) != 0) {
                continue;
            }
            issued++;
            if (!arms[i].run()) {
                arms[i].failures++;
                failed++;
            }
        }
        rounds++;
    }

    if (!sample(&after)) {
        tfail("cannot sample the agent after the soak");
        return;
    }
    show("end", &after);
    tinfo("  %d rounds, %d requests, %d failed, over %.1fs at depth %d", rounds, issued, failed,
          (rGetTicks() - started) / (double) TPS, tdepth());

    /*
        Every request answered. A bare total says the workload broke; the per-arm figure says
        which subsystem, and with eleven arms that is the difference between a diagnosis and a
        re-run.
     */
    if (failed) {
        tinfo("soak: %d of %d requests did not answer as expected", failed, issued);
        for (i = 0; arms[i].name; i++) {
            if (arms[i].failures) {
                tinfo("soak:   %-10s %d failed", arms[i].name, arms[i].failures);
            }
        }
    }
    ttrue(failed == 0);
    ttrue(issued > 0);

    /*
        Reported before asserted, so a failing run leaves the numbers behind rather than only a
        verdict.
     */
    rssRatio = after.rssKb / (double) before.rssKb;
    fdGrowth = after.fds - before.fds;
    dbGrowth = after.dbBytes - before.dbBytes;
    tinfo("  rss x%.2f, descriptors %+d, db %+lld bytes, sessions %d", rssRatio, fdGrowth, (long long) dbGrowth,
          after.sessions);

    if (rssRatio > RSS_GROWTH_LIMIT) {
        tinfo("soak: resident memory grew %.2fx over %d requests (%lld KB to %lld KB)", rssRatio, issued,
              (long long) before.rssKb, (long long) after.rssKb);
    }
    ttrue(rssRatio <= RSS_GROWTH_LIMIT);

    if (after.fds > 0) {
        if (fdGrowth > FD_GROWTH_LIMIT) {
            tinfo("soak: open descriptors grew by %d over %d requests", fdGrowth, issued);
        }
        ttrue(fdGrowth <= FD_GROWTH_LIMIT);
    } else {
        tinfo("  descriptors not reported on this platform");
    }

    if (dbGrowth > DB_GROWTH_LIMIT) {
        tinfo("soak: database plus journal grew by %lld bytes over %d requests", (long long) dbGrowth, issued);
    }
    ttrue(dbGrowth <= DB_GROWTH_LIMIT);

    if (after.sessions > SESSION_LIMIT) {
        tinfo("soak: %d live sessions exceeds the configured limit of %d", after.sessions, SESSION_LIMIT);
    }
    ttrue(after.sessions <= SESSION_LIMIT);

    /*
        And the agent still serves. A soak that exhausted a pool or wedged the event loop would
        still satisfy every growth bound above - the counters would simply stop moving.
     */
    ttrue(armStatic());
    ttrue(armChurn());
    ttrue(armLogin());
    ttrue(armWebSocket());
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        soak();
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
