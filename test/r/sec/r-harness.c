/*
    r-harness.c - Runtime fuzz tests. See README.md for usage.

    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */

/********************************** Includes **********************************/

#include "harness.h"

#ifndef SEC_LIBFUZZER
#include "fuzzcore/fuzz.c"
#endif

#include "r.h"

/*********************************** Locals ***********************************/

/*
    Initialize the runtime once before any rAlloc calls.
 */
static void initRuntime(void)
{
    static int initialised = 0;

    if (!initialised) {
        initialised = 1;
        rInit(0, 0);
    }
}

/*
    Copy and terminate input because string APIs may scan or modify it.
 */
static char *dupInput(cchar *input, size_t len)
{
    char *buf;

    buf = (char*) secAlloc(len + 1);
    memcpy(buf, input, len);
    buf[len] = '\0';
    return buf;
}

/*
    Provide a bounded length helper for C99 builds without strnlen.
 */
static size_t safeStrnlen(cchar *s, size_t max)
{
    size_t i;

    for (i = 0; i < max && s[i]; i++) {
    }
    return i;
}

/********************************** Oracles ***********************************/

/*
    Use valid pointers for integer/pointer argument slots and doubles for floating-point slots.
    Compare bounded output with the corresponding allocated output prefix.
 */
#define PRINTF_NBUF_SIZE 64

static void runFormats(cchar *fmt, char *nbuf, size_t nbufSize, char **abuf, ssize *na, ssize *nb, ...)
{
    va_list ap1, ap2;

    va_start(ap1, nb);
    va_copy(ap2, ap1);
    *na = rVsaprintf(abuf, 0, fmt, ap1);
    *nb = rVsnprintf(nbuf, nbufSize, fmt, ap2);
    va_end(ap1);
    va_end(ap2);
}

static bool printfOracle(cchar *input, size_t len)
{
    char  *fmt;
    char  nbuf[PRINTF_NBUF_SIZE];
    char  *abuf;
    char  scratch[32];
    ssize na, nb, prefix;
    bool  ok;

    initRuntime();
    ok = true;
    fmt = dupInput(input, len);
    abuf = NULL;
    scopy(scratch, sizeof(scratch), "printf-oracle-scratch");

    /*
        Repeat argument groups so each conversion has an argument.
     */
    runFormats(fmt, nbuf, sizeof(nbuf), &abuf, &na, &nb,
               (char*) "hello", (void*) scratch, (double) 3.14159, (char*) NULL, (char*) "world", (void*) scratch,
               (double) -0.5, (char*) NULL, (char*) "z", (void*) scratch, (double) 0.0, (char*) NULL,
               (char*) "", (void*) scratch, (double) 1e300, (char*) NULL, (char*) "hello", (void*) scratch,
               (double) -1e-300, (char*) NULL, (char*) "world", (void*) scratch, (double) 3.14159, (char*) NULL);

    if (nb >= 0) {
        prefix = (ssize) safeStrnlen(nbuf, sizeof(nbuf));
        if (prefix >= (ssize) sizeof(nbuf)) {
            fprintf(stderr, "sec: rVsnprintf buffer not NUL-terminated within bounds for fmt \"%.80s\"\n", fmt);
            ok = false;
        } else if (abuf && na >= 0) {
            if (memcmp(nbuf, abuf, (size_t) prefix) != 0) {
                fprintf(stderr, "sec: rVsnprintf and rVsaprintf disagree for fmt \"%.80s\"\n", fmt);
                ok = false;
            }
        }
    }
    if (abuf) {
        rFree(abuf);
    }
    secFree(fmt);
    return ok;
}

/*
    Check numeric round trips and tokenizer memory safety.
 */
