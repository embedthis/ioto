/*
    fuzzLib.c - Fuzzing test library implementation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "fuzzLib.h"
#include "r.h"
#include "crypt.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <stdlib.h>

/*********************************** Locals ***********************************/

static FuzzRunner *currentRunner = NULL;

/********************************** Forwards **********************************/

static void signalHandler(int sig);
static char *generateRandomData(size_t len, bool printable);
static uint getRandom(void);

/************************************ Code ************************************/

PUBLIC FuzzRunner *fuzzInit(FuzzConfig *config)
{
    FuzzRunner *runner;

    runner = rAllocType(FuzzRunner);
    runner->config = *config;
    runner->corpus = rAllocList(10, 0);
    runner->crashes = rAllocHash(0, 0);
    runner->stats.startTime = rGetTicks();
    runner->crashed = false;
    runner->signal = 0;

    // Create crash directory
    if (runner->config.crashDir) {
        mkdir(runner->config.crashDir, 0755);
    }

    // Initialize random seed
    if (runner->config.seed == 0) {
        runner->config.seed = (uint) rGetTicks();
    }
    srandom(runner->config.seed);

    // Setup signal handlers
    currentRunner = runner;
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGFPE, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGBUS, signalHandler);

    return runner;
}

PUBLIC void fuzzFree(FuzzRunner *runner)
{
    if (!runner) return;

    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGBUS, SIG_DFL);

    rFreeList(runner->corpus);
    rFreeHash(runner->crashes);
    rFree(runner);

    currentRunner = NULL;
}

PUBLIC void fuzzSetOracle(FuzzRunner *runner, FuzzOracle oracle)
{
    runner->oracle = oracle;
}

PUBLIC void fuzzSetMutator(FuzzRunner *runner, FuzzMutator mutator)
{
    runner->mutator = mutator;
}

PUBLIC int fuzzLoadCorpus(FuzzRunner *runner, cchar *path)
{
    char   *content, *line, *next;
    size_t len;
    int    count = 0;

    content = rReadFile(path, &len);
    if (!content) {
        return 0;
    }

    for (line = stok(content, "\n", &next); line; line = stok(NULL, "\n", &next)) {
        if (line[0] && line[0] != '#') {
            fuzzAddCorpus(runner, line, slen(line));
            count++;
        }
    }

    rFree(content);
    return count;
}

PUBLIC void fuzzAddCorpus(FuzzRunner *runner, cchar *input, size_t len)
{
    rPushItem(runner->corpus, snclone(input, len));
}

PUBLIC int fuzzRun(FuzzRunner *runner)
{
    size_t corpusLen;
    int    i;

    if (!runner->oracle) {
        rError("fuzz", "No test oracle configured");
        return -1;
    }

    if (rGetListLength(runner->corpus) == 0) {
        rError("fuzz", "Empty corpus - add at least one test case");
        return -1;
    }

    for (i = 0; i < runner->config.iterations; i++) {
        cchar *input = fuzzGetRandomCorpus(runner, &corpusLen);
        if (!input) continue;

        size_t len = corpusLen;
        char   *mutated = runner->mutator ? runner->mutator(input, &len) : snclone(input, len);

        if (setjmp(runner->crashJmp) == 0) {
            bool passed = runner->oracle(mutated, len);

            if (!passed) {
                if (fuzzIsUniqueCrash(runner, mutated, len)) {
                    fuzzSaveCrash(runner, mutated, len, 0);
                    runner->stats.crashes++;
                    runner->stats.unique++;
                }
            }
            runner->stats.total++;

        } else {
            if (fuzzIsUniqueCrash(runner, mutated, len)) {
                fuzzSaveCrash(runner, mutated, len, runner->signal);
                runner->stats.crashes++;
                runner->stats.unique++;
            }
            runner->crashed = false;
        }

        rFree(mutated);

        if (runner->config.verbose && (i % 1000) == 0 && i > 0) {
            rInfo("fuzz", "Iteration %d/%d - Crashes: %d (unique: %d)",
                  i, runner->config.iterations, runner->stats.crashes, runner->stats.unique);
        }
    }

    runner->stats.endTime = rGetTicks();
    return runner->stats.unique;
}

