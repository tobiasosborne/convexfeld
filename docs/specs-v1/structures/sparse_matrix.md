# Structure: SparseMatrix

**Spec Version:** 1.0
**Primary Module:** Matrix Management

## 1. Purpose

### 1.1 What It Represents

SparseMatrix represents the core constraint matrix and associated data for an optimization problem. It stores the constraint matrix **A** in sparse format, along with objective coefficients **c**, constraint right-hand sides **b**, variable bounds **(l, u)**, constraint senses, and variable types. This structure encapsulates all the numerical data required to define a linear or quadratic optimization problem.

Mathematically, SparseMatrix represents the problem:

```
Minimize:    c'x + constant
Subject to:  Ax {<=, =, >=} b
             l <= x <= u
             x_i in type_i (continuous, binary, integer, etc.)
```

The constraint matrix **A** is stored in two sparse formats: CSC (Compressed Sparse Column) for efficient variable-oriented operations, and optionally CSR (Compressed Sparse Row) for constraint-oriented operations. The dual representation enables efficient access patterns for different solver algorithms.

### 1.2 Role in System

SparseMatrix is the numerical core of the optimization model:

- **Sparse Matrix Container:** Stores constraint matrix in CSC format (primary) and CSR format (built lazily)
- **Problem Data Repository:** Centralizes all numerical problem data (coefficients, bounds, senses, types)
- **Solver Interface:** Provides structured access to problem data for LP/QP/MIP solvers
- **Format Converter:** Transparently builds CSR representation from CSC on demand
- **Memory Manager:** Allocates and manages large arrays for problem data

Every optimization algorithm (simplex, barrier, branch-and-bound) reads from SparseMatrix to access problem structure.

### 1.3 Design Rationale

SparseMatrix design addresses several key concerns:

1. **Sparse Storage:** Most real-world optimization problems have sparse constraint matrices (< 1% density). CSC format stores only nonzeros, saving memory and enabling cache-efficient operations.

2. **Dual Format Support:** Simplex needs column access (FTRAN, pricing), while constraint modification needs row access. Maintaining both formats would double memory; lazy CSR construction balances memory and speed.

3. **Cache Efficiency:** Contiguous arrays (colStart, rowIndices, coeffValues) enable vectorization and prefetching.

4. **Modification Efficiency:** Coefficients, bounds, and senses stored in separate arrays allow independent updates without matrix reconstruction.

5. **Solver Neutrality:** Normalized storage (all constraints as <=) simplifies solver logic, with runtime negation for >= constraints.