static bool numberOracle(cchar *input, size_t len)
{
    char   *buf, *text, *tok, *r;
    char   *end, *end2, *last, *nextp;
    char   nbuf[80];
    int    radix;
    int64  value, value2;
    bool   ok;

    initRuntime();
    ok = true;
    buf = dupInput(input, len);
    radix = (len > 0) ? ((uchar) buf[0] == 0 ? 0 : 2 + ((uchar) buf[0] % 35)) : 10;
    text = (len > 0) ? buf + 1 : buf;

    end = NULL;
    value = stoix(text, &end, radix);
    if (end && end != text) {
        end2 = NULL;
        sitosbuf(nbuf, sizeof(nbuf), value, (radix == 0) ? 10 : radix);
        value2 = stoix(nbuf, &end2, (radix == 0) ? 10 : radix);
        if (value2 != value) {
            fprintf(stderr, "sec: sitosbuf/stoix round trip disagrees: %lld != %lld (radix %d, text \"%.80s\")\n",
                    (long long) value, (long long) value2, radix, text);
            ok = false;
        }
    }
    if (snumber(text)) {
        end2 = NULL;
        stoix(text, &end2, 10);
        if (end2 != text + slen(text)) {
            fprintf(stderr, "sec: snumber true but stoix did not consume all of \"%.80s\"\n", text);
            ok = false;
        }
    }
    //  Memory-safety only: no simple invariant for these.
    sfnumber(text);
    stoi(text);
    shnumber(text);
    sspace(text);

    tok = sclone(text);
    last = NULL;
    stok(tok, " ,;\t\n", &last);
    rFree(tok);

    tok = sclone(text);
    nextp = NULL;
    sptok(tok, "::", &nextp);
    rFree(tok);

    tok = sclone(text);
    last = NULL;
    ssplit(tok, ",", &last);
    rFree(tok);

    r = sreplace(text, "a", "bb");
    rFree(r);

    secFree(buf);
    return ok;
}

/*
    Check date parse/format round trips; negative results indicate parse failure.
 */
static bool dateOracle(cchar *input, size_t len)
{
    char *buf;
    Time t1, t2;
    char *formatted;
    bool ok;

    initRuntime();
    ok = true;
    buf = dupInput(input, len);

    t1 = rParseIsoDate(buf);
    if (t1 >= 0) {
        formatted = rGetIsoDate(t1);
        if (formatted) {
            t2 = rParseIsoDate(formatted);
            if (t2 != t1) {
                fprintf(stderr, "sec: ISO date round trip disagrees: %lld != %lld for \"%.80s\" -> \"%s\"\n",
                        (long long) t1, (long long) t2, buf, formatted);
                ok = false;
            }
            rFree(formatted);
        }
    }
    t1 = rParseHttpDate(buf);
    if (t1 >= 0) {
        formatted = rGetHttpDate(t1);
        if (formatted) {
            t2 = rParseHttpDate(formatted);
            if (t2 != t1) {
                fprintf(stderr, "sec: HTTP date round trip disagrees: %lld != %lld for \"%.80s\" -> \"%s\"\n",
                        (long long) t1, (long long) t2, buf, formatted);
                ok = false;
            }
            rFree(formatted);
        }
    }
    secFree(buf);
    return ok;
}

/*
    Input is pattern followed by newline and path, or one string used for both.
    Check bounded writes and directory prefixes without filesystem access.
 */
static bool pathOracle(cchar *input, size_t len)
{
    char  *buf, *nl, *joined, *dirCopy, *dir;
    cchar *pattern, *path;
    char  fixed[24];
    ssize joinedLen, dirLen;
    bool  ok;

    initRuntime();
    ok = true;
    buf = dupInput(input, len);

    if ((nl = strchr(buf, '\n')) != 0) {
        *nl = '\0';
        pattern = buf;
        path = nl + 1;
    } else {
        pattern = buf;
        path = buf;
    }

    joined = rJoinFile(path, pattern);
    if (joined) {
        joinedLen = (ssize) slen(joined);
        dirCopy = sclone(joined);
        dir = rDirname(dirCopy);
        if (dir) {
            dirLen = (ssize) slen(dir);
            if (dirLen > joinedLen || memcmp(dir, joined, (size_t) dirLen) != 0) {
                fprintf(stderr, "sec: rDirname(rJoinFile(\"%.40s\", \"%.40s\")) = \"%s\" is not a prefix of \"%s\"\n",
                        path, pattern, dir, joined);
                ok = false;
            }
        }
        rFree(dirCopy);
        rFree(joined);
    }
    //  Memory-safety only, no simple round-trip invariant.
    rGetFileExt(path);
    rBasename(path);
    rMatchFile(path, pattern);

    if (rJoinFileBuf(fixed, sizeof(fixed), path, pattern) != NULL) {
        if (safeStrnlen(fixed, sizeof(fixed)) >= sizeof(fixed)) {
            fprintf(stderr, "sec: rJoinFileBuf left \"%.40s\"/\"%.40s\" not NUL-terminated within bounds\n",
                    path, pattern);
            ok = false;
        }
    }
    secFree(buf);
    return ok;
}

