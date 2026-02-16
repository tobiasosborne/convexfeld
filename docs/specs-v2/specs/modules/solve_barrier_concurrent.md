# Module: Solve Barrier & Concurrent

## Purpose

The Solve Barrier & Concurrent module contains the four functions that implement non-simplex solving strategies: the interior-point (barrier) method entry point with quadratic convexity validation, the local concurrent optimizer that races multiple solver instances on shared-memory threads, the distributed concurrent LP optimizer that races solver algorithms across remote remote solvers, and the distributed concurrent MIP optimizer. These functions provide alternative solving paths to the simplex method and implement the concurrent optimization pattern described in the published ConvexFeld documentation, where multiple algorithms or parameter configurations compete in parallel and the first to produce a valid result wins. The barrier entry point additionally handles quadratic objective validation, a prerequisite for interior-point convergence on QP problems (Nocedal and Wright, 2006, Chapter 14).

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

### cxf_solve_concurrent

**Purpose:** Run multiple solver instances in parallel on shared-memory threads, racing for the best MIP solution using a "first to complete wins" strategy with configurable determinism control.

**Signature:**
- Input: `model` : pointer-to-Model - The source model to solve concurrently
- Input: `callbackContext` : pointer - Callback context for user callbacks, passed to each solver instance
- Output: int - Zero on success, or an error code

**Preconditions:**
- The model must be valid with matrix data populated
- The model must not have multiple scenarios (multi-scenario concurrent is not supported)
- The environment must have sufficient threads available for the requested number of concurrent instances
- If user-provided concurrent environments are configured on the model, each must be non-null and valid

**Postconditions:**
- On success with a valid winner: the model's solution info contains the status, objective value, objective bound, and gap from the winning instance; all solutions from all instances have been added to the parent model's solution pool; the winning instance index is recorded; and statistics have been merged
- On success with no winner: the solution status from the first instance is copied to the parent, or INTERRUPTED status is set if no instance produced any result
- On error: the concurrent state is cleared and any partial results are discarded
- In all cases: all cloned models have been freed, all worker threads have been joined, and the concurrent state fields on the parent model have been reset