Alternative designs (dense matrices, list-of-lists) would waste memory or sacrifice cache efficiency.

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| version | int | Internal version marker | Always 1 |
| numVars | int | Number of variables (columns) | >= 0 |
| numConstrs | int | Number of constraints (rows) | >= 0 |
| numNonzeros | int64 | Total nonzero entries in A | >= 0 |
| colStart | int64[] | CSC column start indices | Size: numVars+1, sorted |
| rowIndices | int[] | CSC row indices for each nonzero | Size: numNonzeros |
| coeffValues | double[] | CSC coefficient values | Size: numNonzeros |
| objCoeffs | double[] | Objective coefficients c | Size: numVars |
| lb | double[] | Variable lower bounds | Size: numVars |
| ub | double[] | Variable upper bounds | Size: numVars |
| rhs | double[] | Constraint right-hand sides b | Size: numConstrs |
| sense | char[] | Constraint senses ('<', '=', '>') | Size: numConstrs |
| objOffset | double | Objective constant term | Any double |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| rowStart | CSC matrix | CSR row start indices | Built lazily on first row access |
| rowEnd | CSC matrix | CSR row end indices | Built with rowStart |
| rowColIndices | CSC matrix | CSR column indices | Built with rowStart |
| rowCoeffValues | CSC matrix | CSR coefficient values | Built with rowStart |
| colLen | colStart | Column lengths (nonzeros per column) | Built with CSC matrix |

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| vtype | char[] | Model has discrete variables | NULL (all continuous) |
| varnames | char*[] | Variable names tracked | NULL (anonymous) |
| constrnames | char*[] | Constraint names tracked | NULL (anonymous) |
| modelName | char* | Model has name | NULL (unnamed) |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| solStatus | int | Solution status code | -1 = unsolved, 2 = optimal, etc. |
| matrixType | int | Row-major data availability flag | 0 = CSC only, 1 = CSR ready |
| optimizeFlag | int | Ready for optimization flag | >0 = proceed |
| mipSolveFlag | int | Solving as MIP flag | 1 if MIP solve |
| numQConstrs | int | Count of quadratic constraints | >= 0 |
| numGenConstrs | int | Count of general constraints | >= 0 |
| numSOS | int | Count of SOS constraints | >= 0 |
| forceNonConvex | int | Force non-convex handling flag | 0 or 1 |
| rowDataTrigger | void* | Trigger for row-major build | NULL or pointer |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| CSC arrays (colStart, rowIndices, coeffValues) | 1:1 | Exclusive - allocated/freed with SparseMatrix |
| CSR arrays (rowStart, rowColIndices, etc.) | 0:1 | Exclusive - built lazily, freed with SparseMatrix |
| Bound arrays (lb, ub) | 1:1 | Exclusive - allocated/freed with SparseMatrix |
| Coefficient arrays (objCoeffs, rhs) | 1:1 | Exclusive - allocated/freed with SparseMatrix |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| None | N/A | SparseMatrix is leaf structure (no external references) |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| CxfModel | matrix (primary), primaryMatrix, workMatrix | Model owns 1-3 SparseMatrix instances |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] numVars >= 0, numConstrs >= 0, numNonzeros >= 0
- [x] colStart has size numVars + 1
- [x] colStart[0] = 0
- [x] colStart[numVars] = numNonzeros
- [x] colStart is monotonically non-decreasing (colStart[i] <= colStart[i+1])
- [x] rowIndices has size numNonzeros
- [x] coeffValues has size numNonzeros
- [x] All rowIndices[k] in range [0, numConstrs)
- [x] lb, ub, objCoeffs have size numVars
- [x] rhs, sense have size numConstrs
- [x] If rowStart != NULL, then rowEnd, rowColIndices, rowCoeffValues != NULL
- [x] All sense values in {'<', '=', '>'}
- [x] lb[i] <= ub[i] for all i (or ub[i] = infinity)

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] Sum of all column lengths = numNonzeros
- [x] CSC and CSR represent same matrix (if CSR exists)
- [x] For >= constraints: internally stored as <= with negated coefficients/RHS
- [x] If vtype != NULL, then vtype has size numVars
- [x] If varnames != NULL, then varnames has size numVars
- [x] version = 1 (internal version check)
- [x] If matrixType = 1, then CSR arrays are valid

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After creation | Empty: numVars=0, numConstrs=0, numNonzeros=0, colStart[0]=0 |
| After loading | CSC arrays allocated and populated, CSR arrays NULL |
| After first row access | CSR arrays allocated and populated, matrixType=1 |
| After modification | CSR arrays invalidated (freed), matrixType=0 |
| Before destruction | All arrays either NULL or valid |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_newmodel | Empty model | Zero dimensions, minimal allocations |
| cxf_readmodel | Load from file | Full matrix from file, CSC only |
| cxf_updatemodel | Apply pending changes | Rebuilt CSC from pending buffer |
| Internal allocation | Presolve, concurrent | Copy of existing matrix |

### 5.2 States

```
EMPTY -> POPULATED -> CSC_ONLY -> DUAL_FORMAT -> MODIFIED -> CSC_ONLY
  |                                                |
  +----------------------<------------------------+
                  (modification cycle)

State Descriptions:
- EMPTY: Zero dimensions, no allocations
- POPULATED: CSC arrays allocated and filled
- CSC_ONLY: Only column-major format available (matrixType=0)
- DUAL_FORMAT: Both CSC and CSR formats available (matrixType=1)
- MODIFIED: CSC modified, CSR invalidated
```

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| EMPTY | POPULATED | cxf_addvars, cxf_addconstrs | Allocate CSC arrays |
| POPULATED | CSC_ONLY | cxf_updatemodel | Validate and finalize CSC |
| CSC_ONLY | DUAL_FORMAT | cxf_getconstrs (or row access) | Build CSR arrays from CSC |
| DUAL_FORMAT | MODIFIED | cxf_chgcoeff | Modify CSC, invalidate CSR |
| MODIFIED | CSC_ONLY | Any operation needing clean state | Free CSR arrays |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| cxf_freemodel | Free all arrays: colStart, rowIndices, coeffValues, lb, ub, objCoeffs, rhs, sense, vtype, names, CSR arrays |

### 5.5 Ownership Rules