/*
    Input contains a template followed by key=value lines.
    Also interpret raw bytes as buffer operations and check length against capacity.
 */
static bool bufOpcodes(cchar *input, size_t len)
{
    RBuf   *bp;
    size_t pos;
    uchar  op;
    size_t n;
    schar  delta;
    bool   ok;

    ok = true;
    bp = rAllocBuf(8);
    pos = 0;
    while (pos < len) {
        op = (uchar) input[pos++];
        switch (op % 6) {
        case 0:
            if (pos < len) {
                rPutCharToBuf(bp, (uchar) input[pos++]);
            }
            break;
        case 1:
            n = 0;
            if (pos < len) {
                n = (uchar) input[pos++] % 64;
            }
            n = min(n, len - pos);
            rPutBlockToBuf(bp, &input[pos], n);
            pos += n;
            break;
        case 2:
            n = 0;
            if (pos < len) {
                n = (uchar) input[pos++];
            }
            rGrowBufSize(bp, n);
            break;
        case 3:
            if (pos < len) {
                rInserCharToBuf(bp, (uchar) input[pos++]);
            }
            break;
        case 4:
            delta = 0;
            if (pos < len) {
                delta = (schar) input[pos++];
            }
            rAdjustBufEnd(bp, delta);
            break;
        default:
            rCompactBuf(bp);
            break;
        }
        if (rGetBufLength(bp) > rGetBufSize(bp)) {
            fprintf(stderr, "sec: RBuf length %zu exceeds size %zu after op %d\n",
                    rGetBufLength(bp), rGetBufSize(bp), op % 6);
            ok = false;
        }
    }
    rFreeBuf(bp);
    return ok;
}

static bool templateOracle(cchar *input, size_t len)
{
    char  *buf, *nl, *line, *lastLine, *eq, *key, *value, *result;
    cchar *tmpl;
    RHash *hash;
    bool  ok;

    initRuntime();
    ok = true;
    buf = dupInput(input, len);

    if ((nl = strchr(buf, '\n')) != 0) {
        *nl = '\0';
        tmpl = buf;
        hash = rAllocHash(0, 0);
        line = stok(nl + 1, "\n", &lastLine);
        while (line) {
            if ((eq = strchr(line, '=')) != 0) {
                *eq = '\0';
                key = line;
                value = eq + 1;
            } else {
                key = line;
                value = "";
            }
            if (*key) {
                rAddName(hash, key, value, R_TEMPORAL_NAME | R_TEMPORAL_VALUE);
            }
            line = stok(NULL, "\n", &lastLine);
        }
    } else {
        tmpl = buf;
        hash = rAllocHash(0, 0);
    }
    result = stemplate(tmpl, hash);
    if (result) {
        rFree(result);
    }
    rFreeHash(hash);
    secFree(buf);

    if (!bufOpcodes(input, len)) {
        ok = false;
    }
    return ok;
}

#if defined(R_ORACLE_NUMBER)
    #define ORACLE      numberOracle
    #define ORACLE_NAME "number"
#elif defined(R_ORACLE_DATE)
    #define ORACLE      dateOracle
    #define ORACLE_NAME "date"
#elif defined(R_ORACLE_PATH)
    #define ORACLE      pathOracle
    #define ORACLE_NAME "path"
#elif defined(R_ORACLE_TEMPLATE)
    #define ORACLE      templateOracle
    #define ORACLE_NAME "template"
#else
    #define ORACLE      printfOracle
    #define ORACLE_NAME "printf"
#endif

/****************************** libFuzzer entry *******************************/

