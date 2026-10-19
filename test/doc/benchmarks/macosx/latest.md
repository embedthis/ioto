# Web Server Benchmark Results

## System Configuration

- **Version:** 1.0.0-dev
- **Timestamp:** Sep 23, 2026 02:01 AM UTC
- **Platform:** macosx 25.5.0 (arm64)
- **Profile:** release
- **TLS:** openssl
- **Total Duration:** 300 seconds (30s soak + 270s bench)
- **Initial Memory (after soak):** 7.59 MB
- **Final Memory:** 8.36 MB
- **Memory Delta:** +0.77 MB
## Performance Results

| Category | Test | Req/Sec | Avg Latency (ms) | P95 (ms) | P99 (ms) | Min (ms) | Max (ms) | Bytes | Errors | Iterations |
|----------|------|---------|------------------|----------|----------|----------|----------|-------|--------|------------|
| **Throughput** | | | | | | | | | | |
| | multithread | 23568 | 1.73 | 0.0 | 0.0 | 0.0 | 0.0 | 506.3 MB | 0 | 518497 |
| | singlethread | 13173 | 0.25 | 0.0 | 0.0 | 0.0 | 0.0 | 283.0 MB | 0 | 289815 |
| **Static Files (URL Library)** | | | | | | | | | | |
| | 1KB_warm | 12011 | 0.08 | 1.0 | 1.0 | 0.0 | 19.0 | 52.7 MB | 0 | 53978 |
| | 10KB_warm | 11203 | 0.09 | 1.0 | 1.0 | 0.0 | 52.0 | 491.6 MB | 0 | 50339 |
| | 100KB_warm | 9622 | 0.10 | 1.0 | 1.0 | 0.0 | 15.0 | 1057.1 MB | 0 | 10825 |
| | 1MB_warm | 4637 | 0.22 | 1.0 | 1.0 | 0.0 | 17.0 | 5213.0 MB | 0 | 5213 |
| | 1KB_cold | 1797 | 0.56 | 2.0 | 9.0 | 0.0 | 20.0 | 800.0 KB | 0 | 800 |
| | 10KB_cold | 2030 | 0.49 | 1.0 | 7.0 | 0.0 | 16.0 | 7.8 MB | 0 | 800 |
| | 100KB_cold | 1369 | 0.73 | 3.0 | 17.0 | 0.0 | 19.0 | 19.5 MB | 0 | 200 |
| | 1MB_cold | 909 | 1.10 | 4.0 | 13.0 | 0.0 | 21.0 | 200.0 MB | 0 | 200 |
| **HTTPS (URL Library)** | | | | | | | | | | |
| | 1KB_warm | 10162 | 0.10 | 1.0 | 1.0 | 0.0 | 11.0 | 44.6 MB | 0 | 45669 |
| | 10KB_warm | 9821 | 0.10 | 1.0 | 1.0 | 0.0 | 35.0 | 430.7 MB | 0 | 44100 |
| | 100KB_warm | 5227 | 0.19 | 1.0 | 1.0 | 0.0 | 11.0 | 573.8 MB | 0 | 5876 |
| | 1MB_warm | 1268 | 0.79 | 1.0 | 1.0 | 0.0 | 6.0 | 1427.0 MB | 0 | 1427 |
| | 1KB_cold | 295 | 3.39 | 9.0 | 20.0 | 1.0 | 39.0 | 800.0 KB | 0 | 800 |
| | 10KB_cold | 297 | 3.36 | 9.0 | 19.0 | 1.0 | 48.0 | 7.8 MB | 0 | 800 |
| | 100KB_cold | 255 | 3.91 | 9.0 | 18.0 | 2.0 | 54.0 | 19.5 MB | 0 | 200 |
| | 1MB_cold | 193 | 5.17 | 14.0 | 29.0 | 2.0 | 32.0 | 200.0 MB | 0 | 200 |
| **Static Files (Raw HTTP)** | | | | | | | | | | |
| | 1KB_raw_warm | 14720 | 0.07 | 1.0 | 1.0 | 0.0 | 40.0 | 64.6 MB | 0 | 66197 |
| | 10KB_raw_warm | 10576 | 0.09 | 1.0 | 1.0 | 0.0 | 50.0 | 466.8 MB | 0 | 47804 |
| | 100KB_raw_warm | 11110 | 0.09 | 1.0 | 1.0 | 0.0 | 9.0 | 1219.5 MB | 0 | 12488 |
| | 1MB_raw_warm | 3431 | 0.29 | 1.0 | 1.0 | 0.0 | 42.0 | 3854.0 MB | 0 | 3854 |
| | 1KB_raw_cold | 2168 | 0.46 | 1.0 | 6.0 | 0.0 | 15.0 | 800.0 KB | 0 | 800 |
| | 10KB_raw_cold | 2168 | 0.46 | 1.0 | 6.0 | 0.0 | 27.0 | 7.8 MB | 0 | 800 |
| | 100KB_raw_cold | 1307 | 0.77 | 1.0 | 19.0 | 0.0 | 73.0 | 19.5 MB | 0 | 200 |
| | 1MB_raw_cold | 1081 | 0.93 | 2.0 | 16.0 | 0.0 | 16.0 | 200.0 MB | 0 | 200 |
| **Static Files (Raw HTTPS)** | | | | | | | | | | |
| | 1KB_raw_warm | 11785 | 0.08 | 1.0 | 1.0 | 0.0 | 32.0 | 51.8 MB | 0 | 53001 |
| | 10KB_raw_warm | 11723 | 0.09 | 1.0 | 1.0 | 0.0 | 35.0 | 514.7 MB | 0 | 52710 |
| | 100KB_raw_warm | 7347 | 0.14 | 1.0 | 1.0 | 0.0 | 8.0 | 806.5 MB | 0 | 8259 |
| | 1MB_raw_warm | 1528 | 0.65 | 1.0 | 1.0 | 0.0 | 20.0 | 1719.0 MB | 0 | 1719 |
| | 1KB_raw_cold | 305 | 3.27 | 8.0 | 17.0 | 1.0 | 42.0 | 800.0 KB | 0 | 800 |
| | 10KB_raw_cold | 282 | 3.53 | 9.0 | 20.0 | 1.0 | 46.0 | 7.8 MB | 0 | 800 |
| | 100KB_raw_cold | 278 | 3.60 | 9.0 | 17.0 | 2.0 | 23.0 | 19.5 MB | 0 | 200 |
| | 1MB_raw_cold | 236 | 4.24 | 10.0 | 22.0 | 2.0 | 24.0 | 200.0 MB | 0 | 200 |
| **WebSockets** | | | | | | | | | | |
| | websocket_echo | 12153 | 0.08 | 0.0 | 1.0 | 0.0 | 96.0 | 10.6 MB | 0 | 532532 |
| **PUT Uploads** | | | | | | | | | | |
| | 1KB_warm | 4568 | 0.22 | 1.0 | 1.0 | 0.0 | 37.0 | 16.4 MB | 0 | 16976 |
| | 10KB_warm | 4291 | 0.23 | 1.0 | 1.0 | 0.0 | 30.0 | 160.1 MB | 0 | 16350 |
| | 100KB_warm | 3445 | 0.29 | 1.0 | 1.0 | 0.0 | 12.0 | 331.1 MB | 0 | 3387 |
| | 1MB_warm | 1253 | 0.80 | 2.0 | 4.0 | 0.0 | 35.0 | 1237.6 MB | 0 | 1266 |
| | 1KB_cold | 595 | 1.68 | 6.0 | 17.0 | 0.0 | 63.0 | 793.8 KB | 0 | 800 |
| | 10KB_cold | 644 | 1.55 | 5.0 | 26.0 | 0.0 | 55.0 | 7.8 MB | 0 | 800 |
| | 100KB_cold | 1176 | 0.85 | 2.0 | 9.0 | 0.0 | 9.0 | 19.6 MB | 0 | 200 |
| | 1MB_cold | 727 | 1.38 | 3.0 | 5.0 | 0.0 | 8.0 | 195.5 MB | 0 | 200 |
| **Multipart Uploads** | | | | | | | | | | |
| | 1KB_warm | 6838 | 0.15 | 1.0 | 1.0 | 0.0 | 32.0 | 27.1 MB | 0 | 27941 |
| | 10KB_warm | 6107 | 0.16 | 1.0 | 1.0 | 0.0 | 54.0 | 243.1 MB | 0 | 24833 |
| | 100KB_warm | 4165 | 0.24 | 1.0 | 1.0 | 0.0 | 68.0 | 413.4 MB | 0 | 4228 |
| | 1MB_warm | 1633 | 0.61 | 1.0 | 1.0 | 0.0 | 17.0 | 1606.1 MB | 0 | 1643 |
| | 1KB_cold | 1713 | 0.58 | 1.0 | 6.0 | 0.0 | 17.0 | 793.8 KB | 0 | 800 |
| | 10KB_cold | 3433 | 0.29 | 1.0 | 1.0 | 0.0 | 4.0 | 7.8 MB | 0 | 800 |
| | 100KB_cold | 2352 | 0.42 | 1.0 | 2.0 | 0.0 | 2.0 | 19.6 MB | 0 | 200 |
| | 1MB_cold | 947 | 1.05 | 2.0 | 5.0 | 0.0 | 8.0 | 195.5 MB | 0 | 200 |
| **Auth (Digest)** | | | | | | | | | | |
| | digest_with_session | 9107 | 0.11 | 1.0 | 1.0 | 0.0 | 13.0 | 537.1 KB | 0 | 5000 |
| | digest_cold | 631 | 1.58 | 6.0 | 18.0 | 0.0 | 38.0 | 214.8 KB | 0 | 2000 |
| **Actions** | | | | | | | | | | |
| | simple_warm | 22944 | 0.04 | 0.0 | 1.0 | 0.0 | 48.0 | 2.0 MB | 0 | 257465 |
| | simple_cold | 2403 | 0.42 | 1.0 | 4.0 | 0.0 | 46.0 | 15.6 KB | 0 | 2000 |
| **Mixed Workload** | | | | | | | | | | |
| | mixed_workload | 11759 | 0.09 | 1.0 | 1.0 | 0.0 | 54.0 | 475.3 MB | 0 | 264300 |
| **Connections** | | | | | | | | | | |
| | plain | 1369 | 0.73 | 2.0 | 13.0 | 0.0 | 85.0 | 0 | 0 | 2000 |
| | tls_cold | 298 | 3.34 | 10.0 | 21.0 | 1.0 | 58.0 | 0 | 0 | 2000 |
| | tls_session | 290 | 3.44 | 8.0 | 23.0 | 1.0 | 77.0 | 0 | 0 | 2000 |

## Notes

- **Max Throughput test**: Uses wrk benchmark tool with 12 threads, 40 connections
- **All other tests**: Run with 1 CPU core for the server and 1 CPU core for the client
- **Warm tests**: Reuse connection/socket for all requests
- **Cold tests**: New connection/socket for each request
- **Raw tests**: Direct socket I/O bypassing URL library (shows true server performance)
- **URL Library tests**: Standard HTTP client (includes client overhead)
- All latency values are in milliseconds
- Bytes column shows total data transferred during test
