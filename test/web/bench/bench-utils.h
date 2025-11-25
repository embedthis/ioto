/*
    bench-utils.h - Benchmark utility functions and data structures

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_BENCH_UTILS
#define _h_BENCH_UTILS 1

/********************************** Includes **********************************/

#include "r.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************** Types ************************************/

/**
 * Benchmark result structure
 * Stores timing and statistical data for a benchmark run
 */
typedef struct BenchResult {
    char *name;               // Benchmark name
    int iterations;           // Number of iterations run
    Ticks totalTime;          // Total time (milliseconds)
    Ticks minTime;            // Minimum latency (ms)
    Ticks maxTime;            // Maximum latency (ms)
    double avgTime;           // Average latency (ms)
    double p95Time;           // 95th percentile (ms)
    double p99Time;           // 99th percentile (ms)
    double requestsPerSec;    // Throughput
    int64 bytesTransferred;   // Total bytes
    int errors;               // Error count
    RList *samples;           // Individual timing samples for percentiles
} BenchResult;

/**
 * Connection context for managing URL or raw socket connections
 * Handles warm (reused) and cold (new) connection patterns
 */
typedef struct {
    Url *up;                  // URL connection (NULL if not allocated)
    RSocket *sp;              // Raw socket connection (NULL if not allocated)
    bool reuseConnection;     // True for warm (reuse), false for cold (new each time)
    bool useSocket;           // True for raw socket mode, false for URL mode
    bool useTls;              // True for TLS/HTTPS connections
    Ticks timeout;            // Request timeout in milliseconds
    cchar *host;              // Host for socket connections
    int port;                 // Port for socket connections
} ConnectionCtx;

/**
 * Request execution result
 * Contains status, timing, and data transfer information
 */
typedef struct {
    int status;               // HTTP status code
    ssize bytes;              // Bytes transferred
    Ticks elapsed;            // Elapsed time in milliseconds
    bool success;             // True if request succeeded
} RequestResult;

#define BENCH_MAX_RESULTS 8   // Maximum results per benchmark group

/**
 * Benchmark context for unified result processing
 * Consolidates error counting, logging, and result recording
 * A single global instance is shared across all benchmark functions
 */
typedef struct BenchContext {
    // Global state (persistent across all benchmarks)
    bool fatalError;          // Fatal error occurred, stop all benchmarks
    bool stopOnErrors;        // Stop on first error
    int totalErrors;          // Total errors across all benchmarks

    // Per-benchmark counters (reset for each benchmark)
    int totalRequests;        // Total requests made in current benchmark
    int errorCount;           // Errors in current benchmark
    int seq;                  // Sequence counter for unique IDs

    // Configuration
    cchar *category;          // Category name for logging (e.g., "Static file")
    bool recordResults;       // Whether to record results

    // Duration allocation
    Ticks duration;           // Total benchmark duration
    double totalUnits;        // Total weighted units for duration allocation

    // Results tracking
    BenchResult *results[BENCH_MAX_RESULTS];  // Results array (embedded)
    int resultCount;          // Number of results in array
    int resultOffset;         // Offset into results array
    int classIndex;           // Current class index within results

    // Connection context (for cleanup on fatal error)
    ConnectionCtx *connCtx;   // Connection to cleanup on fatal

    // Bytes for current request
    ssize bytes;              // Bytes transferred for current request
} BenchContext;

// Global benchmark context (defined in bench.tst.c)
extern BenchContext *bctx;

/********************************** Prototypes ********************************/

/*
    Connection Management
 */

/**
 * Create a URL-based connection context
 * @param warm True for warm (reused) connections, false for cold (new each time)
 * @param timeout Request timeout in milliseconds
 * @return Allocated connection context
 */
extern ConnectionCtx *createConnectionCtx(bool warm, Ticks timeout);

/**
 * Create a raw socket connection context
 * @param warm True for warm (reused) connections, false for cold (new each time)
 * @param timeout Request timeout in milliseconds
 * @param host Host to connect to
 * @param port Port to connect to
 * @param useTls True for TLS connections
 * @return Allocated connection context
 */