#ifdef SEC_LIBFUZZER
SEC_LIBFUZZER_ENTRY(ORACLE)

#else

/*********************************** main *************************************/

int main(int argc, char **argv)
{
    FuzzConfig config;
    FuzzRunner *runner;
    cchar      *replay;
    int        crashes, leaked;

    (void) argc;
    (void) argv;

    initRuntime();

    memset(&config, 0, sizeof(config));
    config.stop = false;
    config.verbose = false;
    /*
        fuzzInit creates only one directory level.
     */
    config.crashDir = "crashes";

    runner = fuzzInit(&config);
    fuzzSetOracle(runner, ORACLE);

    /*
        Use input formats appropriate to each test.
     */
#if defined(R_ORACLE_NUMBER)
    {
        static cchar *seeds[] = {
            "00", "01234", "0-1", "0abc1234", "01234 ", "01234.345", "0FF", "0invalid",
            "012345", "00x10", "0010", "0-9223372036854775808", "09223372036854775807",
            "01e5", "0a,b,c", "0a::b::c", "0hello world", "0", "",
            // '5' selects radix 20: 2 + 53 % 35.
            "5H", "5Z", "5100",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#elif defined(R_ORACLE_DATE)
    {
        static cchar *seeds[] = {
            "2023-12-25T10:30:45Z", "2023-12-25T10:30:45+00:00", "2023-12-25T10:30:45.123Z",
            "2023-12-25T10:30:45+05:30", "2023-12-25T10:30:45-08:00", "invalid",
            "2023-12-25", "2023-13-25T10:30:45Z", "2023-12-32T10:30:45Z",
            "2023-12-25T25:30:45Z", "Thu, 01 Jan 1970 00:00:00 GMT", "0001-01-01T00:00:00Z",
            "2023-12-25T10:30:45+99:00", "2023-12-25T10:30:45.999999999Z", "",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#elif defined(R_ORACLE_PATH)
    {
        static cchar *seeds[] = {
            "abc\nabc", "*\nabc", "a*c\nabc", "???\nabc", "**\na/b/c", "**c\na/b/c",
            "Makefile\n/tmp", "/Makefile\n/tmp", ".\n/tmp", "\nMakefile", "a/b\n/a/b/c",
            "\n", "abc", "",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#elif defined(R_ORACLE_TEMPLATE)
    {
        static cchar *seeds[] = {
            "Hello ${name}\nname=World", "${a}${b}\na=1\nb=2", "no substitutions here\n",
            "${missing}", "$", "${}", "${a}\nb=c", "plain text, no dollar\n",
            "\x01\x02\x03\x04\x05", "",
            "${", "abc${", "${x", "${greeting} abc${",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#else
    {
        static cchar *seeds[] = {
            "%d", "%-12345678", "%f", "-37.1234", "%g", "%e", "%3.2f", "%i", "%o", "077",
            "%p", "%s", "Hello World", "%u", "0xffffffff", "%x", "%llX", "%c", "X World",
            "%+d", "%-d", "%#o", "%#x", "%hd", "%ld", "%lld", "%zd",
            "%%", "%", "%z", "% d", "%,d", "%'d", "Hello %s World %d !%c", "%5.3s",
            "%-10.5d", "%010d", "%.0f", "%g", "-37.0", "1234.5678", "%G", "%E", "",
            "%*d", "%.*f", "%*.*f", "z%s%*f",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#endif
    /*
        fuzzLoadDir does not recurse; load each test's subdirectory.
     */
    fuzzLoadDir(runner, "corpus/" ORACLE_NAME);

    replay = getenv("FUZZ_REPLAY");
    if (replay) {
        printf("Replay mode: %s\n", replay);
        fuzzClearCorpus(runner);
        fuzzLoadArtifact(runner, replay);
    }
    printf("Oracle: %s\n", ORACLE_NAME);

    crashes = fuzzRun(runner);
    fuzzReport(runner);
    fuzzFree(runner);

    leaked = fuzzLeakCheck();

    return (crashes > 0 || leaked > 0) ? 1 : 0;
}

#endif /* SEC_LIBFUZZER */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */
