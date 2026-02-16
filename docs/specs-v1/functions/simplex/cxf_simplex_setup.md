# cxf_simplex_setup

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Prepares the solver state for the main simplex iteration loop by initializing working arrays, pricing structures, reduced costs, and phase indicators. This function is called after preprocessing completes and before iterations begin. It reads parameters from the environment, allocates temporary working arrays, determines the initial phase (Phase I for infeasible starting point, Phase II otherwise), and configures all iteration control variables.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Initialized solver state from cxf_simplex_init | Non-null, valid state | Yes |
| env | CxfEnv* | Environment containing solver parameters | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory |

### 2.3 Side Effects

- Allocates reduced cost and dual value arrays
- Allocates pricing state structure
- Allocates timing state structure
- Allocates auxiliary buffer (64KB)
- Initializes phase indicator and control flags
- Stores parameters (tolerance, iteration limit) in state

## 3. Contract

### 3.1 Preconditions

- [ ] State must be initialized (cxf_simplex_init completed)
- [ ] cxf_simplex_crash and cxf_simplex_preprocess should have been called
- [ ] Problem dimensions (numVars, numConstrs) are set in state
- [ ] Objective coefficients are available in state
- [ ] Working bounds are set in state

### 3.2 Postconditions

- [ ] Reduced cost array is allocated and initialized from objective
- [ ] Dual value array is allocated and zero-initialized
- [ ] Pricing state is allocated and initialized
- [ ] Phase indicator is set (1 for infeasible, 2 for feasible)
- [ ] Iteration control fields are initialized
- [ ] Auxiliary buffer is allocated

### 3.3 Invariants

- [ ] Problem dimensions remain unchanged
- [ ] Objective coefficients remain unchanged
- [ ] Bound arrays remain unchanged

## 4. Algorithm

### 4.1 Overview

The setup phase bridges preprocessing and iteration. It reads solver parameters from the environment, allocates working arrays needed during iterations, computes initial reduced costs (which equal objective coefficients before any pivots), determines whether Phase I is needed (based on bound feasibility), and initializes all control variables to their starting values.

### 4.2 Detailed Steps

1. **Extract dimensions** from state: numVars and numConstrs.

2. **Read parameters from environment**:
   - IterationLimit: maximum simplex iterations (default: 10,000,000)
   - FeasibilityTol: constraint violation tolerance (default: 1e-6)
   - OptimalityTol: reduced cost tolerance (default: 1e-6)
   - SimplexPricing: pricing strategy (0=auto, 1=partial, 2=full)

3. **Store parameters in state**.

4. **Allocate reduced cost array**: numVars doubles.
   - Initialize from objective coefficients

5. **Allocate dual value array**: numConstrs doubles.
   - Initialize to zero

6. **Allocate and initialize pricing state**:
   - Allocate PricingState structure
   - Call cxf_pricing_init

7. **Allocate and initialize timing state**.

8. **Initialize eta vector list**:
   - Set etaListHead = NULL
   - Set etaCount = 0

9. **Initialize work counters**:
   - workCounter = 0.0
   - removedRows = 0

10. **Determine initial phase**:
    - Scan variable bounds for feasibility violations
    - If any violations found: phase = 1
    - Otherwise: phase = 2

11. **Initialize control flags**.

12. **Initialize objective value** to 0.0.

13. **Allocate auxiliary buffer** (64KB).

### 4.3 Pseudocode

```
SETUP_ITERATION(state, env):
    n := state.numVars
    m := state.numConstrs

    // Read parameters
    iterLimit := GET_PARAM(env, "IterationLimit", default=10^7)
    feasTol := GET_PARAM(env, "FeasibilityTol", default=1e-6)
    optTol := GET_PARAM(env, "OptimalityTol", default=1e-6)

    state.maxIterations := iterLimit
    state.tolerance := optTol

    // Allocate working arrays
    state.reducedCosts := ALLOCATE(n * 8 bytes)
    state.dualValues := ALLOCATE(m * 8 bytes)

    // Initialize reduced costs = objective coefficients
    FOR i := 0 TO n-1:
        state.reducedCosts[i] := state.objCoeffs[i]

    // Initialize dual values = 0
    ZERO_FILL(state.dualValues, m)

    // Initialize pricing
    state.pricingState := ALLOCATE_PRICING_STATE()
    INIT_PRICING(state.pricingState, state, env)

    // Initialize eta list (empty)
    state.etaHead := NULL
    state.etaCount := 0

    // Determine phase
    hasInfeasibility := FALSE
    FOR i := 0 TO n-1:
        IF state.lb[i] > state.ub[i] + feasTol:
            hasInfeasibility := TRUE
            BREAK

    state.phase := IF hasInfeasibility THEN 1 ELSE 2

    // Initialize flags
    state.objValue := 0.0
    state.workCounter := 0.0
    CLEAR_ALL_FLAGS(state)

    // Allocate auxiliary buffer
    state.auxBuffer := ALLOCATE(65536 bytes)

    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- O(n + m) for allocation and initialization

### 5.2 Space Complexity

- **Auxiliary space:** O(n + m)

Memory allocated:
- Reduced costs: n * 8 bytes
- Dual values: m * 8 bytes
- Pricing state: ~256 bytes + candidate arrays
- Auxiliary buffer: 64 KB

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Reduced cost allocation fails | 1001 | Cannot allocate n doubles |
| Dual value allocation fails | 1001 | Cannot allocate m doubles |
| Pricing state allocation fails | 1001 | Cannot allocate pricing structure |
| Auxiliary buffer allocation fails | 1001 | Cannot allocate 64KB buffer |

### 6.2 Error Behavior

On allocation failure:
- Returns error code immediately
- Previously allocated arrays remain
- Caller handles cleanup via cxf_simplex_final

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No variables | numVars=0 | Allocates zero-size arrays, succeeds |
| No constraints | numConstrs=0 | Allocates zero-size dual array, succeeds |
| All bounds infeasible | All lb > ub | Phase set to 1 |
| Zero objective | All c=0 | Reduced costs all zero |

## 8. Thread Safety

**Thread-safe:** Yes

The function only reads from the environment and writes to the state.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_get_intparam | Parameters | Read integer parameters |
| cxf_get_dblparam | Parameters | Read double parameters |
| cxf_alloc_temp | Memory | Temporary allocation |
| cxf_pricing_init | Pricing | Initialize pricing state |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_solve_lp | Simplex | After crash and preprocess |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_init | Allocates main state |
| cxf_simplex_crash | Establishes initial basis |
| cxf_simplex_preprocess | Preprocesses problem |
| cxf_simplex_iterate | Uses structures set up here |

## 11. Design Notes

### 11.1 Design Rationale

**Initial reduced costs from objective:** For a starting basis with all slacks basic, reduced costs of structural variables equal their objective coefficients.

**Two-phase method detection:** Checking for bound violations determines if Phase I is needed.

**Large auxiliary buffer:** 64KB provides space for eta vectors during iterations.

## 12. References

- Chvatal, V. (1983). "Linear Programming" - Two-phase simplex method
- Maros, I. (2003). "Computational Techniques of the Simplex Method"

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
