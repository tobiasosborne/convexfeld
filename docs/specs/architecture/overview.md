# Convexfeld LP Solver Architecture

Based on established algorithms from optimization literature.

# LP Solver Architecture Overview

**Version:** 1.0
**Status:** Draft

## 1. System Context

### 1.1 What Is the LP Solver?

The LP (Linear Programming) solver is a mathematical optimization engine that finds the optimal solution to constrained linear optimization problems. Given:
- A linear objective function to minimize or maximize
- A set of linear equality and inequality constraints
- Bounds on decision variables

The solver computes the values of decision variables that optimize the objective while satisfying all constraints.

**Problem formulation:**
```
minimize    c^T x
subject to  Ax = b
            l <= x <= u
```

Where:
- x = decision variables (n-dimensional vector)
- c = objective coefficients
- A = constraint matrix (m x n, typically sparse)
- b = right-hand side values
- l, u = variable bounds

### 1.2 Problem Domain

The LP solver addresses:
- **Linear Programs (LP):** All constraints and objectives are linear
- **Quadratic Programs (QP):** Quadratic objective, linear constraints
- **Mixed-Integer Programs (MIP):** Subset of variables restricted to integers (uses LP for relaxations)

### 1.3 Core Algorithm: Revised Simplex Method

The solver implements the **revised simplex method**, an efficient variant of the classical simplex algorithm that avoids storing and updating the full tableau. Instead, it:

1. Maintains a **basis** (set of m basic variables that define a vertex of the feasible region)
2. Stores the **basis inverse** implicitly using Product Form of Inverse (PFI)
3. Performs **pricing** to select improving directions
4. Executes **pivots** to move between adjacent vertices
5. Terminates when optimality conditions are satisfied

**Key advantages:**
- Exploits sparsity in constraint matrix
- Requires O(m^2) space instead of O(mn) for full tableau
- Updates basis inverse incrementally between pivots
- Practical performance is typically polynomial despite exponential worst case

## 2. Component Architecture

### 2.1 Module Hierarchy

The LP solver consists of 17 functional modules organized in 6 dependency layers:

```
+---------------------------------------------------------+
|                    Layer 6: Public API                   |
|                      +------------+                      |
|                      |  Model API  | (30 functions)      |
|                      +------------+                      |
+---------------------------------------------------------+
                               |
+---------------------------------------------------------+
|                  Layer 5: High-Level Algorithms          |
|     +-------------+  +-----------+  +-------------+     |
|     | Simplex Core|  | Crossover |  |  Utilities  |     |
|     | (21 funcs)  |  | (2 funcs) |  | (10 funcs)  |     |
|     +-------------+  +-----------+  +-------------+     |
+---------------------------------------------------------+
                               |
+---------------------------------------------------------+
|                Layer 4: Algorithm Components             |
|     +-------------+  +--------------+                    |
|     |   Pricing   |  | Solver State |                    |
|     |  (6 funcs)  |  |  (4 funcs)   |                    |
|     +-------------+  +--------------+                    |
+---------------------------------------------------------+
                               |
+---------------------------------------------------------+
|               Layer 3: Core Data Operations              |
|  +----------+  +-----------+  +------------+  +--------+ |
|  |  Basis   |  | Callbacks |  |Model Anlys.|  | Timing | |
|  | (8 func) |  | (6 funcs) |  | (6 funcs)  |  |(5 func)| |
|  +----------+  +-----------+  +------------+  +--------+ |
+---------------------------------------------------------+
                               |
+---------------------------------------------------------+
|                  Layer 2: Data Management                |
|  +-------------+  +-----------+  +-------------+        |
|  |   Matrix    |  | Threading |  |   Logging   |        |
|  |  (7 funcs)  |  | (7 funcs) |  |  (5 funcs)  |        |
|  +-------------+  +-----------+  +-------------+        |
+---------------------------------------------------------+
                               |
+---------------------------------------------------------+
|                  Layer 1: Error Handling                 |
|  +--------------+  +-------------+                       |
|  |Error Handling|  | Validation  |                       |
|  |  (10 funcs)  |  |  (2 funcs)  |                       |
|  +--------------+  +-------------+                       |
+---------------------------------------------------------+
                               |
+---------------------------------------------------------+
|                   Layer 0: Foundation                    |
|  +------------+  +------------+                          |
|  |   Memory   |  | Parameters |                          |
|  | (9 funcs)  |  | (4 funcs)  |                          |
|  +------------+  +------------+                          |
+---------------------------------------------------------+
```

