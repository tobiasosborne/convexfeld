# cxf_pricing_init

**Module:** Pricing
**Spec Version:** 1.0

## 1. Purpose

Initializes the pricing subsystem for the simplex algorithm. Allocates data structures for maintaining reduced costs, candidate lists for entering variables, and steepest edge weights if that pricing strategy is selected. Sets up the pricing strategy based on environment parameters and problem characteristics.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| pricingState | PricingState** | Output pointer for allocated state | Non-null | Yes |
| state | SolverState* | Main solver state with dimensions | Non-null, valid state | Yes |
| env | CxfEnv* | Environment with pricing parameters | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory |

### 2.3 Side Effects

- Allocates PricingState structure
- Allocates reduced cost array (if not using state's)
- Allocates candidate list
- Allocates steepest edge weights (if using SE pricing)
- Sets pricing strategy flags

## 3. Contract

### 3.1 Preconditions

- [ ] State has valid dimensions (numVars, numConstrs)
- [ ] Environment has pricing parameters set

### 3.2 Postconditions

- [ ] *pricingState points to initialized structure
- [ ] Candidate list allocated (size based on strategy)
- [ ] Weights array allocated if steepest edge
- [ ] Strategy indicator set

### 3.3 Invariants

- [ ] Main state unchanged

## 4. Algorithm

### 4.1 Overview

Pricing initialization reads the SimplexPricing parameter to determine the strategy, then allocates appropriate data structures. Steepest edge requires additional weight arrays. Partial pricing requires candidate list management.

### 4.2 Detailed Steps

1. **Read pricing strategy**:
   - 0 = automatic selection
   - 1 = partial pricing (Dantzig)
   - 2 = steepest edge
   - 3 = Devex (approximate steepest edge)

2. **Auto-select if needed**:
   - For small problems (n < 1000): full pricing
   - For large sparse: partial pricing
   - For difficult problems: steepest edge

3. **Allocate base structure**:
   - PricingState: ~100 bytes
   - Store strategy, dimensions

4. **Allocate candidate list**:
   - Size based on strategy
   - Partial: ~sqrt(n) candidates
   - Full: n candidates max

5. **Initialize strategy-specific data**:
   - Steepest edge: allocate weights[n], init to 1.0
   - Devex: allocate weights[n], init to 1.0
   - Partial: set up section boundaries

6. **Initialize counters**:
   - candidateCount = 0
   - currentSection = 0
   - updateCount = 0

### 4.3 Pseudocode

```
PRICING_INIT(pricingState, state, env):
    n := state.numVars
    m := state.numConstrs

    // Read strategy
    strategy := GET_PARAM(env, "SimplexPricing")
    IF strategy == 0:  // Auto
        IF n < 1000:
            strategy := 1  // Full/Dantzig
        ELSE IF SPARSITY(state) > 0.95:
            strategy := 1  // Partial
        ELSE:
            strategy := 2  // Steepest edge

    // Allocate structure
    ps := ALLOCATE(sizeof(PricingState))
    ps.strategy := strategy
    ps.numVars := n
    ps.numConstrs := m

    // Candidate list
    IF strategy == 1:  // Partial
        ps.maxCandidates := MAX(100, SQRT(n))
    ELSE:
        ps.maxCandidates := n
    ps.candidates := ALLOCATE(ps.maxCandidates * sizeof(int))
    ps.candidateCount := 0

    // Strategy-specific
    IF strategy == 2 OR strategy == 3:  // SE or Devex
        ps.weights := ALLOCATE(n * sizeof(double))
        FOR i := 0 TO n - 1:
            ps.weights[i] := 1.0
    ELSE:
        ps.weights := NULL

    // Partial pricing sections
    IF strategy == 1:
        ps.numSections := MAX(1, n / ps.maxCandidates)
        ps.currentSection := 0

    *pricingState := ps
    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- O(n) for steepest edge weight initialization
- O(1) for other strategies

### 5.2 Space Complexity

- O(n) for steepest edge weights
- O(sqrt(n)) for partial pricing candidates
- O(1) base structure

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Allocation failure | 1001 | Cannot allocate pricing structures |

### 6.2 Error Behavior

On failure:
- Free any partial allocations
- Set *pricingState = NULL
- Return error code

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Tiny problem | n < 10 | Full pricing always |
| Large problem | n > 100000 | Partial pricing |
| Strategy forced | Explicit parameter | Respect user choice |

## 8. Thread Safety

**Thread-safe:** Yes (allocation only, no shared state)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate structures |
| cxf_get_intparam | Parameters | Read pricing strategy |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_setup | Simplex | During solver setup |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pricing_candidates | Builds candidate list |
| cxf_pricing_steepest | Computes SE prices |
| cxf_pricing_update | Updates after pivot |
| cxf_pricing_free | Frees allocated memory |

## 11. Design Notes

### 11.1 Design Rationale

**Auto-selection:** Different pricing strategies work best for different problems. Auto-selection uses simple heuristics that work well in practice.

**Steepest edge initialization:** Weights start at 1.0, representing unit reference frame. Actual weights computed lazily during first pricing.

### 11.2 Pricing Strategies

- **Dantzig:** Select most negative reduced cost. Simple but can zig-zag.
- **Partial:** Scan subset of variables each iteration. Faster for large problems.
- **Steepest edge:** Accounts for step length. Fewer iterations but more work per iteration.
- **Devex:** Approximates steepest edge. Good balance.

## 12. References

- Harris, P.M.J. (1973). "Pivot Selection Methods of the Devex LP Code"
- Goldfarb, D. and Reid, J.K. (1977). "A Practicable Steepest-Edge Simplex Algorithm"
- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Chapter 10

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
