# cxf_simplex_perturbation

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Applies anti-cycling perturbation to prevent the simplex algorithm from cycling infinitely in degenerate linear programs. This implements the Wolfe (1963) perturbation method, which adds small random perturbations to variable bounds to break degeneracy. The perturbations are small enough to not affect the optimal solution but sufficient to ensure finite termination. The function is called proactively during early iterations to prevent cycling before it starts.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with working bounds | Non-null, valid state | Yes |
| env | CxfEnv* | Environment containing tolerances | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory |

### 2.3 Side Effects

- Modifies working bounds (lb, ub) by adding/subtracting small perturbations
- Stores perturbation values for later removal
- Sets perturbationApplied flag to prevent reapplication

## 3. Contract

### 3.1 Preconditions

- [ ] State is initialized with valid bounds
- [ ] Objective coefficients are set
- [ ] Perturbation has not already been applied (flag is 0)

### 3.2 Postconditions

- [ ] If perturbation applied: bounds modified by small epsilon values
- [ ] If perturbation applied: perturbation arrays store delta values
- [ ] If perturbation applied: perturbationApplied flag = 1
- [ ] Bounds remain valid (lb <= ub after perturbation)

### 3.3 Invariants

- [ ] Optimal solution unchanged (perturbations are infinitesimal)
- [ ] Problem dimensions unchanged

## 4. Algorithm

### 4.1 Overview

The Wolfe perturbation method adds small random perturbations to variable bounds to break ties in degenerate pivots. The key insight is that perturbations should be:
- Small enough to not change the optimal solution
- Different for each variable (to break symmetry)
- Deterministic (for reproducibility)
- Removable (to recover original problem bounds)

### 4.2 Detailed Steps

1. **Check if already applied**: If perturbationApplied flag is set, return immediately.

2. **Extract parameters**: numVars, FeasibilityTol, CXF_INFINITY.

3. **Calculate base perturbation scale**: basePerturbScale = FeasibilityTol * 1e-6.

4. **Allocate perturbation arrays**: lbPerturb, ubPerturb.

5. **Generate deterministic random seed**: Hash problem characteristics.

6. **For each variable j**:

   a. **Skip unbounded variables**: If lb[j] = -infinity and ub[j] = +infinity, skip.

   b. **Compute variable-specific scale**:
      - If |obj[j]| > 1e-8: scale = basePerturbScale / |obj[j]|
      - Else: scale = basePerturbScale
      - Clamp maximum: scale <= feasTol * 1e-3

   c. **Generate random perturbations**: epsilonLB, epsilonUB.

   d. **Store perturbations** for later removal.

   e. **Apply to bounds**:
      - If lb[j] > -infinity: lb[j] += epsilonLB
      - If ub[j] < infinity: ub[j] -= epsilonUB

   f. **Handle bound crossing**: If lb[j] > ub[j], use midpoint averaging.

7. **Set perturbation flag**: perturbationApplied = 1.

### 4.3 Pseudocode

```
APPLY_PERTURBATION(state, env):
    IF state.perturbationApplied:
        RETURN 0

    n := state.numVars
    feasTol := env.FeasibilityTol
    infinity := env.CXF_INFINITY

    baseScale := feasTol * 1e-6

    lbPerturb := ENSURE_ALLOCATED(n doubles)
    ubPerturb := ENSURE_ALLOCATED(n doubles)

    seed := HASH_PROBLEM(state)

    FOR j := 0 TO n-1:
        IF lb[j] <= -infinity AND ub[j] >= infinity:
            lbPerturb[j] := 0
            ubPerturb[j] := 0
            CONTINUE

        IF |obj[j]| > 1e-8:
            scale := baseScale / |obj[j]|
        ELSE:
            scale := baseScale

        scale := MIN(scale, feasTol * 1e-3)

        epsLB := RANDOM_UNIFORM(seed) * scale
        epsUB := RANDOM_UNIFORM(seed) * scale

        lbPerturb[j] := epsLB
        ubPerturb[j] := epsUB

        IF lb[j] > -infinity:
            lb[j] := lb[j] + epsLB
        IF ub[j] < infinity:
            ub[j] := ub[j] - epsUB

        IF lb[j] > ub[j]:
            mid := (original_lb + original_ub) / 2
            lb[j] := mid - epsLB/2
            ub[j] := mid + epsUB/2

    state.perturbationApplied := 1
    RETURN 0
```

### 4.4 Mathematical Foundation

**Degeneracy:** A basic solution is degenerate if one or more basic variables have value zero. This can cause cycling.

**Wolfe Perturbation:** Perturb bounds to ensure all basic feasible solutions are non-degenerate.

**Finite Termination:** With perturbation, the algorithm visits at most C(n+m, m) different bases.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) if already applied
- **Average case:** O(n)
- **Worst case:** O(n)

### 5.2 Space Complexity

- O(n) for perturbation storage

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Perturbation array allocation fails | 1001 | Cannot allocate n doubles |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Already applied | perturbationApplied=1 | Return immediately |
| All unbounded | All lb=-inf, ub=inf | No perturbations applied |
| All fixed | All lb=ub | Midpoint averaging used |
| Zero objective | All obj=0 | Use base scale for all |
| Empty problem | numVars=0 | Loop executes zero times |

## 8. Thread Safety

**Thread-safe:** Yes

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_generate_seed | Utilities | Deterministic random seed |
| cxf_random_uniform | Utilities | Uniform random number |
| fabs | C math | Absolute value |
| calloc | C stdlib | Allocate perturbation arrays |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_solve_lp | Simplex | Early in iteration loop |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_unperturb | Removes perturbations (inverse) |
| cxf_simplex_final | Calls unperturb during cleanup |

## 11. Design Notes

### 11.1 Design Rationale

**Proactive application:** Applied in early iterations rather than waiting to detect cycling.

**Variable-dependent scaling:** High-cost variables get smaller perturbations.

**Bound shrinkage:** Lower bounds move up, upper bounds move down (conservative).

## 12. References

- Wolfe, P. (1963). "A technique for resolving degeneracy in linear programming."
- Bland, R.G. (1977). "New finite pivoting rules for the simplex method."

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