- **Who creates:** Model creation functions (cxf_newmodel, cxf_readmodel) or internal matrix builders
- **Who owns:** CxfModel (as model->matrix)
- **Who destroys:** cxf_freemodel
- **Sharing allowed:** No (each model has independent SparseMatrix, except for concurrent solves with shared read-only copies)

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| Get column data | Column nonzeros | O(nnz_col) where nnz_col = colStart[j+1] - colStart[j] |
| Get row data | Row nonzeros | O(nnz_row) if CSR exists, else O(nnz) to build CSR |
| Get coefficient A[i,j] | Single value | O(nnz_col) binary search in column j |
| Get variable bounds | lb[j], ub[j] | O(1) array access |
| Get constraint RHS | rhs[i] | O(1) array access |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| Change coefficient | Update coeffValues[k] | CSR (if exists) - requires rebuild |
| Change bound | Update lb[j] or ub[j] | Nothing (bounds independent of matrix) |
| Change RHS | Update rhs[i] | Nothing (RHS independent of matrix) |
| Change sense | Update sense[i], may negate row | CSR (if exists) |
| Add column | Extend colStart, append to rowIndices/coeffValues | CSR (if exists) |
| Add row | Rebuild CSC with new row | CSR (if exists) |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| Build CSR from CSC | Enable row access | Atomic - all or nothing |
| Matrix transpose | Convert CSC <-> CSR | Atomic |
| Matrix rebuild | Apply all pending changes | Atomic - rollback on error |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed overhead | ~1 KB (structure fields) |
| CSC arrays | 8*(numVars+1) + 4*numNonzeros + 8*numNonzeros = 8n + 12*nnz bytes |
| Bounds/objective | 8*numVars + 8*numVars + 8*numVars = 24*numVars bytes |
| RHS/sense | 8*numConstrs + numConstrs = 9*numConstrs bytes |
| CSR arrays (if built) | 8*numConstrs + 8*numConstrs + 4*numNonzeros + 8*numNonzeros = 16m + 12*nnz bytes |
| **Total (CSC only)** | ~1 KB + 32n + 9m + 12*nnz bytes |
| **Total (CSC+CSR)** | ~1 KB + 32n + 25m + 24*nnz bytes |

Example: 1000 vars, 500 constrs, 5000 nonzeros (0.5% density)
- CSC only: 32 KB + 4.5 KB + 60 KB = ~97 KB
- CSC+CSR: 32 KB + 12.5 KB + 120 KB = ~165 KB

### 7.2 Allocation Strategy

Separate allocations for each major array to allow independent resizing. Arrays allocated in contiguous blocks (not linked structures) for cache efficiency. CSR arrays allocated lazily only when needed (saves memory for column-oriented workloads).

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| colStart, rowStart, rowEnd | 8 bytes (int64) |
| rowIndices, colLen | 4 bytes (int32) |
| coeffValues, bounds, objCoeffs | 16 bytes (SIMD vectorization) |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Read-only safe with external synchronization

Concurrent reads are safe. Writes require exclusive access.

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Read matrix | Shared lock (multiple readers OK) |
| Build CSR | Exclusive lock (modifies matrixType, CSR pointers) |
| Modify coefficients | Exclusive lock (modifies arrays) |
| Resize (add vars/constrs) | Exclusive lock (reallocates arrays) |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- Multiple threads reading different columns (no lock needed if no concurrent writes)
- Multiple threads reading same column (no lock needed if no concurrent writes)

**Unsafe patterns:**
- Thread A reading column while thread B modifying -> data race
- Thread A building CSR while thread B reading matrix -> inconsistent state
- Multiple threads building CSR concurrently -> double allocation

## 9. Serialization

### 9.1 Persistent Format

SparseMatrix serializes to various optimization file formats:

- **MPS format:** Standard sparse matrix format (text)
  ```
  ROWS
   N  obj
   L  c0
   E  c1
  COLUMNS
   x0  obj  3.0
   x0  c0   1.0
  RHS
   rhs  c0  10.0
  BOUNDS
   LO bounds x0 0.0
   UP bounds x0 5.0
  ```

- **LP format:** Human-readable (text)
  ```
  Minimize
   obj: 3.0 x0 + 2.0 x1
  Subject To
   c0: x0 + 2 x1 <= 10
  Bounds
   0 <= x0 <= 5
  ```

- **REW format:** Native binary (fast, compact)

