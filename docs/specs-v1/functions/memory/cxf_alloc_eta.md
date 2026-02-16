# cxf_alloc_eta

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Arena-based allocator for eta vectors used in the Product Form of the Inverse basis factorization method. Provides efficient allocation of many small eta vectors during simplex iteration by maintaining a pool of pre-allocated chunks. Minimizes malloc/free overhead and enables bulk deallocation at refactorization.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment for memory allocation | Valid CxfEnv pointer | Yes |
| buffer | EtaBuffer* | Eta buffer managing chunk pool | Valid EtaBuffer pointer | Yes |
| size | size_t | Number of bytes to allocate | > 0 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void* | Pointer to allocated memory from chunk pool, or NULL on failure |

### 2.3 Side Effects

- May allocate new chunk if current chunk has insufficient space
- Updates buffer->bytesUsed (position in active chunk)
- Updates buffer->activeChunk (if new chunk allocated)
- Updates buffer->currentChunkSize (exponential growth strategy)
- Links new chunk to chunk chain (for bulk cleanup)

## 3. Contract

### 3.1 Preconditions

- [ ] buffer must be valid EtaBuffer pointer
- [ ] buffer must be initialized (via cxf_init_eta_buffer)
- [ ] size must be greater than 0
- [ ] env must be valid CxfEnv pointer

### 3.2 Postconditions

- [ ] If successful: Returns pointer to at least 'size' bytes in chunk pool
- [ ] If successful: buffer->bytesUsed increased by 'size'
- [ ] If successful: Memory is available for use until next buffer reset/free
- [ ] If new chunk allocated: Chunk is linked to buffer's chunk list
- [ ] If new chunk allocated: buffer->currentChunkSize updated for next allocation
- [ ] If failed: Returns NULL and buffer state unchanged

### 3.3 Invariants

- [ ] All chunks are linked in a list for bulk deallocation
- [ ] bytesUsed never exceeds activeChunk capacity
- [ ] Chunk sizes grow exponentially up to maximum (64KB)

## 4. Algorithm

### 4.1 Overview

The allocator implements an arena (bump-pointer) strategy with exponential chunk growth. It first attempts to allocate from the active chunk's remaining space (fast path). If insufficient space, it allocates a new chunk sized to accommodate the request or the next growth size (whichever is larger), links it to the chain, and updates growth parameters. The chunk size doubles with each allocation up to a maximum of 64KB, balancing memory efficiency with allocation frequency.

### 4.2 Detailed Steps

1. Validate buffer pointer (return NULL if NULL)
2. Get current active chunk and bytes used
3. **Fast path:** Check if request fits in active chunk's remaining space
   - If yes: Return pointer at (chunk_data + bytesUsed), update bytesUsed, done
4. **Slow path:** Need new chunk
5. Determine chunk size: max(requested size, currentChunkSize)
6. Allocate chunk header structure (typically 24 bytes)
7. Allocate chunk data buffer of determined size
8. If allocation failed: Return NULL
9. Initialize chunk metadata (capacity, next=NULL)
10. Link new chunk to chain:
    - If activeChunk exists: activeChunk->next = newChunk
    - If no activeChunk: buffer->firstChunk = newChunk
11. Update buffer state:
    - buffer->activeChunk = newChunk
    - buffer->bytesUsed = size (position in new chunk)
12. Update growth strategy:
    - nextSize = currentChunkSize * 2
    - If nextSize < minChunkSize: use minChunkSize
    - If nextSize > 64KB: cap at 64KB
    - buffer->currentChunkSize = nextSize
13. Return pointer to chunk data

### 4.3 Pseudocode (if needed)

```
FUNCTION cxf_alloc_eta(env, buffer, size) → pointer or NULL
  IF buffer = NULL THEN
    RETURN NULL
  END IF

  activeChunk ← buffer.activeChunk
  bytesUsed ← buffer.bytesUsed

  # Fast path: allocate from current chunk
  IF activeChunk ≠ NULL AND size ≤ (activeChunk.capacity - bytesUsed) THEN
    ptr ← activeChunk.data + bytesUsed
    buffer.bytesUsed ← bytesUsed + size
    RETURN ptr
  END IF

  # Slow path: allocate new chunk
  chunkSize ← max(size, buffer.currentChunkSize)

  newChunk ← cxf_calloc(env, 1, CHUNK_HEADER_SIZE)
  IF newChunk = NULL THEN
    RETURN NULL
  END IF

  newChunk.data ← cxf_malloc(env, chunkSize)
  IF newChunk.data = NULL THEN
    cxf_free(newChunk)
    RETURN NULL
  END IF

  newChunk.capacity ← chunkSize
  newChunk.next ← NULL

  # Link to chain
  IF activeChunk ≠ NULL THEN
    activeChunk.next ← newChunk
  ELSE
    buffer.firstChunk ← newChunk
  END IF

  # Update buffer state
  buffer.activeChunk ← newChunk
  buffer.bytesUsed ← size

  # Exponential growth for next chunk
  nextSize ← chunkSize * 2
  IF nextSize < buffer.minChunkSize THEN
    nextSize ← buffer.minChunkSize
  END IF
  IF nextSize > MAX_CHUNK_SIZE THEN
    nextSize ← MAX_CHUNK_SIZE
  END IF
  buffer.currentChunkSize ← nextSize

  RETURN newChunk.data
END FUNCTION
```

