# Module: Simplex Core

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 11

## 1. Overview

### 1.1 Purpose

The Simplex Core module implements the main control flow and orchestration for the simplex algorithm. It serves as the top-level driver that coordinates all other LP solver modules (Basis, Pricing, Ratio Test, Pivot) into a coherent optimization procedure.

The module provides the complete simplex solution pipeline: state initialization, crash basis construction, preprocessing, the main iteration loop (pricing, ratio test, pivot), phase transitions (Phase I for feasibility, Phase II for optimality), solution refinement, and cleanup. It manages termination conditions including optimality, infeasibility, unboundedness, and resource limits (iteration count, time).

The simplex core also implements the two-phase simplex method: Phase I minimizes constraint violations to find an initial feasible solution, while Phase II optimizes the actual objective function. The module handles transitions between phases and detects when problems are infeasible or unbounded.

### 1.2 Responsibilities

This module is responsible for:

- [x] Allocating and initializing solver state (SolverState structure)
- [x] Constructing initial crash basis through heuristic selection
- [x] Preprocessing: scaling, bound tightening, redundancy removal
- [x] Main iteration loop control and convergence checking
- [x] Phase I/II method orchestration and transitions
- [x] Termination condition detection (optimal, infeasible, unbounded)
- [x] Resource limit enforcement (iterations, time)
- [x] Solution refinement for numerical accuracy
- [x] State cleanup and memory deallocation
- [x] Algorithm auto-selection (primal/dual/barrier)

This module is NOT responsible for:

- [ ] Pricing computation (delegated to Pricing module)
- [ ] Ratio test computation (delegated to Ratio Test module)
- [ ] Pivot execution (delegated to Pivot module)
- [ ] Basis transformations (delegated to Basis module)
- [ ] Matrix storage (delegated to Matrix module)

### 1.3 Design Philosophy

The module follows an orchestration pattern with clear separation of concerns:

**Delegation**: The simplex core coordinates but does not implement numerical algorithms. Each specialized operation (pricing, ratio test, pivot, FTRAN/BTRAN) is delegated to the appropriate module.

**Two-level iteration**: An outer loop controls overall iteration count and convergence, while an inner stabilization loop ensures the basis remains stable between major iterations.

**Fail-fast error handling**: Errors from sub-functions propagate immediately, triggering appropriate cleanup without continuing with partial results.

**Resource awareness**: Time and iteration limits are checked at each iteration, enabling graceful termination with the best solution found so far.

**Phase management**: Clear state machines govern Phase I/II transitions, ensuring mathematical correctness of the two-phase method.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_solve_lp | Main LP solve entry point | cxf_optimize |
| cxf_simplex_init | Allocate and initialize solver state | cxf_solve_lp |
| cxf_simplex_crash | Construct initial crash basis | cxf_solve_lp |
| cxf_simplex_preprocess | Preprocess model (scaling, tightening) | cxf_solve_lp |
| cxf_simplex_setup | Setup iteration structures | cxf_solve_lp |
| cxf_simplex_iterate | Progress logging during iteration | Main loop |
| cxf_simplex_step | Core iteration: pricing and pivot | Main loop |
| cxf_simplex_step2 | Phase 2 bound adjustments | Main loop |
| cxf_simplex_step3 | Optimality checking | Main loop |
| cxf_simplex_post_iterate | Post-iteration bookkeeping | Main loop |
| cxf_simplex_phase_end | Phase completion/transition | Main loop |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| SolverState | Complete solver runtime state (~1192 bytes) |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| PHASE_ONE | 1 | Minimize infeasibilities |
| PHASE_TWO | 2 | Optimize objective |
| STATUS_OPTIMAL | 0 | Optimal solution found |
| STATUS_INFEASIBLE | 3 | Problem is infeasible |
| STATUS_UNBOUNDED | 5 | Problem is unbounded |
| STATUS_ITERATION_LIMIT | 10 | Iteration limit reached |
| STATUS_TIME_LIMIT | 9 | Time limit reached |
| STATUS_NUMERIC | 13 | Numerical difficulties |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| cxf_simplex_refine | Solution refinement for accuracy |
| cxf_simplex_cleanup | Restore eliminated variables |
| cxf_simplex_final | Deallocate all memory |
| cxf_simplex_perturbation | Anti-cycling perturbation |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| select_algorithm | Auto-select primal/dual/barrier | cxf_solve_lp |
| check_termination | Evaluate stopping conditions | Main loop |
| compute_tolerance | Problem-size dependent tolerance | cxf_solve_lp |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| SolverState | struct | Solve duration | Per-solver |
| Phase indicator | int | Solve duration | Per-solver |
| Iteration count | int | Solve duration | Per-solver |
| Objective value | double | Solve duration | Per-solver |

