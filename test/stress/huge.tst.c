/*
    huge.tst.c - Transfer a file larger than 4 GB and verify the byte count and the advertised length

    The suite's only exercise of a response beyond the 32-bit range, which is where a truncated
    length or offset shows up. The advertised Content-Length must survive that boundary intact: a
    truncated one desynchronises the connection, because the peer stops reading at the advertised
    count and takes the rest of the body for the next response. The body is streamed and counted
    rather than buffered.

    agent-prep.sh generates state/site/huge.txt at this depth and not below, and reuses one of the
    right size from an earlier run. Generating it costs more than the transfer it feeds.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

/*********************************** Locals ***********************************/

#define DEPTH 5
#define HUGE  "state/site/huge.txt"
#define SIZE  5368709120LL

/************************************ Code ************************************/

static void testHugeTransfer(void)
{
    Url   *up;
    Ticks started;
    cchar *length;
    char  url[256], buf[64 * 1024];
    ssize n;
    int64 total;
    double seconds;

    if (tdepth() < DEPTH) {
        tskip("runs at depth %d", DEPTH);
        return;
    }
    ttrue(fileSize(HUGE) == SIZE);

    up = urlAlloc(0);
    started = rGetTicks();
    ttrue(urlStart(up, "GET", SFMT(url, "%s/huge.txt", HTTP)) == 0);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);

    length = urlGetHeader(up, "Content-Length");
    ttrue(length != NULL);
    ttrue(length && stoi(length) == SIZE);

    total = 0;
    while ((n = urlRead(up, buf, sizeof(buf))) > 0) {
        total += n;
    }
    seconds = (rGetTicks() - started) / (double) TPS;
    tinfo("  transferred %lld bytes in %.1fs (%.1f MB/s)", (long long) total, seconds,
          total / seconds / (1024 * 1024));
    ttrue(n == 0);
    ttrue(total == SIZE);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testHugeTransfer();
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
