# Structure: SolverContext

**Spec Version:** 1.0
**Primary Module:** Simplex Core

## 1. Purpose

### 1.1 What It Represents

The SolverContext structure represents the complete runtime environment for a single LP solve using the simplex method. It is a comprehensive working copy of the problem data along with all solver state needed to perform simplex iterations. This structure contains the problem definition (objective, bounds, constraints), algorithmic state (basis, phase, iteration count), working arrays for intermediate computations, and configuration parameters that control solver behavior.

The SolverContext isolates all mutable solver data from the immutable model definition, allowing the solver to modify bounds, scale coefficients, perturb values, and track progress without affecting the original problem data. This separation enables multiple solves to be performed sequentially or the same model to be solved with different parameters without interference.

### 1.2 Role in System

The SolverContext is the central data structure for the entire LP solve process. It is:
- Created at the start of each solve by cxf_simplex_init
- Passed to every simplex function (preprocessing, iteration, cleanup)
- Modified throughout the solve as the algorithm progresses
- Queried to extract the final solution
- Destroyed after solve completion

All simplex operations--pricing, ratio test, pivoting, refactorization, solution extraction--operate on this structure. It serves as both input (problem data) and state (current iteration status).

### 1.3 Design Rationale

The SolverContext design creates a self-contained solver environment that:
1. **Isolates solver state from model:** Allows multiple solves without cross-contamination
2. **Enables preprocessing:** Working copies of bounds/coefficients can be modified
3. **Supports warm starts:** Can be initialized from a saved basis
4. **Facilitates profiling:** Contains timing and statistics fields
5. **Allows perturbation:** Original bounds preserved while working bounds are perturbed

The structure is relatively large (~1200 bytes plus arrays) but is allocated only once per solve, not per iteration, making the overhead acceptable.

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| modelRef | CxfModel* | Back-pointer to model for accessing matrix and environment | Non-NULL |
| phase | int | Current simplex phase: 0=setup, 1=phase I (feasibility), 2=phase II (optimality) | 0, 1, or 2 |
| numVars | int | Number of variables in problem | >= 0 |
| numConstrs | int | Number of constraints in problem | >= 0 |
| numNonzeros | int64 | Number of non-zeros in constraint matrix | >= 0 |
| hasQuadratic | int | 1 if problem has quadratic terms, 0 otherwise | 0 or 1 |
| solveMode | int | Algorithm selection: 0=primal simplex, 1=dual simplex, 2=barrier, 3=auto | 0-3 |
| maxIterations | int | Iteration limit before termination | > 0 |
| tolerance | double | Optimality/feasibility tolerance | > 0, typically 1e-6 |
| objValue | double | Current objective function value | Any double |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| scaleFactor | Problem analysis | Work normalization factor | Once at initialization |
| removedRows | Presolve | Count of rows eliminated by presolve | Presolve phase |
| removedCols | Presolve | Count of columns eliminated by presolve | Presolve phase |
| presolveTime | Timing | Elapsed time in presolve | End of presolve |

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| timingState | TimingState* | Always (for profiling) | NULL if allocation fails |
| startBasis | BasisData* | Warm start provided | NULL for cold start |
| qpData | QPData* | hasQuadratic=1 | NULL for LP problems |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| iterationMode | int | Special iteration mode flags (internal control) |
| specialFlag1 | int | Presolve skip flag (internal control) |
| specialFlag2 | int | Processing control flag (internal control) |
| boundsFlag | int | Indicates bounds processing needed |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| WorkingArrays | 1:1 | Exclusive - allocated at init, freed at final |
| BasisState | 1:1 | Exclusive - embedded fields, lifetime tied to SolverContext |
| PricingContext | 0:1 | Exclusive - created on first use, freed at cleanup |
| TimingState | 0:1 | Exclusive - profiling data structure |
| QPData | 0:1 | Exclusive - only for quadratic problems |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| CxfModel | 1:1 | Must outlive SolverContext - needed for environment access |
| SparseMatrix | 1:1 | Must outlive SolverContext - constraint matrix is read-only |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| None | N/A | SolverContext is a top-level structure for the solve process |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] numVars >= 0 and numConstrs >= 0
- [x] phase is in range [0, 2]
- [x] solveMode is in range [0, 3]
- [x] All array pointers are either NULL or point to allocated memory of correct size
- [x] modelRef is non-NULL and valid
- [x] If hasQuadratic=1, qpData is non-NULL

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] numNonzeros matches the actual count in constraint matrix arrays
- [x] If phase=2, the basis must be primal feasible
- [x] objValue equals c^T x_B (objective of basic solution) within tolerance
- [x] removedRows + removedCols <= original problem dimensions
- [x] Working bounds satisfy lb_work[j] <= ub_work[j] for all j

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After initialization | All arrays allocated, phase=0, objValue=0 |
| After preprocessing | Phase=0 or 1, bounds may be modified, removedRows/Cols set |
| During phase I | Phase=1, seeking primal feasibility |
| During phase II | Phase=2, basis is primal feasible, seeking optimality |
| At optimality | Phase=2, all reduced costs satisfy optimality conditions |
| After cleanup | All owned memory freed, structure invalid |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_simplex_init | Start of LP solve | Arrays allocated, phase=0, iteration count=0 |

