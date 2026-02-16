# Module: Solve Barrier & Concurrent

## Purpose

The Solve Barrier & Concurrent module contains the functions that implement non-simplex solving strategies: the interior-point (barrier) method entry point with quadratic convexity validation and the distributed concurrent LP optimizer that races solver algorithms across remote solvers. These functions provide alternative solving paths to the simplex method and implement the concurrent optimization pattern described in the published ConvexFeld documentation, where multiple algorithms or parameter configurations compete in parallel and the first to produce a valid result wins. The barrier entry point additionally handles quadratic objective validation, a prerequisite for interior-point convergence on QP problems (Nocedal and Wright, 2006, Chapter 14).

## Functions

### cxf_solve_barrier

**Purpose:** Validate that the quadratic objective matrix is positive semi-definite (PSD) and prepare the model for the interior-point (barrier) method, including binary variable linearization and optional diagonal adjustment.

**Signature:**
- Input: `model` : pointer-to-Model - The model to be solved by the barrier method
- Input: `hasQuadratic` : int - Flag indicating whether the model has quadratic objective terms (zero for pure LP, nonzero for QP)
- Input/Output: `maxNegDiag` : pointer-to-double (nullable) - On output, the magnitude of the most negative diagonal entry requiring adjustment; null if not needed
- Input/Output: `binaryAdjSum` : pointer-to-double (nullable) - On output, the sum of diagonal adjustments for binary variables; null if not needed
- Input/Output: `binaryAdjCount` : pointer-to-int (nullable) - On output, the count of binary variables whose diagonal entries were linearized; null if not needed
- Input/Output: `factorCache` : pointer-to-pointer (nullable) - On input, a previously computed factorization to reuse; on output, the computed factorization for later reuse; null to build and discard
- Input/Output: `auxCache` : pointer-to-pointer (nullable) - Auxiliary factorization data, paired with factorCache
- Input/Output: `wasModified` : pointer-to-int (nullable) - On output, nonzero if diagonal entries were modified to achieve PSD; null if not needed
- Input: `solverState` : pointer - Additional solver state for the factorization subsystem
- Output: int - Zero on success, or an error code

**Preconditions:**
- The model must be valid and have its matrix data populated
- If `hasQuadratic` is nonzero, the model must have a quadratic objective matrix (Q matrix) with row indices, column indices, and coefficient values
- The environment must have a valid Q tolerance parameter controlling the maximum allowable diagonal adjustment

**Postconditions:**
- On success with `hasQuadratic` nonzero: the Q matrix has been validated as PSD (possibly after adjustment), binary variable diagonal terms have been linearized into the objective, and the Q term count has been updated to exclude binary entries
- On success with `hasQuadratic` zero: the Q term count is set to zero (pure LP mode)
- On Q_NOT_PSD error: the Q matrix cannot be made PSD within the tolerance, and an error message has been logged suggesting the NonConvex parameter
- On out-of-memory error: a factorization allocation failed
- If factorCache is non-null, the computed factorization is returned for potential reuse

**Side Effects:**
- May modify the Q matrix in the model's matrix data (removes binary diagonal entries, adjusts near-zero negative diagonals)
- May modify the linear objective coefficients (adds linearized binary diagonal contributions)
- May apply model modifications and rebuild the row-major matrix representation if diagonal adjustments are made
- Logs a warning if diagonal adjustment is performed
- Logs an error with the NonConvex parameter suggestion if the matrix is not PSD
- Allocates or frees factorization structures depending on cache mode

**Error Conditions:**
- The Q matrix has a diagonal entry more negative than the Q tolerance -> returns the Q_NOT_PSD error code with an error message
- The Q matrix factorization reveals non-PSD structure (off-diagonal dominance) -> returns the Q_NOT_PSD error code
- The required diagonal adjustment exceeds the Q tolerance -> returns the Q_NOT_PSD error code with the adjustment magnitude in the error message
- Memory allocation for factorization fails -> returns the out-of-memory error code

**Behavioral Description:**
This function serves as the convexity gatekeeper for the barrier (interior-point) method. The barrier method requires a convex quadratic objective for convergence guarantees (Nocedal and Wright, 2006, Chapter 14), and this function enforces that requirement before the actual interior-point iterations begin.

**Phase 1: Row data preparation.** The function ensures the model's row-major matrix representation is current by invoking the row data preparation helper.

**Phase 2: PSD validation.** If no cached factorization is available, the function scans the Q matrix diagonal entries to perform a preliminary PSD check:

1. **Binary variable filtering.** Diagonal entries corresponding to binary variables are skipped during the PSD check, because for binary variables x in {0,1}, the identity x-squared = x holds, allowing their quadratic terms to be converted to linear terms.

2. **Diagonal classification.** Each non-binary diagonal entry is classified: if it is more negative than the negated absolute value of the Q tolerance, the matrix is definitively not PSD and an error is returned immediately. If it is negative but within tolerance, the function flags the matrix for factorization-based verification.

3. **Factorization verification.** When the diagonal scan is inconclusive (small negative or off-diagonal entries present), a Cholesky-like factorization is built to definitively determine PSD status. If the factorization succeeds, the matrix is PSD; if it fails, the function computes the required diagonal adjustment and checks it against the tolerance.

**Phase 3: Binary variable linearization.** For models confirmed as PSD (or pure LP), the function converts binary variable Q diagonal terms to linear objective contributions. For each binary variable with diagonal coefficient q, the term (1/2) * q * x-squared simplifies to (1/2) * q * x, so the function adds (1/2) * q to the variable's linear objective coefficient and removes the entry from the Q matrix. This is a standard algebraic simplification for binary quadratic programs (Boros and Hammer, 2002).

**Phase 4: Diagonal adjustment.** If the factorization analysis identifies that a small positive perturbation to the diagonal would make the matrix PSD, and the required adjustment is within the Q tolerance, the function applies the adjustment, logs a warning with the adjustment magnitude, applies the model modifications, and rebuilds the row-major matrix.

**Phase 5: Factorization caching.** The factorization is either returned in the cache parameters for reuse by subsequent calls (incremental solves) or freed if no cache is requested.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded solve context.

**Dependencies:**
- P1.02 (Model) - reads matrix data, Q matrix, variable types, objective coefficients
- P1.01 (Environment) - reads Q tolerance parameter, NonConvex parameter
- P3.15 (Matrix Finalization) - row data preparation
- Internal factorization subsystem (not separately specified) - Cholesky-like factorization for PSD verification

---

### cxf_solve_concurrent_distributed

**Purpose:** Run multiple LP solver instances across distributed remote solvers, each using a different solving method (barrier, dual simplex, primal simplex), and apply the winning basis to the parent model with a simplex cleanup pass.

**Signature:**
- Input: `model` : pointer-to-Model - The source model to solve via distributed concurrent LP
- Input: `callbackContext` : pointer - Callback context for user callbacks
- Output: int - Zero on success, or an error code

**Preconditions:**
- The model must be valid with matrix data populated
- The environment must have distributed remote solver connectivity configured
- The distributed jobs parameter must be set to a positive value
- If user-provided concurrent environments exist on the model, they must be valid

**Postconditions:**
- On success with an optimal winner: the winner's basis (variable and constraint basis status arrays) has been applied to the parent model, a simplex cleanup pass has been executed to verify and refine the solution, iteration counts and barrier iteration counts have been recorded, and the winning method has been logged
- On success with a non-optimal winner (infeasible, unbounded, etc.): the solution state has been cleared and the terminal status has been stored
- On error: all workers have been terminated and synchronized, resources have been freed, and the solve has been finalized
- In all cases: all worker models and environments have been freed, the callback critical section has been released, and all allocated arrays have been deallocated

**Side Effects:**
- Creates up to three separate worker environments by connecting to remote solvers
- Clones the model to each worker environment
- Registers a log callback on each worker that relays log messages to the parent through a critical section for thread safety
- Starts asynchronous optimization on each worker
- Polls workers periodically, sleeping between polls
- Terminates all workers once one completes
- Extracts basis status arrays (VBasis and CBasis) from the winning worker
- Applies the basis to the parent model and runs a simplex cleanup pass (temporarily in quiet mode)
- Records iteration count, barrier iteration count, and runtime in the solution info
- Logs the winning method name and solve statistics
- Finalizes the solve state on the parent model

**Error Conditions:**
- No distributed workers can be established -> returns the distributed setup error code
- Worker environment creation or parameter copy fails -> propagated immediately
- Model cloning or log callback registration fails -> sets early error flag and terminates all workers
- Asynchronous optimization start fails -> sets early error flag and terminates all workers
- Worker status query fails during polling -> sets early error flag
- Basis extraction from the winner fails -> propagated
- Basis application or simplex cleanup fails -> propagated
- Memory allocation fails for basis buffer or worker arrays -> returns out-of-memory

**Behavioral Description:**
This function implements distributed concurrent LP solving, where fundamentally different LP algorithms race on separate remote solvers. It leverages distributed computing to run algorithms that would otherwise be mutually exclusive.

**Phase 1: Worker count determination.** The function determines how many workers to create. In automatic mode (no user-provided concurrent environments), the count is capped at three, corresponding to the three fundamentally different LP solving methods. With user-provided environments, the count is the minimum of the available distributed jobs and the user's concurrent count.

**Phase 2: Worker environment setup.** For each worker, the function connects to a remote solver via the distributed worker setup mechanism, copies parameters from the parent environment, and assigns a solving method:
- Worker 0: Barrier (interior-point) method, which is often fastest for dense LP problems.
- Worker 1: Dual simplex, which is effective for sparse and well-conditioned problems.
- Worker 2 and beyond: Primal simplex, as a numerically robust fallback.

Each worker also has nested concurrent jobs disabled and output suppressed.

**Phase 3: Model distribution and async start.** The function clones the model to each worker and registers a log relay callback. The callback uses a critical section (mutex) to safely relay worker log messages to the parent environment. Each worker then starts asynchronous optimization.

**Phase 4: Polling.** The function enters a polling loop that periodically queries each worker's runtime and status. A worker is considered complete when it reaches a terminal status (optimal, suboptimal, infeasible, infeasible-or-unbounded, or unbounded). The loop sleeps between iterations and checks for user termination requests and callback interrupts.

**Phase 5: Termination and winner selection.** Once any worker completes (or all workers are no longer in progress, or the user requests termination), all workers are terminated and waited on. The first worker to reach a terminal status is selected as the winner. If no worker produced a terminal status, the first worker is used as a fallback.

**Phase 6: Basis extraction and cleanup.** For optimal solutions, the function extracts the variable basis status (VBasis) and constraint basis status (CBasis) arrays from the winning worker. It then applies this basis to the parent model with a simplex cleanup pass, which may perform a small number of additional simplex iterations to verify and refine the solution. Any extra simplex iterations are logged.

**Phase 7: Statistics and logging.** Iteration counts, barrier iteration counts, and runtime are recorded. The function logs the winning method name (barrier, dual simplex, or primal simplex) and the solve statistics.

**Thread Safety:** The function manages distributed workers that run asynchronously on remote servers. A critical section protects the log message relay between workers and the parent. All parent model modifications occur after all workers have been terminated and synchronized.

**Dependencies:**
- P1.02 (Model) - model cloning, solution state, basis application
- P1.01 (Environment) - distributed jobs parameter, remote solver configuration, output flag
- P3.11 (Threading & Synchronization) - critical section management, sleep
- P3.10 (Callbacks) - callback structure initialization and cleanup, log callback registration
- Distributed worker subsystem (not separately specified) - worker environment setup, async optimization, model synchronization

---

## Module-Level Behavioral Notes

### Relationship Between the Functions

The functions in this module serve complementary roles in the solver's algorithm portfolio:

| Function | Problem Type | Execution Model | Result Type |
|----------|-------------|-----------------|-------------|
| cxf_solve_barrier | LP/QP | Single-threaded entry | Barrier-ready model |
| cxf_solve_concurrent_distributed | LP | Remote remote solvers | Basis status arrays |

cxf_solve_barrier performs algorithmic preprocessing (PSD validation and binary linearization); cxf_solve_concurrent_distributed is an execution orchestrator that coordinates multiple solver instances.

### Concurrent Optimization Pattern

The concurrent function follows this common pattern:

1. **Validate:** Check preconditions (scenarios, worker availability).
2. **Spawn:** Create independent solver instances with diversified parameters.
3. **Execute:** Run instances in parallel on remote servers.
4. **Collect:** Poll for completion and gather results.
5. **Select:** Choose the best result based on objective value and determinism rules.
6. **Aggregate:** Merge solutions and statistics from all instances.
7. **Cleanup:** Free all cloned models, environments, and temporary resources.

This portfolio-based approach is a well-established technique in optimization, where the best algorithm for a given instance is often unknown a priori (Rice, 1976; Xu, Hutter, Hoos, and Leyton-Brown, 2008).

### Diversification Strategies

| Function | Diversification Method |
|----------|----------------------|
| cxf_solve_concurrent_distributed | Fixed method assignment (barrier/dual/primal) |

The LP distributed function uses fundamentally different algorithms (interior-point versus simplex variants) because there are only a few structurally different LP methods.

### Barrier Method Context

cxf_solve_barrier is called before the actual interior-point iterations begin. It does not perform the barrier solve itself; it validates and prepares the model. The interior-point iterations are handled by downstream functions in the barrier subsystem. The PSD validation is a necessary precondition because the barrier method converges only for convex QPs (Nocedal and Wright, 2006, Chapter 14). For non-convex problems, the error messages direct the user to the NonConvex parameter, which enables spatial branching or other non-convex handling strategies.

### Binary Variable Linearization

The algebraic identity x-squared = x for x in {0,1} allows quadratic diagonal terms involving binary variables to be converted to linear terms. For a binary variable x with Q diagonal entry q, the quadratic contribution (1/2) * q * x-squared becomes (1/2) * q * x, which is added directly to the linear objective coefficient. This standard simplification (Boros and Hammer, 2002) reduces the problem's quadratic complexity and can eliminate the need for PSD validation entirely if all Q diagonal entries involve binary variables.

### Determinism Considerations

The distributed function does not expose a determinism parameter because remote solvers execute independently and determinism is not guaranteed at the distributed level.

### Error Handling Patterns

| Function | Fatal Error Handling | Graceful Degradation |
|----------|---------------------|---------------------|
| cxf_solve_barrier | Immediate return on Q_NOT_PSD or OOM | N/A |
| cxf_solve_concurrent_distributed | First worker failure is fatal | Later worker failures reduce count and continue |

The distributed function follows the pattern: if the first worker fails, the solve cannot proceed (there is no baseline); if a later worker fails, the function continues with fewer workers.

### Resource Management

All functions follow strict resource cleanup:

1. Worker threads/processes are joined or synchronized before any model is freed.
2. Worker models are freed before worker environments.
3. Temporary buffers (solution arrays, basis arrays) are freed last.
4. Parent model concurrent state fields are reset to zero.

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Operation completed normally | Both |
| Out-of-memory | Memory allocation failed | Both |
| Q_NOT_PSD | Quadratic matrix not positive semi-definite | cxf_solve_barrier only |
| Distributed setup error | No distributed workers available | cxf_solve_concurrent_distributed only |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_solve_barrier | Not thread-safe | Single-threaded barrier entry |
| cxf_solve_concurrent_distributed | Manages remote workers; uses critical section for log relay | Parent modifications occur after all workers complete |

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.01, P1.02, P3.10, P3.11, P3.15 (dependent specs)
[x] All algorithms cite published sources (Nocedal & Wright, Boros & Hammer, Rice, Xu et al.)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Boros, E. and Hammer, P.L. (2002). "Pseudo-Boolean Optimization." *Discrete Applied Mathematics*, 123(1-3):155-225.
- Nocedal, J. and Wright, S.J. (2006). *Numerical Optimization*. Second Edition. Springer. Springer Series in Operations Research.
- Rice, J.R. (1976). "The Algorithm Selection Problem." *Advances in Computers*, 15:65-118.
- Xu, L., Hutter, F., Hoos, H.H., and Leyton-Brown, K. (2008). "SATzilla: Portfolio-based Algorithm Selection for SAT." *Journal of Artificial Intelligence Research*, 32:565-606.
