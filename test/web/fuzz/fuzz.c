/*
    fuzz.c - Fuzzing test library implementation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "fuzz.h"

/*********************************** Locals ***********************************/

static FuzzRunner *currentRunner = NULL;
static bool       (*shouldStopCallback)(void) = NULL;

/********************************** Forwards **********************************/

static uint hex2int(char c);
static char *fuzzUnescapeString(cchar *str, size_t *outLen);
static char *generateRandomData(size_t len, bool printable);
static uint getRandom(void);
static void signalHandler(int sig);
static size_t utf8Encode(uint cp, char *buf);

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

PUBLIC void fuzzSetShouldStopCallback(bool (*callback)(void))
{
    shouldStopCallback = callback;
}

PUBLIC int fuzzLoadCorpus(FuzzRunner *runner, cchar *path)
{
    char   *content, *line, *next, *unescaped;
    size_t len, unescapedLen;
    int    count = 0;

    content = rReadFile(path, &len);
    if (!content) {
        return 0;
    }
    for (line = stok(content, "\n", &next); line; line = stok(NULL, "\n", &next)) {
        // Skip comments and blank lines
        line = strim(line, " \t\r\n", 0);
        if (line[0] && line[0] != '#') {
            // Unescape the line to convert \r\n → CR LF, \x00 → null byte, etc.
            unescaped = fuzzUnescapeString(line, &unescapedLen);
            if (unescaped) {
                fuzzAddCorpus(runner, unescaped, unescapedLen);
                rFree(unescaped);
                count++;
            }
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
    size_t corpusLen, len;
    cchar  *input;
    char   *mutated;
    Ticks  deadline, now;
    int    i, maxIterations;
    bool   useDuration;

    if (!runner->oracle) {
        rError("fuzz", "No test oracle configured");
        return -1;
    }
    if (rGetListLength(runner->corpus) == 0) {
        rError("fuzz", "Empty corpus - add at least one test case");
        return -1;
    }
    useDuration = (runner->config.duration > 0);
    if (useDuration) {
        deadline = runner->stats.startTime + runner->config.duration;
        maxIterations = INT_MAX;
    } else {
        deadline = 0;
        maxIterations = runner->config.iterations;
    }
    for (i = 0; i < maxIterations; i++) {
        if (useDuration) {
            now = rGetTicks();
            if (now >= deadline) {
                break;
            }
        }
        if (runner->config.randomize) {
            input = fuzzGetRandomCorpus(runner, &corpusLen);
        } else {
            input = rGetItem(runner->corpus, i % rGetListLength(runner->corpus));
            corpusLen = slen(input);
        }
        if (!input) continue;

        // Check if fuzzing should stop (e.g., server crashed)
        if (shouldStopCallback && shouldStopCallback()) {
            rInfo("fuzz", "Stopping fuzzing early at iteration %d", i);
            break;
        }
        len = corpusLen;
        mutated = (runner->mutator && runner->config.mutate) ?
                  runner->mutator(input, &len) : snclone(input, len);

        if (setjmp(runner->crashJmp) == 0) {
            bool passed = runner->oracle(mutated, len);

            if (!passed) {
                if (fuzzIsUniqueCrash(runner, mutated, len)) {
                    fuzzSaveCrash(runner, mutated, len, 0);
                    runner->stats.crashes++;
                    runner->stats.unique++;

                    // Stop on first error if configured
                    if (runner->config.stop) {
                        rInfo("fuzz", "Stopping on first error at iteration %d", i);
                        rFree(mutated);
                        break;
                    }
                }
            }
            runner->stats.total++;

        } else {
            if (fuzzIsUniqueCrash(runner, mutated, len)) {
                fuzzSaveCrash(runner, mutated, len, runner->signal);
                runner->stats.crashes++;
                runner->stats.unique++;

                // Stop on first error if configured
                if (runner->config.stop) {
                    rInfo("fuzz", "Stopping on first crash at iteration %d", i);
                    rFree(mutated);
                    break;
                }
            }
            runner->crashed = false;
        }
        rFree(mutated);
        if (runner->config.verbose && (i % 1000) == 0) {
            if (useDuration) {
                Ticks elapsed = rGetTicks() - runner->stats.startTime;
                rInfo("fuzz", "Iterations: %d, Elapsed: %.1fs - Crashes: %d (unique: %d)",
                      i, (double) elapsed / 1000.0, runner->stats.crashes, runner->stats.unique);
            } else {
                rInfo("fuzz", "Iterations: %d/%d - Crashes: %d (unique: %d)",
                      i, runner->config.iterations, runner->stats.crashes, runner->stats.unique);
            }
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
    offset = (size_t) (pos - input);
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
    Unescape a string with support for:
    - Standard escapes: \r \n \t \\ \' \"
    - Hex codes: \x00 \xFF
    - Unicode: \u0041 \u{1F600}
    Returns newly allocated unescaped string with actual byte values
 */
static char *fuzzUnescapeString(cchar *str, size_t *outLen)
{
    RBuf   *buf;
    char   *result;
    size_t hexLen, i, len;
    uint   cp;

    if (!str) {
        *outLen = 0;
        return NULL;
    }
    len = slen(str);
    buf = rAllocBuf(len);  // Will be <= original length

    for (i = 0; i < len; i++) {
        if (str[i] == '\\' && i + 1 < len) {
            switch (str[i + 1]) {
            case 'r':
                rPutCharToBuf(buf, '\r');
                i++;
                break;
            case 'n':
                rPutCharToBuf(buf, '\n');
                i++;
                break;
            case 't':
                rPutCharToBuf(buf, '\t');
                i++;
                break;
            case '\\':
                rPutCharToBuf(buf, '\\');
                i++;
                break;
            case '\'':
                rPutCharToBuf(buf, '\'');
                i++;
                break;
            case '"':
                rPutCharToBuf(buf, '"');
                i++;
                break;
            case '0':
                rPutCharToBuf(buf, '\0');
                i++;
                break;
            case 'x':
                // Hex escape: \xHH (2 hex digits)
                if (i + 3 < len && isxdigit((uchar) str[i + 2]) && isxdigit((uchar) str[i + 3])) {
                    uchar byte = (uchar) ((hex2int(str[i + 2]) << 4) | hex2int(str[i + 3]));
                    rPutCharToBuf(buf, byte);
                    i += 3;
                } else {
                    // Invalid hex escape, keep literal
                    rPutCharToBuf(buf, '\\');
                }
                break;
            case 'u':
                // Unicode escape: \uHHHH or \u{HHHHHH}
                if (i + 2 < len && str[i + 2] == '{') { /* } */
                    // Extended form: \u{HHHHHH}
                    hexLen = 0;
                    cp = 0;
                    /* { */
                    for (size_t j = i + 3; j < len && str[j] != '}' && hexLen < 6; j++) {
                        if (isxdigit((uchar) str[j])) {
                            cp = (cp << 4) | hex2int(str[j]);
                            hexLen++;
                        } else {
                            break;
                        }
                    }
                    /* { */
                    if (hexLen > 0 && i + 3 + hexLen < len && str[i + 3 + hexLen] == '}') {
                        // Valid unicode escape - encode as UTF-8
                        char   utf8[5];
                        size_t utf8len = utf8Encode(cp, utf8);
                        rPutBlockToBuf(buf, utf8, utf8len);
                        i += 3 + hexLen + 1;  // Skip \u{HHHHHH}
                    } else {
                        // Invalid, keep literal
                        rPutCharToBuf(buf, '\\');
                    }
                } else if (i + 5 < len) {
                    // Standard form: \uHHHH (4 hex digits)
                    if (isxdigit((uchar) str[i + 2]) && isxdigit((uchar) str[i + 3]) &&
                        isxdigit((uchar) str[i + 4]) && isxdigit((uchar) str[i + 5])) {
                        cp = (hex2int(str[i + 2]) << 12) | (hex2int(str[i + 3]) << 8) |
                             (hex2int(str[i + 4]) << 4) | hex2int(str[i + 5]);
                        // Encode as UTF-8
                        char   utf8[5];
                        size_t utf8len = utf8Encode(cp, utf8);
                        rPutBlockToBuf(buf, utf8, utf8len);
                        i += 5;  // Skip \uHHHH
                    } else {
                        // Invalid, keep literal
                        rPutCharToBuf(buf, '\\');
                    }
                } else {
                    // Invalid, keep literal
                    rPutCharToBuf(buf, '\\');
                }
                break;
            default:
                // Unknown escape, keep literal backslash
                rPutCharToBuf(buf, '\\');
                break;
            }
        } else {
            rPutCharToBuf(buf, str[i]);
        }
    }
    rAddNullToBuf(buf);
    *outLen = rGetBufLength(buf);
    result = rBufToStringAndFree(buf);
    return result;
}

/*
    Convert hex character to integer value
 */
static uint hex2int(char c)
{
    if (c >= '0' && c <= '9') return (uint) (c - '0');
    if (c >= 'a' && c <= 'f') return (uint) (c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint) (c - 'A' + 10);
    return 0;
}

/*
    Encode Unicode codepoint as UTF-8
    Returns number of bytes written (1-4)
 */
static size_t utf8Encode(uint cp, char *buf)
{
    if (cp <= 0x7F) {
        // 1-byte sequence: 0xxxxxxx
        buf[0] = (char) cp;
        return 1;
    } else if (cp <= 0x7FF) {
        // 2-byte sequence: 110xxxxx 10xxxxxx
        buf[0] = (char) (0xC0 | (cp >> 6));
        buf[1] = (char) (0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
        buf[0] = (char) (0xE0 | (cp >> 12));
        buf[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char) (0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        buf[0] = (char) (0xF0 | (cp >> 18));
        buf[1] = (char) (0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char) (0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char) (0x80 | (cp & 0x3F));
        return 4;
    }
    // Invalid codepoint, return replacement character �
    buf[0] = (char) 0xEF;
    buf[1] = (char) 0xBF;
    buf[2] = (char) 0xBD;
    return 3;
}

/************************* Server Crash Detection *****************************/

PUBLIC pid_t fuzzGetServerPid(void)
{
    static pid_t cachedPid = 0;
    FILE         *fp;
    char         line[32];

    if (cachedPid != 0) {
        return cachedPid;
    }
    fp = fopen(".testme/server.pid", "r");
    if (!fp) {
        return 0;
    }
    if (fgets(line, sizeof(line), fp)) {
        cachedPid = (pid_t) atoi(line);
    }
    fclose(fp);
    return cachedPid;
}

PUBLIC bool fuzzIsServerAlive(pid_t pid)
{
    if (pid <= 0) {
        return false;
    }
#if _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process == NULL) {
        return false;
    }
    DWORD exitCode;
    bool  alive = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
#else
    return kill(pid, 0) == 0;
#endif
}

PUBLIC void fuzzReportServerCrash(cchar *input, size_t len)
{
    char   *hash, *filename, *metafile, *content;
    FILE   *fp;
    size_t i;

    hash = fuzzHash(input, len);
    filename = sfmt("crashes/server/crash-%s.txt", hash);
    metafile = sfmt("crashes/server/crash-%s.meta", hash);

    mkdir("crashes", 0755);
    mkdir("crashes/server", 0755);

    fp = fopen(filename, "w");
    if (fp) {
        fprintf(fp, "=== SERVER CRASH ===\n");
        fprintf(fp, "Timestamp: %lld\n", (long long) rGetTicks());
        fprintf(fp, "Input length: %zu\n", len);
        fprintf(fp, "Input hash: %s\n", hash);
        fprintf(fp, "\n--- Input Data (hex) ---\n");

        for (i = 0; i < len; i++) {
            fprintf(fp, "%02x ", (unsigned char) input[i]);
            if ((i + 1) % 16 == 0) fprintf(fp, "\n");
        }
        fprintf(fp, "\n\n--- Input Data (printable) ---\n");
        for (i = 0; i < len; i++) {
            char c = input[i];
            if (c >= 32 && c < 127) {
                fputc(c, fp);
            } else {
                fprintf(fp, "\\x%02x", (unsigned char) c);
            }
        }
        fprintf(fp, "\n");
        fclose(fp);
    }
    content = sfmt("Signal: 0 (server crash)\nLength: %lld\nHash: %s\nTime: %lld\n",
                   (long long) len, hash, (long long) rGetTicks());
    rWriteFile(metafile, content, slen(content), 0644);
    rInfo("fuzz", "Server crash input saved to: %s", filename);

    rFree(hash);
    rFree(filename);
    rFree(metafile);
    rFree(content);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