### 5.2 States

```
UNINITIALIZED
     |
     v
ALLOCATED --> PREPROCESSED --> PHASE_I --> PHASE_II --> OPTIMAL
     |            |                |           |           |
     |            |                v           v           v
     |            +-----------> INFEASIBLE  OPTIMAL    UNBOUNDED
     |                             |           |           |
     |                             v           v           v
     +------------------------> CLEANUP --> DESTROYED
```

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| UNINITIALIZED | ALLOCATED | cxf_simplex_init | Allocate all arrays, copy problem data |
| ALLOCATED | PREPROCESSED | cxf_simplex_preprocess | Apply presolve reductions, modify bounds |
| PREPROCESSED | PHASE_I | Infeasible crash basis | Start phase I (artificial variables) |
| PREPROCESSED | PHASE_II | Feasible crash basis | Skip directly to phase II |
| PHASE_I | PHASE_II | Feasibility achieved | Switch objective, continue iterations |
| PHASE_I | INFEASIBLE | Artificial variable stuck | Terminate with infeasibility status |
| PHASE_II | OPTIMAL | Optimality conditions met | Terminate with optimal status |
| PHASE_II | UNBOUNDED | Unbounded ray detected | Terminate with unbounded status |
| OPTIMAL | CLEANUP | cxf_simplex_final | Prepare for destruction |
| INFEASIBLE | CLEANUP | cxf_simplex_final | Prepare for destruction |
| UNBOUNDED | CLEANUP | cxf_simplex_final | Prepare for destruction |
| CLEANUP | DESTROYED | Free all memory | All arrays freed |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| cxf_simplex_final | Free all working arrays, basis arrays, eta pool, timing state, QP data |

### 5.5 Ownership Rules

- **Who creates:** cxf_simplex_init
- **Who owns:** Calling code (typically cxf_solve_lp)
- **Who destroys:** cxf_simplex_final
- **Sharing allowed:** No - exclusive ownership during solve

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| Get objective value | Current objValue | O(1) |
| Get iteration count | Number of simplex iterations | O(1) |
| Get current phase | Phase (0/1/2) | O(1) |
| Check optimality | Boolean: optimality conditions satisfied | O(n) to check reduced costs |
| Get basic solution | Values of basic variables | O(m) via FTRAN |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| Perform pivot | Update basis, add eta vector | Basic solution, reduced costs |
| Perturb bounds | Modify working bounds | Feasibility status |
| Update objective | Modify objective coefficients | Reduced costs, objValue |
| Increment phase | Change from phase I to II | Iteration strategy |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| Preprocess | Apply presolve reductions | All-or-nothing (can rollback) |
| Refactorize basis | Rebuild eta representation | All-or-nothing |
| Extract solution | Copy final values to model | Read-only |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed overhead | ~1200 bytes (structure itself) |
| Per-variable arrays | ~56 bytes x numVars (bounds, obj, status, types, flags, etc.) |
| Per-constraint arrays | ~24 bytes x numConstrs (rhs, senses, row scaling, basis header) |
| Basis eta vectors | Variable: ~50 KB - 5 MB depending on sparsity and iteration count |
| Total | ~1 KB + 56n + 24m + eta storage |

**Example:** 10,000 vars + 5,000 constraints = ~680 KB + eta storage

### 7.2 Allocation Strategy

The structure itself is allocated as a single fixed-size block. Working arrays are allocated separately as contiguous blocks per data type (all bounds together, all objective coefficients together, etc.). This improves cache locality for operations that scan a single array.

Eta vectors use a pool allocator to avoid malloc/free overhead during iterations.

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| Structure itself | 8 bytes (contains pointers) |
| Double arrays (bounds, obj, etc.) | 8 bytes |
| Int arrays (status, header) | 4 bytes |
| Char arrays (types, senses) | 1 byte |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Not thread-safe

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Read | None if no concurrent writes |
| Write | Exclusive access to SolverContext |
| Resize | Not applicable (fixed size after allocation) |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- None - simplex algorithm is inherently sequential