### 2.2 Module Descriptions

#### Layer 0: Foundation

**Memory Management (9 functions)**
- Purpose: Core allocation, deallocation, lifecycle management
- Key functions: cxf_malloc, cxf_calloc, cxf_free, cxf_realloc
- Responsibilities: Pool management, tracking, cleanup
- Design: Thread-safe allocators with environment-level accounting

**Parameters (4 functions)**
- Purpose: Read solver tolerances and thresholds
- Key functions: cxf_get_feasibility_tol, cxf_get_optimality_tol, cxf_get_infinity
- Responsibilities: Tolerance retrieval, parameter validation
- Design: Direct offset-based reads from environment structure

#### Layer 1: Error Handling

**Error Handling (10 functions)**
- Purpose: Error detection, reporting, logging, recovery
- Key functions: cxf_error, cxf_errorlog, cxf_checkenv
- Responsibilities: Error code management, message formatting, validation checks
- Design: Centralized error propagation with logging integration

**Validation (2 functions)**
- Purpose: Input validation and type checking
- Key functions: cxf_validate_array, cxf_validate_vartypes
- Responsibilities: NaN/Inf detection, type normalization
- Design: Pre-condition checkers for API entry points

#### Layer 2: Data Management

**Logging (5 functions)**
- Purpose: Output formatting, message routing, callback management
- Key functions: cxf_log_printf, cxf_register_log_callback
- Responsibilities: Formatted output, log file management, user callbacks
- Design: Buffered output with critical section protection

**Threading (7 functions)**
- Purpose: Thread management, synchronization, CPU detection
- Key functions: cxf_acquire_solve_lock, cxf_release_solve_lock
- Responsibilities: Lock acquisition, thread count configuration
- Design: Windows CRITICAL_SECTION wrapper with recursive support

**Matrix Operations (7 functions)**
- Purpose: Sparse matrix operations, vector arithmetic
- Key functions: cxf_matrix_multiply, cxf_dot_product, cxf_build_row_major
- Responsibilities: CSC/CSR conversions, sparse linear algebra
- Design: Sparse-aware algorithms with index sorting

#### Layer 3: Core Data Operations

**Timing (5 functions)**
- Purpose: Performance measurement and profiling
- Key functions: cxf_get_timestamp, cxf_timing_start, cxf_timing_end
- Responsibilities: High-resolution timing, phase measurement
- Design: QueryPerformanceCounter-based wall clock timing

**Model Analysis (6 functions)**
- Purpose: Analyze model characteristics and statistics
- Key functions: cxf_is_mip_model, cxf_is_quadratic, cxf_coefficient_stats
- Responsibilities: Model type detection, presolve statistics
- Design: Read-only model inspection without modification

**Callbacks (6 functions)**
- Purpose: User callback infrastructure and termination
- Key functions: cxf_init_callback_struct, cxf_pre_optimize_callback
- Responsibilities: Callback registration, invocation, termination signaling
- Design: Magic-validated state structures with atomic termination flags

**Basis Operations (8 functions)**
- Purpose: Basis matrix factorization, updates, linear system solves
- Key functions: cxf_ftran, cxf_btran, cxf_basis_refactor
- Responsibilities: Forward/backward transformation, LU factorization
- Design: Product Form of Inverse with eta vector list

#### Layer 4: Algorithm Components

**Solver State (4 functions)**
- Purpose: High-level solver state initialization and cleanup
- Key functions: cxf_init_solve_state, cxf_cleanup_solve_state
- Responsibilities: SolverContext lifecycle, bound tightening
- Design: 1192-byte structure coordinating all solver components

**Pricing (6 functions)**
- Purpose: Variable selection for entering the basis
- Key functions: cxf_pricing_candidates, cxf_pricing_steepest
- Responsibilities: Steepest edge pricing, partial pricing
- Design: Multi-stage candidate filtering with cache invalidation

#### Layer 5: High-Level Algorithms

**Simplex Core (21 functions)**
- Purpose: Main simplex algorithm implementation
- Key functions: cxf_solve_lp, cxf_simplex_iterate, cxf_pivot_primal
- Responsibilities: Iteration control, pivot execution, phase management
- Design: Two-phase simplex with perturbation for degeneracy

**Crossover (2 functions)**
- Purpose: Convert interior-point solution to basic solution
- Key functions: cxf_crossover, cxf_crossover_bounds
- Responsibilities: Barrier-to-simplex transition
- Design: Bound snapping followed by simplex polishing

**Utilities (10 functions)**
- Purpose: Miscellaneous helper functions
- Key functions: cxf_fix_variable, cxf_generate_seed
- Responsibilities: Variable fixing, random seed generation, constraint queries
- Design: Standalone helpers with no central pattern

#### Layer 6: Public API

**Model API (30 functions)**
- Purpose: Public API functions exposed to users
- Key functions: cxf_optimize, cxf_addvar, cxf_getdblattr
- Responsibilities: Model construction, modification, optimization, query
- Design: Three-level hierarchy (simple -> batch -> extended) with lazy updates

### 2.3 Key Data Structures

```
CxfModel (model instance)
+-- validMagic: 0xC0FEFE1D (integrity check)
+-- env: CxfEnv* (environment pointer)
+-- matrix: SparseMatrix* (constraint matrix)
+-- attrTablePtr: AttrTablePtr* (attribute system)
+-- solutionData: SolutionData* (results)

SparseMatrix (constraint matrix)
+-- numConstrs, numVars (dimensions)
+-- colStart, rowIndices, coeffValues (CSC format)
+-- lb, ub, obj (bounds and objective)

SolverContext (iteration state, 1192 bytes)
+-- phase: current simplex phase (0/1/2)
+-- basisHeader: maps rows to basic variables
+-- variableStatus: maps variables to status
+-- etaList: linked list of eta update vectors
+-- timingState: performance measurements

CxfEnv (solver environment)
+-- active: initialization status flag
+-- CRITICAL_SECTION*: thread safety lock
+-- errorBuffer: last error message
+-- parameters: solver tolerances and settings
```

## 3. Design Decisions

### 3.1 Why Revised Simplex?

**Trade-offs:**

| Aspect | Full Tableau | Revised Simplex | Rationale |
|--------|--------------|-----------------|-----------|
| Space | O(mn) | O(m^2 + nnz) | Exploits sparsity, critical for large models |
| Time per pivot | O(mn) | O(m^2 + d) | Better for sparse problems (d << n) |
| Implementation | Simpler | More complex | Complexity justified by performance gains |
| Numerical stability | Lower | Higher | Basis inverse maintained more accurately |

**Selection criteria:**
- Most real-world LP problems are sparse (< 1% density)
- Typical m/n ratio is 1:3 to 1:10
- Product Form allows incremental updates between pivots
- Refactorization threshold (100-200 pivots) controls numerical drift

### 3.2 Product Form of Inverse (PFI)

**Alternative approaches considered:**

1. **Explicit Basis Inverse (EBI)**
   - Store B^-1 as dense m x m matrix
   - Rejected: O(m^2) space, expensive to update

2. **LU with Forrest-Tomlin Updates**
   - Maintain LU factorization, update incrementally
   - More complex, better for some problem classes
   - PFI chosen for simplicity and general robustness

3. **External LU Libraries (UMFPACK, MUMPS)**
   - Rejected: Licensing, portability, control concerns

**PFI benefits:**
- Simple incremental updates (append eta vector)
- Sparse storage (only changed columns)
- Natural fit for simplex pivoting
- Refactorization point is clear (eta count threshold)

### 3.3 Dual vs Primal Simplex

**Default choice: Dual simplex**

Rationale:
- Bound changes (common in MIP) preserve dual feasibility
- Warm starts from previous solutions more effective
- Fewer iterations in practice for most problem classes
- Primal degeneracy more common than dual degeneracy

**Algorithm selection heuristics:**
```
if (problem has quadratic terms):
    use Barrier
else if (problem is SOCP):
    use Barrier only
else if (user specified method):
    use specified method
else:
    use Dual Simplex (default)
```

### 3.4 Two-Phase vs Big-M Method

**Choice: Two-phase simplex**

Phase I: Minimize sum of artificial variables
Phase II: Minimize original objective

Advantages over Big-M:
- Avoids numerical scaling issues (no large M constant)
- Clear separation of feasibility vs optimality
- Easier to detect true infeasibility
- More numerically stable

### 3.5 Anti-Cycling Strategy

**Perturbation approach:**
- Apply small random perturbations to bounds early in iteration
- Remove perturbations near optimality
- Preferred over Bland's rule (simpler, better performance)

### 3.6 Memory Management

**Environment-level pool:**
- All allocations tracked at environment level
- Bulk deallocation on environment cleanup
- Thread-safe with critical section protection
- Enables deterministic leak detection

**Why not standard malloc/free?**
- Accounting for memory limits
- Better error reporting on OOM
- Potential for future optimizations (arena allocation)

## 4. External Interfaces

### 4.1 API Boundaries

```
User Application
       |
+-------------------------------------+
|  Public C API (30 functions)        |
|  - cxf_optimize                     |
|  - cxf_addvar, cxf_addconstr        |
|  - cxf_getdblattr, cxf_setintparam  |
+-------------------------------------+
       |
+-------------------------------------+
|  Internal LP Solver (112 functions) |
|  - cxf_solve_lp                     |
|  - cxf_simplex_iterate              |
|  - cxf_ftran, cxf_btran             |
+-------------------------------------+
       |
+-------------------------------------+
|  External Dependencies              |
|  - stdlib (malloc, free, memcpy)    |
|  - math (fabs, sqrt, log10)         |
|  - Windows API (CRITICAL_SECTION)   |
+-------------------------------------+
```

### 4.2 API Design Patterns

**Lazy Update Model:**
- Modifications queued in pending buffer
- cxf_updatemodel() processes batch changes
- cxf_optimize() implicitly calls update
- Enables O(1) single modifications

**Error Propagation:**
- All functions return integer status code
- 0 = success, non-zero = error
- cxf_geterrormsg() retrieves last error text
- Error logged via cxf_errorlog before return

**Two-Call Pattern:**
```c
// Call 1: Query size
cxf_getconstrs(model, &numnz, NULL, NULL, NULL, 0, len);

// Call 2: Get data
cxf_getconstrs(model, &numnz, cbeg, cind, cval, 0, len);
```

**Attribute System:**
- Table-driven dispatch
- Type-checked at runtime
- Supports scalar (model-level) and array (element-level) access
- Function pointers or direct memory access for performance

## 5. Core Algorithms

### 5.1 Simplex Method Overview

**One iteration:**
1. **Pricing:** Select entering variable j with most negative reduced cost
2. **FTRAN:** Solve Bx = A_j to get pivot column
3. **Ratio Test:** Select leaving variable i with minimum ratio
4. **Pivot:** Update basis, swap entering/leaving variables
5. **BTRAN:** Update dual variables y^T = c_B^T B^-1
6. **Update:** Append eta vector to basis inverse representation

### 5.2 Basis Inverse Maintenance

**Product Form of Inverse (PFI):**

Basis inverse represented as: B^-1 = E_k * E_{k-1} * ... * E_1

Each eta vector E_i encodes a single pivot operation:
```
E_i = I with column p replaced by eta column
eta[p] = 1/pivot_element
eta[j] = -a[j]/pivot_element for j != p
```

**Operations:**

**FTRAN (Forward Transformation):** Solve Bx = b
```
x = B^-1 * b = E_k * E_{k-1} * ... * E_1 * b
Apply etas from oldest to newest (forward)
```