### 4.2 State Lifecycle

```
MODEL_RECEIVED
      |
      v (cxf_simplex_init)
  INITIALIZED --> State allocated, arrays copied
      |
      v (cxf_simplex_crash)
CRASH_COMPLETE --> Initial basis selected
      |
      v (cxf_simplex_preprocess)
PREPROCESSED --> Scaling, tightening applied
      |
      v (cxf_simplex_setup)
    READY --> Reduced costs, pricing initialized
      |
      v (iteration loop)
  ITERATING --> Main simplex loop
      |   |
      |   v (cxf_simplex_step) --> Pricing, ratio test, pivot
      |   |
      |   v (cxf_simplex_phase_end) --> Phase transitions
      |   |
      |   +--< continue if not terminated
      |
      v (termination detected)
  TERMINATED --> Optimal, infeasible, unbounded, or limit
      |
      v (cxf_simplex_refine)
   REFINED --> Solution polished
      |
      v (cxf_simplex_cleanup)
 CLEANED_UP --> Eliminated variables restored
      |
      v (cxf_simplex_final)
  DESTROYED --> All memory freed
```

### 4.3 State Invariants

At all times, the following must be true:

- [x] Exactly m variables are basic (one per constraint)
- [x] Phase indicator is 1 or 2
- [x] Iteration count is non-negative and bounded by limit
- [x] Basic solution satisfies constraints within tolerance

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | Allocation functions | State and array allocation |
| Basis | FTRAN, BTRAN, refactor | Basis operations |
| Pricing | Steepest edge selection | Entering variable |
| Ratio Test | Harris ratio test | Leaving variable |
| Pivot | Primal pivot | Basis updates |
| Parameters | Tolerances, limits | Configuration |
| Timing | Elapsed time tracking | Time limits |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Parameters, Matrix (problem data available)
- **Before:** Solution is needed

This module initializes in order:
1. cxf_simplex_init (allocate state)
2. cxf_simplex_crash (select basis)
3. cxf_simplex_preprocess (reduce problem)
4. cxf_simplex_setup (prepare iteration)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| Standard C | Library | memset, memcpy |
| Math library | Library | fabs, sqrt |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| API (cxf_optimize) | cxf_solve_lp | Stable |
| Crossover | Solution handoff | Stable |
| MIP solver | LP relaxation solves | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [x] cxf_solve_lp signature and return codes
- [x] Termination status codes (0, 3, 5, 9, 10, 13)
- [x] Solution data locations in model structure

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [x] State is fully cleaned up on any exit path
- [x] Solution is mathematically correct when status is OPTIMAL
- [x] Infeasibility/unboundedness detection is correct
- [x] Resource limits are respected

### 7.2 Required Invariants

What this module requires from others:

- [x] Model contains valid problem data
- [x] Environment provides valid parameters
- [x] Sub-modules maintain their documented contracts

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Memory allocation | NULL return from allocator |
| Infeasibility | Phase I fails to find feasible point |
| Unboundedness | Ratio test finds no limiting row |
| Iteration limit | Counter exceeds parameter |
| Time limit | Elapsed time exceeds parameter |
| Numerical issues | Basis becomes ill-conditioned |

### 8.2 Error Propagation

How errors flow through this module:

```
Sub-function error --> Capture first error
                  --> Continue to cleanup
                  --> Return original error

Resource limit --> Set status
             --> Exit iteration loop
             --> Return with partial solution

Infeasible/Unbounded --> Set status
                    --> Exit cleanly with proof
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Memory failure | Abort, return 1001 |
| Infeasible | Return status 3, no solution |
| Unbounded | Return status 5, ray available |
| Limits | Return best solution found |
| Numeric | Refactorize and continue, or abort |

## 9. Thread Safety

### 9.1 Concurrency Model

Each solver instance operates independently. Multiple models can be solved concurrently on different threads with no interaction.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| None internal | N/A | Per-solver isolation |
| Model lock | Model during solve | Caller responsibility |

### 9.3 Thread Safety Guarantees

- [x] Concurrent solves on different models are safe
- [x] Same model cannot be solved concurrently (caller must prevent)
- [x] Environment parameters must not change during solve

### 9.4 Known Race Conditions

Model modification during solve is undefined behavior.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_simplex_init | O(n + m) | O(n + m) |
| cxf_simplex_crash | O(n * m) sparse | O(n + m) |
| cxf_simplex_preprocess | O(nnz) | O(n + m) |
| Single iteration | O(m * d) | O(m) |
| Total solve | O(m^2 * n) average | O(m^2 + mn) |

Where: n = variables, m = constraints, d = density, nnz = nonzeros

### 10.2 Hot Paths

The main iteration loop is the critical path:
1. Pricing (O(sqrt(n)) with partial pricing)
2. BTRAN (O(k * d) where k = eta count)
3. FTRAN (O(k * d))
4. Ratio test (O(m) worst case)
5. Pivot (O(d) for eta creation)

Total per iteration: O(m) to O(m * d) depending on sparsity

### 10.3 Memory Usage

- SolverState: ~1192 bytes base
- Per-variable: ~56 bytes (bounds, objective, status, flags)
- Per-constraint: ~24 bytes (RHS, sense, slack)
- Eta chain: 100-1000 eta vectors, 1-50 MB
- Working arrays: O(m) for transformations

Typical 10,000 var + 5,000 constraint problem: ~1-2 MB state + eta growth

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_solve_lp](functions/simplex/cxf_solve_lp.md) - Main LP solve entry point
2. [cxf_simplex_init](functions/simplex/cxf_simplex_init.md) - Allocate and initialize state
3. [cxf_simplex_crash](functions/simplex/cxf_simplex_crash.md) - Construct initial basis
4. [cxf_simplex_preprocess](functions/simplex/cxf_simplex_preprocess.md) - Preprocess model
5. [cxf_simplex_setup](functions/simplex/cxf_simplex_setup.md) - Setup iteration structures
6. [cxf_simplex_iterate](functions/simplex/cxf_simplex_iterate.md) - Progress logging
7. [cxf_simplex_step](functions/simplex/cxf_simplex_step.md) - Core iteration
8. [cxf_simplex_step2](functions/simplex/cxf_simplex_step2.md) - Phase 2 adjustments
9. [cxf_simplex_step3](functions/simplex/cxf_simplex_step3.md) - Optimality checking
10. [cxf_simplex_post_iterate](functions/simplex/cxf_simplex_post_iterate.md) - Post-iteration
11. [cxf_simplex_phase_end](functions/simplex/cxf_simplex_phase_end.md) - Phase transitions

### Internal Functions

1. cxf_simplex_refine - Solution refinement
2. cxf_simplex_cleanup - Variable restoration
3. cxf_simplex_final - Memory deallocation
4. cxf_simplex_perturbation - Anti-cycling

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Two-phase method | Guaranteed feasibility detection | Big-M method |
| Modular delegation | Clean interfaces, testability | Monolithic implementation |
| Early termination | Resource-bounded solving | Run to completion only |
| Auto algorithm selection | User convenience | Force manual selection |

### 12.2 Known Limitations

- [x] Simplex worst-case is exponential (rare in practice)
- [x] Very large problems may exhaust eta storage
- [x] Highly degenerate problems may cycle (mitigated by perturbation)

### 12.3 Future Improvements

- [x] Parallel simplex for large problems
- [x] GPU acceleration for dense operations
- [x] Adaptive tolerance adjustment during solve
- [x] Warm start from previous solution

## 13. References

- Dantzig, G. (1963). *Linear Programming and Extensions*.
- Chvatal, V. (1983). *Linear Programming*.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*.
- Koberstein, A. (2005). "The dual simplex method, techniques for a fast and stable implementation."
- Bixby, R.E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress."

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