extern ConnectionCtx *createSocketCtx(bool warm, Ticks timeout, cchar *host, int port, bool useTls);

/**
 * Get a URL connection from context
 * For warm connections, returns existing connection
 * For cold connections, allocates a new connection
 * @param ctx Connection context
 * @return URL connection
 */
extern Url *getConnection(ConnectionCtx *ctx);

/**
 * Get a raw socket from context
 * For warm connections, returns existing socket
 * For cold connections, allocates a new socket
 * @param ctx Connection context
 * @return Raw socket
 */
extern RSocket *getSocket(ConnectionCtx *ctx);

/**
 * Release a connection (URL or socket)
 * For warm connections, keeps connection open
 * For cold connections, closes and frees connection
 * @param ctx Connection context
 */
extern void releaseConnection(ConnectionCtx *ctx);

/**
 * Free connection context and any allocated connections
 * @param ctx Connection context
 */
extern void freeConnectionCtx(ConnectionCtx *ctx);

/**
 * Execute a request using connection context
 * Handles connection allocation, request execution, and cleanup
 * @param ctx Connection context
 * @param method HTTP method (GET, POST, PUT, DELETE, etc.)
 * @param url Request URL
 * @param data Request body data (or NULL)
 * @param dataLen Length of request body
 * @param headers Additional headers (or NULL)
 * @return Request result with status, bytes, elapsed time
 */
extern RequestResult executeRequest(ConnectionCtx *ctx, cchar *method, cchar *url,
                                     cchar *data, size_t dataLen, cchar *headers);

/**
 * Execute a raw socket HTTP request
 * Handles connection, request send, response read, and cleanup
 * @param ctx Socket connection context (must be created with createSocketCtx)
 * @param request Pre-formatted HTTP request string
 * @param expectedSize Expected response body size
 * @return Request result with status, bytes, elapsed time
 */
extern RequestResult executeRawRequest(ConnectionCtx *ctx, cchar *request, ssize expectedSize);

/*
    BenchContext - Unified Result Processing
 */

/**
 * Initialize a benchmark context
 * Preserves recordResults from ctx (set before calling this function)
 * @param ctx Context to initialize
 * @param category Category name for logging (e.g., "Static file")
 * @param description Description for tinfo output (e.g., "Benchmarking static files...")
 */
extern void initBenchContext(BenchContext *ctx, cchar *category, cchar *description);

/**
 * Process a request result - handles error counting, logging, and recording
 * Updates totalRequests, errorCount, totalErrors, and records timing
 * Calculates elapsed time and success based on status code
 * @param ctx Benchmark context
 * @param result Request result (status set, elapsed/success calculated by this function)
 * @param url Request URL (for error logging)
 * @param startTime Request start time for elapsed calculation
 * @return true if benchmark should continue, false if fatal error occurred
 */
extern bool processResponse(BenchContext *ctx, RequestResult *result, cchar *url, Ticks startTime);

/**
 * Finish benchmark context - logs warning if errors occurred and finalizes results
 * @param ctx Benchmark context
 * @param count Number of results to finalize (0 to skip finalization)
 * @param groupName Test group name for saving results (NULL to skip finalization)
 */
extern void finishBenchContext(BenchContext *ctx, int count, cchar *groupName);

/*
    Error Reporting
 */

/**
 * Log a request error with consistent formatting
 * @param benchName Benchmark name (e.g., "Static files", "Auth")
 * @param url Request URL
 * @param status HTTP status code
 * @param errorCount Current error count
 * @param recordResults True if recording results (limits error logging)
 */
extern void logRequestError(cchar *benchName, cchar *url, int status, int errorCount, bool recordResults);

/*
    Duration and Timing
 */

/**
 * Configure duration-based benchmarking from TESTME_DURATION environment variable
 * Divides total duration into soak (10%) and benchmark (90%) phases
 * Benchmark time is divided equally among test groups
 * @param numGroups Number of test groups to allocate benchmark time among
 */
extern void configureDuration(int numGroups);

/**
 * Get soak phase duration
 * @return Soak duration in milliseconds
 */