### 4.4 Mathematical Foundation (if applicable)

**Exponential Growth:**
- Chunk size sequence: S₀, S₁, S₂, ... where Sᵢ₊₁ = min(2·Sᵢ, MAX)
- If S₀ = 4KB and MAX = 64KB: 4KB → 8KB → 16KB → 32KB → 64KB → 64KB (capped)
- Total chunks for n allocations: O(log n) in ideal case
- Total memory overhead: O(log n) × chunk_size ≈ O(total_allocation_size)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - fast path, allocation fits in active chunk
- **Average case:** O(1) amortized - occasional chunk allocation amortized over many fast-path allocations
- **Worst case:** O(1) - single chunk allocation

Where:
- Exponential growth ensures infrequent chunk allocations
- Amortized analysis: N allocations require O(log N) chunk allocations = O(1) amortized

### 5.2 Space Complexity

- **Auxiliary space:** O(log N) - number of chunks grows logarithmically
- **Total space:** O(Σ size) - sum of all allocation requests
- **Overhead:** O(log N) × 24 bytes for chunk headers + unused space in last chunk

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| buffer is NULL | N/A (returns NULL) | Invalid buffer pointer |
| Chunk allocation fails | N/A (returns NULL) | Out of memory |
| Data allocation fails | N/A (returns NULL) | Out of memory |

### 6.2 Error Behavior

Returns NULL on any error. Partially allocated resources (chunk header without data) are cleaned up before returning. Buffer state is unchanged on error.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| First allocation | buffer empty, size=100 | Allocate first chunk, return pointer |
| Request larger than chunk | size > currentChunkSize | Allocate chunk of requested size |
| Maximum chunk size reached | currentChunkSize = 64KB | Future chunks also 64KB (no further growth) |
| Exact fit in active chunk | bytesUsed + size = capacity | Allocate from active, next request needs new chunk |
| Very large request | size = 1MB | Allocate 1MB chunk, next chunk still grows normally |

## 8. Thread Safety

**Thread-safe:** Conditionally

Not thread-safe by design. Each solver thread has its own EtaBuffer. No synchronization within this function. Caller must ensure:
- Single-threaded access to buffer
- No concurrent allocations from same buffer

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_calloc | Memory | Allocate chunk header (zero-initialized) |
| cxf_malloc | Memory | Allocate chunk data buffer |
| cxf_free | Memory | Clean up on partial allocation failure |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_basis_update | Solver | Allocate eta vector during basis update |
| cxf_simplex_step | Solver | Create eta vectors during pivots |
| cxf_basis_refactor | Solver | Build eta list during refactorization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_init_eta_buffer | Initializes buffer before use |
| cxf_free_eta_buffer | Frees all chunks in bulk |
| cxf_reset_eta_buffer | Resets buffer for reuse without freeing |
| cxf_malloc | Lower-level allocation function |

## 11. Design Notes

### 11.1 Design Rationale

Arena allocation is optimal for eta vectors because:
1. **Many small allocations** - Hundreds per refactorization cycle
2. **Temporary lifetime** - All freed together at refactorization
3. **Predictable pattern** - Allocation-only, no individual frees
4. **Performance critical** - Hot path in simplex algorithm

Exponential growth balances:
- Small initial chunks (minimize waste for small problems)
- Large later chunks (minimize allocation overhead for large problems)
- Capped growth (prevent excessive memory use)

### 11.2 Performance Considerations

**Fast path performance:**
- Pointer arithmetic: ~1 nanosecond
- Update bytesUsed: ~1 nanosecond
- Total: ~2-5 nanoseconds (extremely fast)

**Slow path performance:**
- Chunk header allocation: ~50-100 nanoseconds
- Data allocation: ~100-500 nanoseconds
- Linking and updates: ~10 nanoseconds
- Total: ~200-600 nanoseconds (infrequent)

**Amortized:** With 50 allocations between chunk allocations, amortized cost ≈ 10 nanoseconds/allocation.

### 11.3 Future Considerations

- Pre-allocate multiple chunks at initialization for predictable problems
- Support thread-local eta buffers for parallel basis updates
- Add memory usage statistics for tuning chunk sizes

## 12. References

- "The Art of Computer Programming Vol 1" - Arena allocation patterns
- "Efficient Implementation of the Revised Simplex Method" - Eta vectors and Product Form of Inverse
- Linux kernel slab allocator - Similar chunked allocation strategy

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