**Side Effects:**
- Clones the model once for each concurrent instance (allocates per-instance model copies)
- Creates worker threads for instances 1 through N (instance 0 runs on the calling thread's scheduling)
- Copies MIP start hints to each cloned instance
- Copies solver parameters and logging configuration to each instance environment
- Sets per-instance thread counts and instance offsets for search diversification
- Marks non-primary instances as quiet (suppressed output)
- Logs the concurrent setup (instance count, threads per instance)
- Logs the winning instance index on completion
- Modifies the parent model's solution pool by adding solutions from all instances
- Merges timing statistics from the primary instance into the parent

**Error Conditions:**
- Model has multiple scenarios -> returns the scenario conflict error code with INTERRUPTED status
- Insufficient threads for the requested concurrent instance count -> returns the concurrent setup error code with a diagnostic message including the thread limit and requested count
- A user-provided concurrent environment is null -> returns the concurrent setup error code
- Model cloning fails for any instance -> returns the error from clone
- Thread creation fails in non-deterministic mode -> returns out-of-memory
- Solution validation fails -> propagated from the validation check
- Memory allocation fails for solution buffers -> returns out-of-memory

**Behavioral Description:**
This function implements the concurrent MIP optimization pattern, where multiple solver instances with different configurations race in parallel. The first instance to find a good solution wins, reducing overall wall-clock time when the best algorithm variant is unknown a priori. This is a standard portfolio strategy for combinatorial optimization (Xu, Hutter, Hoos, and Leyton-Brown, 2008).

**Phase 1: Validation.** The function validates that concurrent solving is feasible: no multi-scenario conflicts, sufficient threads, and valid concurrent environments. It computes the thread allocation, dividing available threads evenly among instances when in automatic mode, or respecting user-specified per-instance thread counts when concurrent environments are provided.

**Phase 2: Instance spawning.** For each concurrent instance, the function:
1. Clones the source model, creating an independent copy that can solve in parallel.
2. Copies MIP start hints from the parent to the clone, enabling each instance to benefit from user-provided warm-start information.
3. Configures the instance environment: the first instance inherits full logging and parameter setup; subsequent instances run in quiet mode with suppressed output.
4. Sets per-instance search diversification via the instance offset parameter, so each instance explores a different portion of the search space.
5. Creates a worker thread for each instance beyond the first.

**Phase 3: Execution and collection.** The function triggers all instances and then polls their completion flags in a busy-wait loop. The polling loop:
- Yields the thread between checks to avoid excessive CPU consumption.
- Periodically checks for user interrupts (e.g., Ctrl+C).
- Transitions to a sleep-based polling strategy after a threshold number of yield iterations.
- Joins each completed thread and records its objective value and status.

**Phase 4: Winner selection.** The winner is selected based on the determinism mode parameter:
- **Opportunistic mode:** The instance with the best objective value wins, regardless of completion order. This is non-deterministic because thread scheduling affects which instance completes first.
- **First-error mode:** Error-free instances are preferred over instances that encountered out-of-memory errors. A warning is logged if both error and success instances exist, as the winner may differ across runs.
- **Deterministic mode:** A strict preference ordering ensures reproducible results: success instances are preferred, and ties are broken by objective value. Non-determinism warnings are issued when error instances have better objectives than success instances.

**Phase 5: Solution aggregation.** All solutions from all instances are copied to the parent model's solution pool, not just the winner's. This maximizes the number of feasible solutions available for analysis. The function also computes the best primal bound, dual bound, and gap across all instances.

**Phase 6: Cleanup.** All cloned models are freed, worker threads are joined, temporary buffers are deallocated, and the concurrent state fields on the parent model are reset to zero.

**Thread Safety:** The function itself is called from a single thread but creates and manages multiple worker threads internally. Each worker thread operates on its own independent model clone and environment. The parent model is not modified concurrently; all writes to the parent occur after all workers have been joined. Callback state and completion flags use lock-free polling for thread coordination.

**Dependencies:**
- P1.02 (Model) - model cloning, solution info, concurrent environment configuration
- P1.01 (Environment) - thread limits, determinism mode, concurrent parameters
- P3.11 (Threading & Synchronization) - thread creation, joining, yielding, sleep
- P3.10 (Callbacks) - callback initialization and clearing for concurrent instances

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
- The DistributedMIPJobs parameter must be set to a positive value
- If user-provided concurrent environments exist on the model, they must be valid

**Postconditions:**
- On success with an optimal winner: the winner's basis (variable and constraint basis status arrays) has been applied to the parent model, a simplex cleanup pass has been executed to verify and refine the solution, iteration counts and barrier iteration counts have been recorded, and the winning method has been logged
- On success with a non-optimal winner (infeasible, unbounded, etc.): the solution state has been cleared and the terminal status has been stored
- On error: all workers have been terminated and synchronized, resources have been freed, and the solve has been finalized
- In all cases: all worker models and environments have been freed, the callback critical section has been released, and all allocated arrays have been deallocated

**Side Effects:**
- Creates up to three separate worker environments by connecting to remote remote solvers
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
- No distributed workers can be established -> returns the distributed license error code
- Worker environment creation or parameter copy fails -> propagated immediately
- Model cloning or log callback registration fails -> sets early error flag and terminates all workers
- Asynchronous optimization start fails -> sets early error flag and terminates all workers
- Worker status query fails during polling -> sets early error flag
- Basis extraction from the winner fails -> propagated
- Basis application or simplex cleanup fails -> propagated
- Memory allocation fails for basis buffer or worker arrays -> returns out-of-memory

**Behavioral Description:**
This function implements distributed concurrent LP solving, where fundamentally different LP algorithms race on separate remote solvers. Unlike the local concurrent optimizer (cxf_solve_concurrent) which uses threads on a single machine for MIP, this function targets LP problems and leverages distributed computing to run algorithms that would otherwise be mutually exclusive.

**Phase 1: Worker count determination.** The function determines how many workers to create. In automatic mode (no user-provided concurrent environments), the count is capped at three, corresponding to the three fundamentally different LP solving methods. With user-provided environments, the count is the minimum of the available distributed jobs and the user's concurrent count.

**Phase 2: Worker environment setup.** For each worker, the function connects to a remote remote solver via the distributed worker setup mechanism, copies parameters from the parent environment, and assigns a solving method:
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

### cxf_solve_concurrent_mip

**Purpose:** Run multiple solver instances across distributed remote solvers with parameter diversification, aggregate solutions and statistics from all workers, and select the best result based on objective value and optimality gap analysis.

**Signature:**
- Input: `model` : pointer-to-Model - The source model to solve via distributed concurrent MIP
- Input: `callbackContext` : pointer - Callback context for user callbacks
- Output: int - Zero on success, or an error code

**Preconditions:**
- The model must be valid with matrix data populated
- The environment must have distributed remote solver connectivity or a valid license for distributed solving
- The DistributedMIPJobs parameter must be set to a positive value
- No active distributed solve may already be in progress (license lock must be available)
- The model must not have multiple scenarios (multi-scenario distributed concurrent MIP is not supported)

**Postconditions:**
- On success: the solution info contains the aggregated status, objective value, objective bound, optimality gap, solution count, worker count, and winning instance index; statistics (node count, open node count, iteration count, barrier iteration count, and first-order method iteration count) have been summed across all workers; all feasible solutions have been collected; and the best bound across workers has been recorded
- On early exit (error or unsupported configuration): the status is set to INTERRUPTED, any partially started workers have been terminated and synchronized, and the license lock has been released
- In all cases: all worker models and environments have been freed, the license lock has been released, and all allocated arrays have been deallocated

**Side Effects:**
- Acquires a distributed license lock at the beginning and releases it during cleanup
- Creates separate worker environments by connecting to remote remote solvers
- For the first worker, may use the current remote solver as the first distributed worker
- Copies parameters to each worker environment
- Assigns per-worker diversification via the seed parameter (base instance offset plus worker index)
- For the last worker (when six or more workers are configured), sets aggressive MIP parameters: MIPFocus set to prove optimality, increased cutting plane passes, aggressive presolve level, and pseudocost variable branching
- Disables nested distributed and concurrent jobs on all workers
- Delegates the actual distributed execution to the distributed worker dispatch function
- Aggregates statistics by summing across all workers (not just the winner)
- Performs optimality gap checking: compares the relative and absolute gaps against the MIPGap tolerances, and upgrades the status to optimal if the gap is within tolerance
- Copies the winning solution and solution pool to the parent model
- Logs duplicate server warnings, worker count, and winning instance information
- Warns if the PoolSearchMode parameter is set (not supported in distributed concurrent mode)

**Error Conditions:**
- License does not support distributed algorithms -> returns the distributed license error code
- A distributed solve is already in progress (license lock conflict) -> returns an error with a diagnostic message
- Multi-scenario model -> returns the scenario conflict error code
- First worker fails to start -> returns the distributed license error code (fatal: no workers available)
- Later worker fails to start -> continues with fewer workers
- Worker parameter copy fails -> propagated
- Solution validation fails -> propagated
- Distributed worker dispatch fails -> propagated
- Memory allocation fails for worker arrays or solution buffer -> returns out-of-memory

**Behavioral Description:**
This function implements distributed concurrent MIP solving, where multiple solver instances with different parameter configurations race across remote remote solvers. Each worker explores the same search space but with different search strategies, increasing the probability that at least one worker finds a good solution quickly. This is the distributed counterpart of cxf_solve_concurrent, trading shared-memory threading for remote remote solver execution.

**Phase 1: Validation and setup.** The function validates the license, checks for multi-scenario conflicts, and determines the worker count based on the DistributedMIPJobs parameter and the available distributed workers. A solution buffer is allocated for copying solutions. If the PoolSearchMode parameter is set, a warning is logged that it is not supported in this mode.

**Phase 2: Worker environment creation.** For each worker, the function creates a separate environment and connects to a remote remote solver. The first worker may use the current remote solver if the model already has remote solver data. Parameters are copied from the parent to each worker, and search diversification is configured:

- **Standard workers** receive the parent's parameters with a per-worker seed offset (base instance offset plus worker index) for search diversification.
- **The last worker** (when six or more workers are configured) receives aggressive parameter settings designed to complement the standard workers: MIPFocus is set to emphasize proving optimality, cutting plane passes are increased, presolve is set to its most aggressive level, and variable branching uses pseudocost branching. This aggressive-last-worker strategy creates a specialized explorer that may find tighter bounds or prove optimality faster than the standard workers.

All workers have nested distributed jobs, concurrent jobs, and pool search mode disabled.

**Phase 3: Distributed execution.** The function delegates to the distributed worker dispatch function, which clones the model to each worker, starts distributed optimization, coordinates execution, and collects results. This dispatch function handles the actual polling, termination, and solution collection.

**Phase 4: Statistics aggregation.** After all workers complete, the function sums statistics across all workers: node count, open node count, first-order method iteration count, simplex iteration count, and barrier iteration count. The best bound is taken as the minimum across all workers.

**Phase 5: Optimality gap analysis.** The function checks whether the current solution satisfies the optimality gap tolerances (relative and absolute MIPGap parameters). If the gap between the best objective and best bound is within either tolerance, the status is upgraded to OPTIMAL. This handles the case where the distributed solve was interrupted or timed out but the existing solution is provably optimal or near-optimal.

**Phase 6: Result finalization.** The solution info is populated with the final status, scaled objective value, scaled bound, gap, solution count, and worker count. The winning solution values are copied to the parent model's solution buffer.

**Thread Safety:** The function manages distributed workers that run on remote servers. The parent model is only modified after all workers have been terminated and synchronized. A license lock prevents concurrent distributed solves from the same environment.

**Dependencies:**
- P1.02 (Model) - solution info, matrix data, concurrent environment configuration
- P1.01 (Environment) - distributed jobs parameter, MIPGap tolerances, license state, remote solver configuration
- P3.10 (Callbacks) - error callback pre/post processing
- Distributed worker subsystem (not separately specified) - worker environment setup, distributed worker dispatch, license lock management

---

## Module-Level Behavioral Notes

### Relationship Between the Four Functions

The four functions in this module serve complementary roles in the solver's algorithm portfolio:

| Function | Problem Type | Execution Model | Result Type |
|----------|-------------|-----------------|-------------|
| cxf_solve_barrier | LP/QP | Single-threaded entry | Barrier-ready model |
| cxf_solve_concurrent | MIP | Local shared-memory threads | Solution pool |
| cxf_solve_concurrent_distributed | LP | Remote remote solvers | Basis status arrays |
| cxf_solve_concurrent_mip | MIP | Remote remote solvers | Solution pool |

cxf_solve_barrier is the only function that performs algorithmic preprocessing (PSD validation and binary linearization); the three concurrent functions are execution orchestrators that coordinate multiple solver instances.

### Concurrent Optimization Pattern

All three concurrent functions follow a common pattern:

1. **Validate:** Check preconditions (license, scenarios, thread/worker availability).
2. **Spawn:** Create independent solver instances with diversified parameters.
3. **Execute:** Run instances in parallel (threads or remote servers).
4. **Collect:** Poll for completion and gather results.
5. **Select:** Choose the best result based on objective value and determinism rules.
6. **Aggregate:** Merge solutions and statistics from all instances.
7. **Cleanup:** Free all cloned models, environments, and temporary resources.

This portfolio-based approach is a well-established technique in combinatorial optimization, where the best algorithm for a given instance is often unknown a priori (Rice, 1976; Xu, Hutter, Hoos, and Leyton-Brown, 2008).

### Diversification Strategies

| Function | Diversification Method |
|----------|----------------------|
| cxf_solve_concurrent | Per-instance seed offset, user-configurable per-environment parameters |
| cxf_solve_concurrent_distributed | Fixed method assignment (barrier/dual/primal) |
| cxf_solve_concurrent_mip | Per-worker seed offset plus aggressive parameters on the last worker (with 6+ workers) |

The LP distributed function uses fundamentally different algorithms (interior-point versus simplex variants) because there are only a few structurally different LP methods. The MIP functions use seed-based diversification because MIP solving is inherently randomized (e.g., tie-breaking, heuristic selection), so different seeds explore different parts of the search tree.

### Barrier Method Context

cxf_solve_barrier is called before the actual interior-point iterations begin. It does not perform the barrier solve itself; it validates and prepares the model. The interior-point iterations are handled by downstream functions in the barrier subsystem. The PSD validation is a necessary precondition because the barrier method converges only for convex QPs (Nocedal and Wright, 2006, Chapter 14). For non-convex problems, the error messages direct the user to the NonConvex parameter, which enables spatial branching or other non-convex handling strategies.

### Binary Variable Linearization

The algebraic identity x-squared = x for x in {0,1} allows quadratic diagonal terms involving binary variables to be converted to linear terms. For a binary variable x with Q diagonal entry q, the quadratic contribution (1/2) * q * x-squared becomes (1/2) * q * x, which is added directly to the linear objective coefficient. This standard simplification (Boros and Hammer, 2002) reduces the problem's quadratic complexity and can eliminate the need for PSD validation entirely if all Q diagonal entries involve binary variables.

### LP vs MIP Concurrent Result Handling

A key difference between the distributed LP and distributed MIP functions is the result type:

- **LP (cxf_solve_concurrent_distributed):** Extracts basis status arrays (VBasis/CBasis) from the winner and applies them to the parent model with a simplex cleanup pass. This is appropriate because LP solutions are defined by their basis, and applying the optimal basis allows local simplex iterations to verify and refine the solution.

- **MIP (cxf_solve_concurrent and cxf_solve_concurrent_mip):** Collects solution values and adds them to a solution pool. MIP solutions are defined by their variable values (not a basis), so the natural aggregation is to pool all feasible solutions found by any worker.

### Determinism Considerations

The local concurrent optimizer (cxf_solve_concurrent) provides three determinism modes because thread scheduling on shared-memory systems is inherently non-deterministic. The distributed functions do not expose this parameter because remote remote solvers execute independently and determinism is not guaranteed at the distributed level.

### Error Handling Patterns

| Function | Fatal Error Handling | Graceful Degradation |
|----------|---------------------|---------------------|
| cxf_solve_barrier | Immediate return on Q_NOT_PSD or OOM | N/A |
| cxf_solve_concurrent | Thread creation failure is fatal in non-deterministic mode | In deterministic mode, marks failed instance and continues |
| cxf_solve_concurrent_distributed | First worker failure is fatal | Later worker failures reduce count and continue |
| cxf_solve_concurrent_mip | First worker failure is fatal | Later worker failures reduce count and continue |

Both distributed functions follow the pattern: if the first worker fails, the solve cannot proceed (there is no baseline); if a later worker fails, the function continues with fewer workers.

### Resource Management

All concurrent functions follow strict resource cleanup:

1. Worker threads/processes are joined or synchronized before any model is freed.
2. Worker models are freed before worker environments.
3. License locks are released in cleanup, regardless of success or failure.
4. Temporary buffers (solution arrays, basis arrays) are freed last.
5. Parent model concurrent state fields are reset to zero.

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Operation completed normally | All four |
| Out-of-memory | Memory allocation failed | All four |
| Q_NOT_PSD | Quadratic matrix not positive semi-definite | cxf_solve_barrier only |
| Concurrent setup error | Insufficient threads or invalid environments | cxf_solve_concurrent only |
| Distributed license error | No distributed workers or license conflict | Both distributed functions |
| Scenario conflict | Multi-scenario not supported with concurrent | cxf_solve_concurrent, cxf_solve_concurrent_mip |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_solve_barrier | Not thread-safe | Single-threaded barrier entry |
| cxf_solve_concurrent | Creates internal threads; caller must be single-threaded | Workers use independent model clones |
| cxf_solve_concurrent_distributed | Manages remote workers; uses critical section for log relay | Parent modifications occur after all workers complete |
| cxf_solve_concurrent_mip | Manages remote workers; uses license lock | Parent modifications occur after all workers complete |

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