### 9.2 Version Compatibility

| Version | Compatible | Migration |
|---------|------------|-----------|
| 12.x | Yes | Native binary format |
| 11.x | Yes | MPS/LP universally compatible |
| Other solvers | Partial | MPS format widely supported, LP format varies |

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Sequential column access | Excellent | Contiguous arrays, cache-friendly |
| Random column access | Good | O(1) via colStart[j] |
| Sequential row access | Good (if CSR exists) | Requires CSR build first time |
| Random coefficient access | Moderate | O(nnz_col) binary search |
| Coefficient modification | Moderate | Invalidates CSR cache |

### 10.2 Cache Behavior

CSC format is highly cache-efficient for column operations:
- colStart, rowIndices, coeffValues accessed sequentially during FTRAN/pricing
- Typical column fits in L1 cache (< 100 nonzeros)
- CSR build is memory-bandwidth-bound (reads entire CSC, writes entire CSR)

### 10.3 Memory Bandwidth

Simplex operations (BTRAN, FTRAN) are memory-bandwidth-limited. Sparse storage reduces bandwidth by 100-1000x vs dense matrices. Vectorization of coefficient operations can achieve 2-4x speedup with aligned arrays.

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| colStart not sorted | Check during validation | Return CXF_ERR_DATA_NOT_AVAILABLE |
| Row index out of bounds | Check during iteration | Return CXF_ERR_INDEX_OUT_OF_RANGE |
| Inconsistent dimensions | numNonzeros != colStart[numVars] | Return CXF_ERR_DATA_NOT_AVAILABLE |
| NaN/Inf coefficients | Check on coefficient set | Return CXF_ERR_INVALID_ARGUMENT |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| Index bounds | On access | O(1) per access |
| Sorted colStart | After matrix build | O(n) |
| Consistent dimensions | After matrix build | O(1) |
| NaN/Inf check | On coefficient set | O(1) per coefficient |

## 12. Examples

### 12.1 Typical Instance

**Small LP: 3 vars, 2 constraints, 5 nonzeros**

```
Minimize:  3x_0 + 2x_1 + 4x_2
Subject to:
  x_0 + 2x_1       <= 10  (c0)
  3x_0      + x_2  >=  6  (c1, stored as -3x_0 - x_2 <= -6)
  x_0, x_1, x_2 >= 0

SparseMatrix:
  numVars = 3
  numConstrs = 2
  numNonzeros = 5

  CSC format:
    colStart    = [0, 2, 3, 5]
    colLen      = [2, 1, 2]
    rowIndices  = [0, 1, 0, 1]     # Note: row 1 has original indices
    coeffValues = [1.0, -3.0, 2.0, -1.0]  # Note: c1 coefficients negated

  Objective and bounds:
    objCoeffs = [3.0, 2.0, 4.0]
    lb = [0.0, 0.0, 0.0]
    ub = [inf, inf, inf]

  Constraints:
    sense = ['<', '>']  # Original senses (stored, not used for math)
    rhs = [10.0, -6.0]  # Note: c1 RHS negated

  CSR format (built on demand):
    rowStart       = [0, 2, 5]
    rowEnd         = [2, 5]
    rowColIndices  = [0, 1, 0, 2]
    rowCoeffValues = [1.0, 2.0, -3.0, -1.0]  # Coefficients as stored
```

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| Empty | No variables, no constraints | numVars=0, numConstrs=0, colStart=[0] |
| Single var | One variable, no constraints | numVars=1, numConstrs=0, colStart=[0,0] |
| Dense column | Variable in every constraint | Column length = numConstrs |
| Sparse matrix | < 1% density | Most columns have < 5 nonzeros |
| Singleton rows | Constraint with 1 variable | Row length = 1 (efficient presolve target) |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| CxfModel | Parent - owns SparseMatrix |
| PendingBuffer | Sibling - batches changes before SparseMatrix update |
| SolverContext | Consumer - reads SparseMatrix for optimization algorithms |

## 14. References

- "Sparse Matrix Storage Formats" - standard computer science references
- "The Simplex Method" - Chvatal, for CSC usage in simplex
- MPS format specification: IBM MPSX standard

## 15. Validation Checklist

Before finalizing:

- [x] All fields documented
- [x] All invariants identified
- [x] Lifecycle complete
- [x] Thread safety analyzed
- [x] No implementation details leaked (no byte offsets)
- [x] Could implement structure from this spec

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