PUBLIC void fuzzReport(FuzzRunner *runner)
{
    Ticks elapsed = runner->stats.endTime - runner->stats.startTime;

    rPrintf("\n=== Fuzzing Report ===\n");
    rPrintf("Iterations:     %d\n", runner->stats.total);
    rPrintf("Crashes:        %d\n", runner->stats.crashes);
    rPrintf("Unique crashes: %d\n", runner->stats.unique);
    rPrintf("Elapsed time:   %.2f seconds\n", (double) elapsed / TPS);
    if (elapsed > 0) {
        rPrintf("Rate:           %.0f tests/sec\n", (double) runner->stats.total / ((double) elapsed / TPS));
    }
    if (runner->config.crashDir && runner->stats.unique > 0) {
        rPrintf("Crash files:    %s/\n", runner->config.crashDir);
    }
    rPrintf("\n");
}

PUBLIC void fuzzSaveCrash(FuzzRunner *runner, cchar *input, size_t len, int sig)
{
    char *hash, *filename, *content, *metafile;
    int  fd;

    if (!runner->config.crashDir) return;

    hash = fuzzHash(input, len);
    filename = sfmt("%s/crash-%s.txt", runner->config.crashDir, hash);

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, input, len);
        close(fd);
    }

    content = sfmt("Signal: %d\nLength: %lld\nHash: %s\nTime: %lld\n",
                   sig, (long long) len, hash, (long long) rGetTicks());

    metafile = sfmt("%s/crash-%s.meta", runner->config.crashDir, hash);
    rWriteFile(metafile, content, slen(content), 0644);

    rFree(hash);
    rFree(filename);
    rFree(content);
    rFree(metafile);
}

PUBLIC char *fuzzBitFlip(cchar *input, size_t *len)
{
    char *result;

    if (*len == 0) return sclone("");
    result = snclone(input, *len);
    result[getRandom() % *len] ^= (1 << (getRandom() % 8));
    return result;
}

PUBLIC char *fuzzByteFlip(cchar *input, size_t *len)
{
    char *result;

    if (*len == 0) return sclone("");
    result = snclone(input, *len);
    result[getRandom() % *len] = ~result[getRandom() % *len];
    return result;
}

PUBLIC char *fuzzInsertRandom(cchar *input, size_t *len)
{
    size_t insertLen = (getRandom() % 100) + 1;
    size_t pos = *len > 0 ? (getRandom() % *len) : 0;
    char   *result = rAlloc(*len + insertLen);
    size_t i;

    memcpy(result, input, pos);
    for (i = 0; i < insertLen; i++) {
        result[pos + i] = getRandom() % 256;
    }
    memcpy(result + pos + insertLen, input + pos, *len - pos);
    *len += insertLen;
    return result;
}

PUBLIC char *fuzzDeleteRandom(cchar *input, size_t *len)
{
    size_t deleteLen, pos;
    char   *result;

    if (*len <= 1) return snclone(input, *len);

    deleteLen = (getRandom() % (*len / 2)) + 1;
    pos = getRandom() % (*len - deleteLen + 1);
    result = rAlloc(*len - deleteLen);

    memcpy(result, input, pos);
    memcpy(result + pos, input + pos + deleteLen, *len - pos - deleteLen);
    *len -= deleteLen;
    return result;
}

PUBLIC char *fuzzOverwriteRandom(cchar *input, size_t *len)
{
    size_t overwriteLen, pos, i;
    char   *result;

    if (*len == 0) return sclone("");

    result = snclone(input, *len);
    overwriteLen = (getRandom() % *len) + 1;
    pos = getRandom() % (*len - overwriteLen + 1);

    for (i = 0; i < overwriteLen; i++) {
        result[pos + i] = getRandom() % 256;
    }
    return result;
}

