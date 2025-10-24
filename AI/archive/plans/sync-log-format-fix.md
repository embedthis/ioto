# Sync Log Fixed-Size Format Documentation

**Date:** 2025-10-10
**Status:** Proposed
**Priority:** Low
**Type:** Documentation / Code Clarity

## Problem Statement

### Current Issue

The sync log in [src/cloud/sync.c](../../../src/cloud/sync.c) uses `int32` type for writing binary length fields to disk. The field sizes should be well-documented for maintainability and clarity.

**Requirements**:
- Sync logs are **local only** - no cross-system sharing needed
- Want **well-documented disk format** for maintainability
- **No byte-order conversion needed** (host byte order is acceptable for local-only files)
- **No endianness handling needed** (files stay on same system)

### Detailed Analysis

#### Current Implementation (Lines 265-424)

**writeSize()** - Line 414-424:
```c
static int writeSize(int len)
{
    int32 len32;

    len32 = (int32) len;
    if (fwrite(&len32, sizeof(len32), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}
```

**writeBlock()** - Line 397-412:
```c
static int writeBlock(cchar *buf)
{
    int32 len;

    len = (int32) slen(buf) + 1;

    if (fwrite(&len, sizeof(len), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    if (fwrite(buf, (size_t) len, 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}
```

**readSize()** - Line 265-274:
```c
static size_t readSize(FILE *fp)
{
    int32 len;

    if (fread(&len, sizeof(len), 1, fp) != 1) {
        //  EOF
        return 0;
    }
    return (size_t) len;
}
```

**readBlock()** - Line 276-302:
```c
static cchar *readBlock(FILE *fp, RBuf *buf)
{
    cchar *result;
    int32   len;

    if (!buf) {
        return NULL;
    }
    //  The length includes a trailing null
    if (fread(&len, sizeof(len), 1, fp) != 1) {
        rError("sync", "Corrupt sync log");
        return 0;
    }
    if (len < 0 || len > DB_MAX_ITEM) {
        rError("sync", "Corrupt sync log");
        return 0;
    }
    if (fread(rGetBufStart(buf), (size_t) len, 1, fp) != 1) {
        rError("sync", "Corrupt sync log");
        return 0;
    }
    // ... rest of function
}
```

### Documentation Issues Identified

#### 1. **int32 Type Clarity**

From [paks/osdep/dist/osdep.h](../../../paks/osdep/dist/osdep.h):
```c
typedef int int32;  // Line 953
```

**Issue**: Using `int32` typedef is less clear than explicit `uint32_t` for documenting disk format.
- `uint32_t` explicitly states size and signedness
- Makes code self-documenting for maintenance
- **Note**: No portability concern since files are local-only

#### 2. **Size Field Documentation**

The current code writes `int32` values directly to disk using `fwrite(&len32, sizeof(len32), 1, fp)`:
- Field sizes should be explicitly documented in code comments
- No endianness conversion needed (local files only)
- **Action needed**: Add clear documentation of on-disk format

#### 3. **Size Calculation Clarity**

Line 381 in `logChange()`:
```c
len = (int) (slen(change->cmd) + slen(change->data) + slen(change->key) + slen(change->updated) + 4);
```

**Analysis**:
- `slen()` returns `ssize` which is 64-bit on all platforms per CLAUDE.md
- Casting to `int` should be documented why this is safe
- The `+ 4` accounts for 4 null terminators (one per string) - should be commented
- **Action needed**: Add comments explaining the size calculation

### Use Case Analysis

Since sync logs are **local-only** files that never move between systems:

| Use Case | Requirements | Current Status |
|----------|--------------|----------------|
| Local read/write on same device | Fixed size, documented format | ✅ Works, needs docs |
| Cross-platform compatibility | Not needed | ✅ N/A |
| Backup/restore on same device | Must survive process restart | ✅ Works |
| Human readability | Not required (binary format) | ✅ Acceptable |
| Performance | Fast I/O with minimal overhead | ✅ Current impl good |

---

## Proposed Solution

### Documentation and Clarity Improvements

Focus on improving code clarity and documentation without changing functionality.

#### 1. **Use Explicit Fixed-Size Types**

Update sync.c to use `uint32_t` instead of `int32` for clarity:
```c
/*
    Sync log format uses 32-bit unsigned integers for all size fields.
    Written in host byte order (local files only - no cross-system compatibility needed).
*/
typedef uint32_t SyncSize;  // Explicitly 32-bit for disk format

#define SYNC_MAX_SIZE  0x7FFFFFFF  // Maximum safe size for any field
```