**BTRAN (Backward Transformation):** Solve y^T B = c^T
```
y^T = c^T * B^-1 = c^T * E_1^T * E_2^T * ... * E_k^T
Apply transposed etas from newest to oldest (backward)
```

**Refactorization:**
- Triggered when eta count exceeds threshold (typically 100-200)
- Recompute LU factorization of current basis
- Reset eta list to empty
- Prevents numerical drift and fill-in

### 5.3 Pricing Strategies

**Steepest Edge:**
- Select variable with largest improvement per unit move
- Accounts for pivot column norm (more accurate than Dantzig)
- Requires edge weight maintenance (updated after each pivot)

**Partial Pricing:**
- Scan subset of variables (chunk) per iteration
- Reduces pricing cost from O(n) to O(n/k)
- Maintains multiple candidate chunks
- Invalidates chunks after basis change

**Devex (Dual Steepest Edge):**
- Approximates steepest edge weights without full computation
- Lower overhead than exact steepest edge
- Reasonable accuracy for most problems

### 5.4 Degeneracy Handling

**Problem:** Multiple variables tied at same objective value
- Causes zero-step pivots (no objective improvement)
- Can lead to cycling (infinite loop revisiting same bases)

**Solution: Perturbation**
- Add small random perturbations to bounds: l' = l + epsilon * random()
- Breaks ties, ensures strict improvement
- Applied early (first ~100 iterations)
- Removed before final solution (unperturb)
- Guarantees finite termination

### 5.5 Crossover Algorithm

**Purpose:** Convert interior-point (barrier) solution to basic solution

**Steps:**
1. **Bound Snapping:**
   - Fix variables very close to bounds (within tolerance)
   - Reduces problem dimension

2. **Simplex Polishing:**
   - Run dual simplex on reduced problem
   - Constructs optimal basis
   - Produces optimal basic solution

**Why needed:**
- Barrier solutions are typically interior (not at vertex)
- MIP requires basic solution for branching
- Dual information requires basic solution

## 6. Cross-Cutting Concerns

### 6.1 Numerical Stability

**Techniques employed:**
- Refactorization to limit condition number growth
- Tolerance-based zero clamping (values < 1e-10)
- Perturbed bounds to avoid exact degeneracy
- Iterative refinement of solutions

### 6.2 Performance Optimization

**Sparsity exploitation:**
- Compressed Sparse Column (CSC) matrix storage
- Sparse FTRAN/BTRAN implementations
- Spike tracking for hyper-sparse vectors

**Memory locality:**
- Eta vectors stored as linked list
- Basis header array enables O(1) row lookup
- Cache-friendly data layouts

**Algorithmic choices:**
- Partial pricing reduces pricing overhead
- Dual simplex minimizes iterations
- Crossover only when needed

### 6.3 Determinism

**Challenges:**
- Floating-point non-associativity
- Thread scheduling non-determinism
- Random seed generation

**Solutions:**
- Fixed random seed parameter
- Single-threaded by default for LP
- Deterministic tie-breaking in pricing

### 6.4 Termination Conditions

**Optimality:**
- All reduced costs satisfy optimality conditions
- Primal feasibility: Ax = b, l <= x <= u (within tolerance)
- Dual feasibility: reduced costs have correct signs

**Infeasibility:**
- Phase I terminates with non-zero artificial variables

**Unboundedness:**
- Entering variable has no leaving candidate (unbounded direction)

**Limits:**
- Iteration limit reached
- Time limit reached
- User termination signal

## 7. References

**Textbooks:**
- Dantzig (1963). "Linear Programming and Extensions"
- Chvatal, V. (1983). "Linear Programming"
- Maros (2003). "Computational Techniques of the Simplex Method"

**Papers:**
- Bartels & Golub (1969). "The simplex method using LU decomposition"
- Forrest & Tomlin (1972). "Updated triangular factors of the basis"
- Harris (1973). "Pivot selection methods of the Devex LP code"

**Implementation References:**
- GLPK (GNU Linear Programming Kit)
- CLP (COIN-OR Linear Programming)

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
