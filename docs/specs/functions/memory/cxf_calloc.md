# cxf_calloc

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Allocate zero-initialized memory from the environment's memory pool. This function combines memory allocation with zero-initialization, analogous to standard C library calloc(). It is used when allocating structures or arrays that must start with all fields set to zero for correctness or safety, such as structures with pointer fields that should be NULL or counters that should start at zero.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment context pointer | Valid CxfEnv pointer | Yes |
| size | size_t | Total number of bytes to allocate | > 0 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void* | Pointer to zero-initialized memory block, or NULL on failure |

### 2.3 Side Effects

- Updates environment's total memory tracking counter
- Increments environment's allocation count
- May add allocation to environment's tracking list
- Acquires and releases environment's critical section lock
- Writes zeros to entire allocated block

## 3. Contract

### 3.1 Preconditions

- [ ] env must be a valid CxfEnv pointer (not NULL)
- [ ] size must be greater than 0
- [ ] Environment lock must be available

### 3.2 Postconditions

- [ ] If successful: Returns pointer to exactly 'size' bytes of memory
- [ ] If successful: All bytes in returned memory are set to zero (0x00)
- [ ] If successful: Memory is aligned for any data type
- [ ] If successful: Environment's memory counter is increased by 'size'
- [ ] If failed: Returns NULL and environment state is unchanged

### 3.3 Invariants

- [ ] Environment's total memory count accurately reflects all allocations
- [ ] All bytes in allocated memory are zero before function returns

## 4. Algorithm

### 4.1 Overview

The function allocates memory using the standard allocation mechanism (via cxf_malloc or equivalent), then fills the entire allocated block with zeros using a memory-setting operation. This ensures that pointers are NULL, integers are zero, floating-point values are 0.0, and flags are cleared. The operation is atomic from the caller's perspective - either fully zero-initialized memory is returned, or NULL.

### 4.2 Detailed Steps

1. Validate input parameters (env != NULL, size > 0)
2. Allocate memory using underlying allocation function (cxf_malloc)
   - This handles lock acquisition, tracking, and memory limit checks
3. If allocation successful:
   - Fill allocated memory with zeros using memset or equivalent
   - Return pointer to zero-initialized memory
4. If allocation failed:
   - Return NULL (tracking already handled by allocation function)

### 4.3 Pseudocode (if needed)

```
FUNCTION cxf_calloc(env, size) → pointer or NULL
  IF env = NULL OR size = 0 THEN
    RETURN NULL
  END IF

  # Allocate memory
  ptr ← cxf_malloc(env, size)

  IF ptr ≠ NULL THEN
    # Zero-initialize the allocated memory
    FOR i FROM 0 TO size-1 DO
      ptr[i] ← 0
    END FOR
  END IF

  RETURN ptr
END FUNCTION
```

### 4.4 Mathematical Foundation (if applicable)

Not applicable - this is a system-level resource management function.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(size)
- **Average case:** O(size)
- **Worst case:** O(size)

Where:
- size = number of bytes to allocate and zero
- Zeroing operation is linear in allocation size
- Modern CPUs use SIMD instructions (SSE2, AVX2) for fast zeroing

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - fixed tracking overhead
- **Total space:** O(size) - the requested allocation

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| env is NULL | N/A (returns NULL) | Invalid environment pointer |
| size is 0 | N/A (returns NULL) | Invalid allocation size |
| Insufficient memory | N/A (returns NULL) | Underlying allocation failed |
| Memory limit exceeded | N/A (returns NULL) | Would exceed configured MemLimit |

### 6.2 Error Behavior

Returns NULL on any error condition, identical to cxf_malloc. Callers must check for NULL and propagate error code CXF_ERR_OUT_OF_MEMORY (1001). No partial initialization occurs - either fully zero-initialized memory is returned, or NULL.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Null environment | env=NULL, size=1024 | Return NULL immediately |
| Zero size | env=valid, size=0 | Return NULL immediately |
| Very large allocation | env=valid, size=1GB | Allocate if possible, zero all bytes |
| Single byte | env=valid, size=1 | Return pointer to single zero byte |

## 8. Thread Safety

**Thread-safe:** Yes

**Synchronization required:** Thread safety is inherited from the underlying cxf_malloc function, which acquires environment->criticalSection. Multiple threads can call cxf_calloc concurrently without corruption.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_malloc | Memory | Allocate memory with tracking |
| memset | System | Fill memory with zeros |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_newmodel | API | Allocate MatrixData structure |
| cxf_simplex_init | Solver | Allocate SolverState structure |
| Various internal functions | Various | Allocate structures requiring zero-init |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_malloc | Non-zeroing variant - faster but returns uninitialized memory |
| cxf_realloc | Resize operation - new bytes may be uninitialized |
| cxf_free | Inverse operation - deallocates memory |
| calloc | Standard C equivalent (but without environment tracking) |

## 11. Design Notes

### 11.1 Design Rationale

Zero-initialization prevents several classes of bugs:
1. **NULL pointer safety** - Pointer fields start NULL, not random garbage
2. **Counter initialization** - Counts and flags start at zero
3. **Deterministic behavior** - Eliminates undefined behavior from uninitialized reads
4. **Security** - Prevents information leakage from previous memory contents

The performance cost (memset overhead) is acceptable for structures and is negligible compared to solver runtime.

### 11.2 Performance Considerations

Zeroing performance on modern hardware:
- Small (<64 bytes): ~1-2 CPU cycles per byte (rep stosb instruction)
- Medium (64B-4KB): ~0.5 cycles/byte (SIMD: SSE2 movdqa)
- Large (>4KB): ~0.25 cycles/byte (SIMD: AVX2 vmovdqa)

Examples on 3.0 GHz CPU:
- 128 bytes (typical structure): ~40 nanoseconds
- 1 KB (SolverState): ~170 nanoseconds
- 64 KB (large array): ~8 microseconds
- 1 MB (matrix): ~130 microseconds

Overhead is <1% of typical solve time and is worth the safety benefit.

### 11.3 Future Considerations

Potential optimizations:
- Use OS zero-page mechanisms (VirtualAlloc, mmap) for very large allocations (>64KB)
- These return zero pages from kernel without explicit memset
- Could check size threshold and use different allocator for large requests

## 12. References

- Standard C calloc specification (ISO C standard)
- Intel Optimization Manual: Memory Initialization Techniques
- POSIX mmap with MAP_ANONYMOUS (zero-page optimization)

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