#### 2. **Document writeSize()**

```c
/*
    Write a 32-bit size field to the sync log.
    Size is written in host byte order (local files only).

    @param len The size value to write (must be <= SYNC_MAX_SIZE)
    @return 0 on success, error code otherwise
*/
static int writeSize(int len)
{
    uint32_t size32;

    if (len < 0 || len > SYNC_MAX_SIZE) {
        rError("sync", "Invalid sync log size: %d", len);
        return R_ERR_BAD_ARGS;
    }

    size32 = (uint32_t) len;

    if (fwrite(&size32, sizeof(uint32_t), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}
```

#### 3. **Document readSize()**

```c
/*
    Read a 32-bit size field from the sync log.
    Size is stored in host byte order (local files only).

    @param fp File pointer to read from
    @return Size value, or 0 on EOF/error
*/
static size_t readSize(FILE *fp)
{
    uint32_t size32;

    if (fread(&size32, sizeof(uint32_t), 1, fp) != 1) {
        //  EOF
        return 0;
    }

    return (size_t) size32;
}
```

#### 4. **Document writeBlock()**

```c
/*
    Write a string block to the sync log.
    Format: [32-bit length][string data including null terminator]

    @param buf String to write (must include null terminator in length calculation)
    @return 0 on success, error code otherwise
*/
static int writeBlock(cchar *buf)
{
    ssize  len;
    uint32_t size32;

    len = slen(buf) + 1;  // +1 for null terminator

    if (len < 0 || len > SYNC_MAX_SIZE) {
        rError("sync", "String too large for sync log: %lld bytes", (long long)len);
        return R_ERR_BAD_ARGS;
    }

    size32 = (uint32_t) len;

    if (fwrite(&size32, sizeof(uint32_t), 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    if (fwrite(buf, (size_t) len, 1, ioto->syncLog) != 1) {
        rError("sync", "Cannot write to sync log");
        return R_ERR_CANT_WRITE;
    }
    return 0;
}
```

#### 5. **Document readBlock()**

```c
/*
    Read a string block from the sync log.
    Format: [32-bit length][string data including null terminator]

    @param fp File pointer to read from
    @param buf Buffer to read into
    @return Pointer to string data, or NULL on error
*/
static cchar *readBlock(FILE *fp, RBuf *buf)
{
    cchar *result;
    uint32_t size32;
    size_t len;

    if (!buf) {
        return NULL;
    }

    // Read 32-bit length field
    if (fread(&size32, sizeof(uint32_t), 1, fp) != 1) {
        rError("sync", "Corrupt sync log - cannot read size");
        return 0;
    }

    len = (size_t) size32;

    if (len > DB_MAX_ITEM) {
        rError("sync", "Corrupt sync log - block too large: %zu", len);
        return 0;
    }

    // Read string data
    if (fread(rGetBufStart(buf), len, 1, fp) != 1) {
        rError("sync", "Corrupt sync log - cannot read data");
        return 0;
    }

    result = rGetBufStart(buf);
    rAdjustBufEnd(buf, (ssize) len);
    rAdjustBufStart(buf, (ssize) rGetBufLength(buf));
    rAddNullToBuf(buf);
    return result;
}
```

#### 6. **Document logChange() Size Calculation**

Add comment at line 381:
```c
/*
    Calculate total size for sync log entry.
    Total = sum of string lengths + 4 null terminators (one per string).
    Safe to cast to int since DB_MAX_ITEM limit ensures no overflow.
*/
len = (int) (slen(change->cmd) + slen(change->data) + slen(change->key) + slen(change->updated) + 4);
```

---

## Implementation Strategy

### Phase 1: Documentation Pass

1. **Update type definitions** to use `uint32_t` instead of `int32`
2. **Add comprehensive function documentation** for all I/O functions
3. **Add inline comments** explaining size calculations
4. **Document file format** in source code header

### Phase 2: Code Review

1. **Review all size calculations** for correctness
2. **Verify bounds checking** on all reads
3. **Confirm error handling** is comprehensive

### Phase 3: Documentation

1. Add sync log format specification to DESIGN.md
2. Document local-only nature of sync logs
3. Add maintenance notes for future developers

---

## On-Disk Format Specification