**Unsafe patterns:**
- Any concurrent modification during solve
- Reading solution values while pivoting
- Multiple threads iterating on the same SolverContext

Note: Multiple SolverContexts for different problems can be used concurrently (each on its own thread) with proper model/environment locking.

## 9. Serialization

### 9.1 Persistent Format

The SolverContext is not directly serialized. However, components can be saved:
- **Basis:** Variable/constraint status via cxf_getbasis/cxf_setbasis
- **Solution:** Variable values via cxf_getX, cxf_getPi
- **Objective:** Via cxf_getobjval

These are saved to/loaded from the CxfModel, not the SolverContext directly.

### 9.2 Version Compatibility

| Version | Compatible | Migration |
|---------|------------|-----------|
| N/A | N/A | SolverContext is ephemeral, not versioned |

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Sequential array scans | Yes | Cache-friendly for bounds, objective, status |
| Random variable lookup | Yes | O(1) array indexing |
| Basis operations | Moderate | Eta list traversal has poor cache locality |
| Working array reuse | Yes | Same buffers reused across iterations |

### 10.2 Cache Behavior

Dense arrays (bounds, objective) have excellent cache locality. Sparse operations (eta vectors, constraint matrix) exploit sparsity but have poorer cache behavior. The structure size (~1 KB) fits in L2/L3 cache, but working arrays do not for large problems.

### 10.3 Memory Bandwidth

Simplex iterations are primarily compute-bound (sparse linear algebra) rather than bandwidth-bound. The bottleneck is typically BTRAN/FTRAN operations, which stream through eta vectors and working arrays. Memory bandwidth becomes critical for dense problems or when eta count is very high.

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| Allocation failure | NULL pointer returned | Return OUT_OF_MEMORY, cleanup partial allocation |
| Numerical instability | Very large/small values in FTRAN/BTRAN | Trigger refactorization or perturbation |
| Iteration limit exceeded | maxIterations reached | Terminate with ITERATION_LIMIT status |
| Infeasible problem | Phase I fails | Terminate with INFEASIBLE status |
| Unbounded problem | Ratio test fails in phase II | Terminate with UNBOUNDED status |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| Array allocation success | At initialization | O(1) per allocation |
| Bounds consistency | After preprocessing | O(n) |
| Basis validity | After crash/warm start | O(n + m) |
| Optimality conditions | Every iteration | O(n) for reduced cost check |

## 12. Examples

### 12.1 Typical Instance

**Problem:** 10,000 variables, 5,000 constraints, 50,000 non-zeros

**Memory breakdown:**
- Structure: 1,200 bytes
- Variable arrays: 56 x 10,000 = 560 KB
- Constraint arrays: 24 x 5,000 = 120 KB
- Eta vectors (avg): ~1-2 MB
- **Total:** ~1.7-2.7 MB

**Iteration profile:**
- Phase: 2 (phase II)
- Iterations: 5,000-15,000
- Refactorizations: 5-15
- Eta count: 0-25,000 (sawtooth pattern)

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| Empty problem | n=0, m=0 | All arrays have zero size, immediate optimal |
| Single variable | n=1, m=0 | Trivial solve by bounds |
| Only equality constraints | All senses '=' | May require artificial variables in phase I |
| Unbounded | No finite optimum | Detected in ratio test, terminate early |
| Infeasible | No feasible solution | Phase I fails, artificial variables remain |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| CxfModel | Parent model, SolverContext operates on its data |
| SparseMatrix | Constraint matrix, referenced read-only |
| BasisState | Embedded/referenced, owned by SolverContext |
| PricingContext | Owned by SolverContext, created on demand |
| TimingState | Owned by SolverContext, profiling data |
| EtaFactors | Owned by BasisState, which is owned by SolverContext |

## 14. References

- Dantzig, G. B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Chvatal, V. (1983). *Linear Programming*. W. H. Freeman.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Bixby, R. E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress." *Operations Research*, 50(1):3-15.
- Koberstein, A. (2005). "The Dual Simplex Method, Techniques for a Fast and Stable Implementation." Ph.D. thesis, Universitat Paderborn.

## 15. Validation Checklist

Before finalizing:

- [x] All fields documented with logical meaning, not byte offsets
- [x] All invariants identified (structural, consistency, temporal)
- [x] Lifecycle complete with state diagram
- [x] Thread safety analyzed (not thread-safe)
- [x] No implementation details leaked (no magic offsets)
- [x] Could implement structure from this spec (using standard data structures)
- [x] Standard simplex terminology used (phase I/II, basic variables, optimality)
- [x] Relationships to other structures documented
- [x] Performance characteristics explained
- [x] References to optimization literature included

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
