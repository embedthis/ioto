/*
    login-failure.tst.c - An unknown user and a wrong password are refused alike, and the refusal is logged

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "auth.h"

/************************************ Code ************************************/

static int compareTicks(cvoid *a, cvoid *b)
{
    Ticks x = *(Ticks*) a, y = *(Ticks*) b;

    return x < y ? -1 : (x > y ? 1 : 0);
}

static Ticks median(Ticks *samples, int count)
{
    qsort(samples, (size_t) count, sizeof(Ticks), compareTicks);
    return samples[count / 2];
}

static void testSameResponse(void)
{
    char *unknownBody, *wrongBody, *log;
    int  unknown, wrong;

    waitFailurePeriod();
    unknown = postLogin("nosuchuser", FIXTURE_PASSWORD, &unknownBody, NULL);
    wrong = postLogin("guest", "wrong-password", &wrongBody, NULL);
    teqi(unknown, 401);
    teqi(wrong, 401);
    tmatch(unknownBody, wrongBody, "An unknown user and a wrong password must get the same body");

    //  One "auth" line per failure, with the client address, the same reason for both, and no credentials
    log = readLog();
    tcontains(log, "auth: Refused POST from 127.0.0.1");
    tcontains(log, "login failed (route '/api/public/')");
    tfalse(scontains(log, "wrong-password"), "The password must never be logged");
    tfalse(scontains(log, "nosuchuser"), "The submitted username must not be logged");
    rFree(log);
    rFree(unknownBody);
    rFree(wrongBody);
}

/*
    An unknown user costs the same password work as a known one. Timing needs repetitions, so it runs
    at depth 3 and above, in rounds kept below the failure limit.
 */
static void testSameCost(void)
{
    Ticks start, unknown[20], wrong[20];
    int   i, round;

    if (tdepth() < 3) {
        return;
    }
    for (i = 0, round = 0; round < 10; round++) {
        waitFailurePeriod();
        start = rGetHiResTicks();
        postLogin("nosuchuser", "wrong-password", NULL, NULL);
        unknown[i] = rGetHiResTicks() - start;
        start = rGetHiResTicks();
        postLogin("guest", "wrong-password", NULL, NULL);
        wrong[i] = rGetHiResTicks() - start;
        i++;
        start = rGetHiResTicks();
        postLogin("nosuchuser", "wrong-password", NULL, NULL);
        unknown[i] = rGetHiResTicks() - start;
        start = rGetHiResTicks();
        postLogin("guest", "wrong-password", NULL, NULL);
        wrong[i] = rGetHiResTicks() - start;
        i++;
    }
    tinfo("median unknown %lld, wrong password %lld", (long long) median(unknown, i), (long long) median(wrong, i));
    ttrue(median(unknown, i) * 2 >= median(wrong, i), "An unknown user must cost at least half of a known user");
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testSameResponse();
        testSameCost();
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