### Sync Log File Format (Current)

```
+------------------+
| Record 1         |
+------------------+
| Record 2         |
+------------------+
| ...              |
+------------------+
```

#### Record Format
```
+------------------+
| Total Size       | uint32_t (host byte order)
+------------------+
| Command Length   | uint32_t (host byte order)
| Command String   | N bytes (includes null)
+------------------+
| Data Length      | uint32_t (host byte order)
| Data String      | N bytes (includes null)
+------------------+
| Key Length       | uint32_t (host byte order)
| Key String       | N bytes (includes null)
+------------------+
| Updated Length   | uint32_t (host byte order)
| Updated String   | N bytes (includes null)
+------------------+
```

**Notes**:
- All 32-bit size fields stored in **host byte order** (local files only)
- String lengths **include** the trailing null terminator
- Total Size is the sum of all string lengths + 4 null terminators
- Maximum record size: DB_MAX_ITEM (defined in db.h)
- **Portability**: Files are local-only and not moved between systems

---

## Testing Requirements

### Unit Tests

Existing tests in `test/db-sync-*.tst.*` should continue to pass with no changes needed.

Additional verification:
```c
// Verify size field clarity
void testSyncSizeFields(void) {
    // Confirm uint32_t is 4 bytes
    assert(sizeof(uint32_t) == 4);

    // Confirm SYNC_MAX_SIZE is reasonable
    assert(SYNC_MAX_SIZE <= DB_MAX_ITEM);
}

// Test write/read round-trip with documented format
void testSyncLogRoundTrip(void) {
    // Write data using documented functions
    // Read back and verify
    // Confirm size fields are exactly 4 bytes
}
```

### Code Review Checklist

1. **Type consistency**: All size fields use `uint32_t`
2. **Bounds checking**: All reads validate size <= DB_MAX_ITEM
3. **Error messages**: Clear messages for size validation failures
4. **Documentation**: Every function has clear comments explaining format

---

## Performance Considerations

### Current Performance
- Direct binary write with `int32`: minimal overhead
- No byte swapping needed (local files only)
- I/O-bound operation

### After Changes
- Using `uint32_t` instead of `int32`: **no performance change**
- Added bounds checking: negligible overhead
- Added documentation: **no runtime impact**
- **Overall impact**: None - same performance

---

## Risks and Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Breaking existing logs | None | No format change, only documentation |
| Performance regression | None | No algorithm changes |
| Type size assumptions | Low | `uint32_t` is standard-guaranteed 32-bit |
| Documentation drift | Low | Keep docs in source code near functions |

---

## Implementation Checklist

- [ ] Add `SyncSize` typedef using `uint32_t` to sync.c
- [ ] Add `SYNC_MAX_SIZE` constant to sync.c
- [ ] Update `writeSize()` to use `uint32_t` and add documentation
- [ ] Update `readSize()` to use `uint32_t` and add documentation
- [ ] Update `writeBlock()` to use `uint32_t` and add documentation
- [ ] Update `readBlock()` to use `uint32_t` and add documentation
- [ ] Add bounds checking to `writeSize()` and `writeBlock()`
- [ ] Improve error messages for size validation failures
- [ ] Add comment to `logChange()` explaining size calculation
- [ ] Add file format documentation header to sync.c
- [ ] Run existing test suite (`make APP=unit test`)
- [ ] Verify no performance regression
- [ ] Update .agent/designs/DESIGN.md with format specification
- [ ] Code review

---

## Timeline

| Phase | Duration | Milestone |
|-------|----------|-----------|
| Implementation | 2-3 hours | Code complete |
| Testing | 30 minutes | All tests pass |
| Documentation | 30 minutes | Docs updated |
| Code Review | 30 minutes | Approved |
| **Total** | **4-5 hours** | Ready for commit |

---

## References

- [src/cloud/sync.c](../../../src/cloud/sync.c) - Current implementation
- [paks/osdep/dist/osdep.h](../../../paks/osdep/dist/osdep.h) - Type definitions
- C11 Standard - `<stdint.h>` fixed-width integer types

---

## Approval

**Prepared by**: Claude Code
**Date**: 2025-10-10
**Status**: Updated - Documentation-Only Approach

**Summary of Changes**:
- Removed endianness handling (not needed for local files)
- Focus on documentation and code clarity
- Use standard `uint32_t` type for explicitness
- No format changes, backward compatible