PUBLIC char *fuzzInsertSpecial(cchar *input, size_t *len)
{
    static cchar *special[] = { "\x00", "\r\n", "\r", "\n", "\t", "\"", "'", "<", ">", "&", ";", "|", "`", "$", NULL };
    cchar        *specialStr = special[getRandom() % 14];
    size_t       specialLen = slen(specialStr);
    size_t       pos = *len > 0 ? (getRandom() % *len) : 0;
    char         *result = rAlloc(*len + specialLen);

    memcpy(result, input, pos);
    memcpy(result + pos, specialStr, specialLen);
    memcpy(result + pos + specialLen, input + pos, *len - pos);
    *len += specialLen;
    return result;
}

PUBLIC char *fuzzReplace(cchar *input, size_t *len, cchar *pattern, cchar *replacement)
{
    char   *pos = (char*) scontains(input, pattern);
    size_t patternLen, replaceLen, offset;
    char   *result;

    if (!pos) return snclone(input, *len);

    patternLen = slen(pattern);
    replaceLen = slen(replacement);
    offset = (size_t)(pos - input);
    result = rAlloc(*len - patternLen + replaceLen);

    memcpy(result, input, offset);
    memcpy(result + offset, replacement, replaceLen);
    memcpy(result + offset + replaceLen, pos + patternLen, *len - offset - patternLen);
    *len = *len - patternLen + replaceLen;
    return result;
}

PUBLIC char *fuzzSplice(cchar *input, size_t *len)
{
    return fuzzDuplicate(input, len);
}

PUBLIC char *fuzzDuplicate(cchar *input, size_t *len)
{
    char *result = rAlloc(*len * 2);

    memcpy(result, input, *len);
    memcpy(result + *len, input, *len);
    *len *= 2;
    return result;
}

PUBLIC char *fuzzTruncate(cchar *input, size_t *len)
{
    size_t newLen;

    if (*len <= 1) return snclone(input, *len);
    newLen = getRandom() % *len;
    if (newLen == 0) newLen = 1;
    *len = newLen;
    return snclone(input, newLen);
}

PUBLIC char *fuzzRandomString(size_t len)
{
    return generateRandomData(len, true);
}

PUBLIC char *fuzzRandomData(size_t len)
{
    return generateRandomData(len, false);
}

PUBLIC char *fuzzHash(cchar *input, size_t len)
{
    uchar digest[CRYPT_SHA256_SIZE];

    cryptGetSha256Block((cuchar*) input, len, digest);
    return sfmt("%02x%02x%02x%02x%02x%02x%02x%02x",
                digest[0], digest[1], digest[2], digest[3],
                digest[4], digest[5], digest[6], digest[7]);
}

PUBLIC bool fuzzIsUniqueCrash(FuzzRunner *runner, cchar *input, size_t len)
{
    char *hash = fuzzHash(input, len);
    bool unique = rLookupName(runner->crashes, hash) == NULL;

    if (unique) {
        rAddName(runner->crashes, hash, (void*) 1, 0);
    }
    rFree(hash);
    return unique;
}

PUBLIC cchar *fuzzGetRandomCorpus(FuzzRunner *runner, size_t *len)
{
    int   count = rGetListLength(runner->corpus);
    uint  index;
    cchar *entry;

    if (count == 0) {
        *len = 0;
        return NULL;
    }
    index = getRandom() % (uint) count;
    entry = rGetItem(runner->corpus, (int) index);
    *len = entry ? slen(entry) : 0;
    return entry;
}

static void signalHandler(int sig)
{
    if (currentRunner) {
        currentRunner->crashed = true;
        currentRunner->signal = sig;
        longjmp(currentRunner->crashJmp, 1);
    }
}

static char *generateRandomData(size_t len, bool printable)
{
    char   *result = rAlloc(len + 1);
    size_t i;

    for (i = 0; i < len; i++) {
        result[i] = printable ? 32 + (getRandom() % 95) : getRandom() % 256;
    }
    result[len] = '\0';
    return result;
}

static uint getRandom(void)
{
    return (uint) random();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