extern Ticks getSoakDuration(void);

/**
 * Get benchmark duration per group
 * @return Duration in milliseconds allocated to each test group
 */
extern Ticks getBenchDuration(void);

/**
 * Create a new benchmark result structure
 * @param name Benchmark name
 * @return Allocated benchmark result structure
 */
extern BenchResult *createBenchResult(cchar *name);

/**
 * Free a benchmark result structure
 * @param result Benchmark result to free
 */
extern void freeBenchResult(BenchResult *result);

/**
 * Record a timing sample
 * @param result Benchmark result structure
 * @param elapsed Elapsed time in milliseconds
 */
extern void recordTiming(BenchResult *result, Ticks elapsed);

/*
    Result Management

    Note: Uses the global bctx context for stopOnErrors, fatalError, and totalErrors
 */

/**
 * Initialize a benchmark result (NULL-safe wrapper for createBenchResult)
 * @param name Benchmark name
 * @param recordResults True to create result, false to return NULL
 * @param description Optional description to log via tinfo when recordResults is true (NULL to skip)
 * @return Benchmark result or NULL if not recording
 */
extern BenchResult *initResult(cchar *name, bool recordResults, cchar *description);

/**
 * Record a request result
 * Updates iterations, timing, bytes, and errors
 * @param result Benchmark result (NULL-safe)
 * @param isSuccess True if request succeeded
 * @param elapsed Elapsed time in milliseconds
 * @param bytes Bytes transferred
 */
extern void recordRequest(BenchResult *result, bool isSuccess, Ticks elapsed, ssize bytes);

/**
 * Finalize benchmark results
 * Calculates stats, prints results, saves to JSON, and frees memory
 * @param results Array of benchmark results
 * @param count Number of results in array
 * @param groupName Test group name (e.g., "static_files", "auth")
 */
extern void finalizeResults(BenchResult **results, int count, cchar *groupName);

/**
 * Calculate statistics from recorded samples
 * Computes min, max, avg, p95, p99, and requests/sec
 * @param result Benchmark result structure
 */
extern void calculateStats(BenchResult *result);

/**
 * Print benchmark results to console
 * @param result Benchmark result structure
 */
extern void printBenchResult(BenchResult *result);

/**
 * Save benchmark group results to JSON
 * Appends results to the global results structure
 * @param groupName Test group name (e.g., "static_files", "uploads")
 * @param results Array of benchmark results
 * @param count Number of results in array
 */
extern void saveBenchGroup(cchar *groupName, BenchResult **results, int count);

/**
 * Save final results to JSON file
 * Writes complete results with metadata to doc/benchmarks/latest.json5
 */
extern void saveFinalResults(void);

/**
 * Get process memory size for a specific PID
 * @param pid Process ID (0 for current process)
 * @return Memory size in bytes (resident set size)
 */
extern int64 getProcessMemorySize(int pid);

/**
 * Record initial memory size (after soak phase)
 * Prints and stores the initial memory size
 */
extern void recordInitialMemory(void);

/**
 * Record final memory size (at benchmark completion)
 * Prints and stores the final memory size
 */
extern void recordFinalMemory(void);

/*
    Raw Socket Utilities
 */

/**
 * Extract Content-Length from raw HTTP headers
 * @param headers Raw HTTP header buffer
 * @param len Length of header buffer
 * @return Content length value, or -1 if not found or invalid
 */
extern ssize parseContentLength(cchar *headers, size_t len);

/**
 * Read HTTP headers from socket until \r\n\r\n delimiter
 * @param sp Socket to read from
 * @param buf Buffer to store headers
 * @param bufsize Size of buffer
 * @param deadline Timeout deadline
 * @param bodyStart Returns offset where body begins (after \r\n\r\n)
 * @param bodyLen Returns how much body data was read with headers
 * @return Total bytes read, or -1 on error
 */
extern ssize readHeaders(RSocket *sp, char *buf, size_t bufsize, Ticks deadline, ssize *bodyStart, ssize *bodyLen);

#ifdef __cplusplus
}
#endif
#endif /* _h_BENCH_UTILS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
