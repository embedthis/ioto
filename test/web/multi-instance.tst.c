/*
    multi-instance.tst.c - Test multiple web server instances

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

#define HOST1_CONFIG "{ web: { documents: './site', listen: ['http://:4100'] } }"
#define HOST2_CONFIG "{ web: { documents: './site', listen: ['http://:4200'] } }"

/*
    Test creating multiple hosts
 */
static void testCreateMultipleHosts(void)
{
    Json    *config1, *config2;
    WebHost *host1, *host2;

    config1 = jsonParse(HOST1_CONFIG, 0);
    config2 = jsonParse(HOST2_CONFIG, 0);

    ttrue(config1 != NULL, "Config 1 should parse successfully");
    ttrue(config2 != NULL, "Config 2 should parse successfully");

    host1 = webAllocHost(config1, 0);
    host2 = webAllocHost(config2, 0);

    ttrue(host1 != NULL, "Host 1 should allocate successfully");
    ttrue(host2 != NULL, "Host 2 should allocate successfully");
    ttrue(host1 != host2, "Hosts should be different instances");

    // Verify each has independent config
    ttrue(host1->config == config1, "Host 1 should have config 1");
    ttrue(host2->config == config2, "Host 2 should have config 2");
    ttrue(host1->config != host2->config, "Configs should be independent");

    // Verify each has independent sessions
    ttrue(host1->sessions != NULL, "Host 1 should have sessions hash");
    ttrue(host2->sessions != NULL, "Host 2 should have sessions hash");
    ttrue(host1->sessions != host2->sessions, "Sessions should be independent");

    // Verify independent connection counters
    ttrue(host1->connSequence == 0, "Host 1 connSequence should start at 0");
    ttrue(host2->connSequence == 0, "Host 2 connSequence should start at 0");

    // Cleanup
    webFreeHost(host1);
    webFreeHost(host2);
    jsonFree(config1);
    jsonFree(config2);
}

/*
    Test independent connection counters
 */
static void testIndependentConnectionCounters(void)
{
    Json    *config1, *config2;
    WebHost *host1, *host2;

    config1 = jsonParse(HOST1_CONFIG, 0);
    config2 = jsonParse(HOST2_CONFIG, 0);

    host1 = webAllocHost(config1, 0);
    host2 = webAllocHost(config2, 0);

    // Initial counters should be zero
    ttrue(host1->connSequence == 0, "Host 1 connSequence should be 0");
    ttrue(host2->connSequence == 0, "Host 2 connSequence should be 0");

    // Simulate connections
    host1->connSequence++;
    host1->connSequence++;
    host2->connSequence++;

    // Verify independence
    ttrue(host1->connSequence == 2, "Host 1 should have connSequence = 2");
    ttrue(host2->connSequence == 1, "Host 2 should have connSequence = 1");

    webFreeHost(host1);
    webFreeHost(host2);
    jsonFree(config1);
    jsonFree(config2);
}

static void fiberMain(void *data)
{
    testCreateMultipleHosts();
    testIndependentConnectionCounters();
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
