# cxf_pricing_candidates

**Module:** Pricing
**Spec Version:** 1.0

## 1. Purpose

Builds a list of candidate variables for entering the basis based on reduced cost violations. For partial pricing, scans a section of variables. For full pricing, scans all nonbasic variables. Returns candidates sorted by attractiveness (most negative reduced cost first for minimization).

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| pricingState | PricingState* | Pricing state with strategy | Non-null, initialized | Yes |
| state | SolverState* | Solver state with reduced costs | Non-null, valid state | Yes |
| tolerance | double | Optimality tolerance | > 0, typically 1e-6 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Number of candidates found (>= 0) |

### 2.3 Side Effects

- Updates pricingState.candidates array
- Updates pricingState.candidateCount
- Advances currentSection for partial pricing

## 3. Contract

### 3.1 Preconditions

- [ ] Reduced costs are computed and current
- [ ] Variable status array is valid
- [ ] Pricing state is initialized

### 3.2 Postconditions

- [ ] Candidates array contains variable indices
- [ ] All candidates have attractive reduced costs
- [ ] candidateCount reflects actual count
- [ ] Candidates sorted by attractiveness (optional)

### 3.3 Invariants

- [ ] Reduced costs unchanged
- [ ] Variable status unchanged

## 4. Algorithm

### 4.1 Overview

Scan nonbasic variables in the current pricing section (or all for full pricing). A variable is a candidate if its reduced cost indicates improvement potential:
- For minimization: nonbasic-at-lower with negative RC, or nonbasic-at-upper with positive RC
- Collect candidates up to maximum, keeping most attractive

### 4.2 Detailed Steps

1. **Determine scan range**:
   - Full pricing: 0 to numVars-1
   - Partial pricing: section boundaries

2. **Clear candidate list**:
   - candidateCount = 0

3. **Scan variables**:
   - For each variable j in range:
     - Skip if basic (status >= 0)
     - Get reduced cost d_j
     - Check attractiveness based on status:
       - At lower: attractive if d_j < -tolerance
       - At upper: attractive if d_j > tolerance
       - Free: attractive if |d_j| > tolerance
     - If attractive: add to candidates

4. **Sort candidates** (optional):
   - Sort by |reduced cost| descending
   - Or keep top-k only

5. **Advance section** (partial pricing):
   - currentSection = (currentSection + 1) % numSections

### 4.3 Pseudocode

```
PRICING_CANDIDATES(pricingState, state, tolerance):
    ps := pricingState
    n := state.numVars

    // Determine range
    IF ps.strategy == PARTIAL:
        sectionSize := n / ps.numSections
        startIdx := ps.currentSection * sectionSize
        endIdx := MIN(startIdx + sectionSize, n)
    ELSE:
        startIdx := 0
        endIdx := n

    // Scan for candidates
    ps.candidateCount := 0

    FOR j := startIdx TO endIdx - 1:
        status := state.varStatus[j]

        // Skip basic variables
        IF status >= 0:
            CONTINUE

        rc := state.reducedCosts[j]

        // Check attractiveness
        attractive := FALSE
        IF status == -1:  // At lower bound
            IF rc < -tolerance:
                attractive := TRUE
        ELSE IF status == -2:  // At upper bound
            IF rc > tolerance:
                attractive := TRUE
        ELSE:  // Free variable
            IF |rc| > tolerance:
                attractive := TRUE

        IF attractive:
            IF ps.candidateCount < ps.maxCandidates:
                ps.candidates[ps.candidateCount] := j
                ps.candidateCount++
            ELSE:
                // Replace least attractive if this is better
                MAYBE_REPLACE(ps, j, rc)

    // Sort candidates by attractiveness
    SORT_BY_REDUCED_COST(ps.candidates, ps.candidateCount, state.reducedCosts)

    // Advance section for partial pricing
    IF ps.strategy == PARTIAL:
        ps.currentSection := (ps.currentSection + 1) MOD ps.numSections

    RETURN ps.candidateCount
```

## 5. Complexity

### 5.1 Time Complexity

- **Full pricing:** O(n) to scan all variables
- **Partial pricing:** O(n/k) where k = number of sections
- **With sorting:** Add O(c log c) for c candidates

### 5.2 Space Complexity

- O(1) additional (uses pre-allocated candidate array)

## 6. Error Handling

### 6.1 Error Conditions

None - always succeeds (may return 0 candidates)

### 6.2 Return Value

- Returns count of candidates found
- 0 indicates optimality (no attractive variables)

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Optimal | All RCs satisfy tolerance | Return 0 |
| All attractive | Many violations | Return up to maxCandidates |
| Empty section | Partial pricing gap | Return 0, advance section |
| One candidate | Single violation | Return 1 |

## 8. Thread Safety

**Thread-safe:** No (modifies pricing state)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| qsort | C stdlib | Sort candidates |
| fabs | C math | Absolute value |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Each iteration |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pricing_init | Allocates candidate array |
| cxf_pricing_steepest | Alternative: SE pricing |
| cxf_pricing_update | Updates RCs after pivot |

## 11. Design Notes

### 11.1 Design Rationale

**Section cycling:** Partial pricing cycles through sections to ensure all variables eventually get checked.

**Candidate limit:** Keeping limited candidates reduces memory and sorting overhead while capturing best options.

**Sort by attractiveness:** Trying best candidates first often finds good entering variable quickly.

### 11.2 Performance Considerations

- Full scan is O(n) but with very small constant
- Partial pricing effective for n > 10000
- Consider SIMD for reduced cost comparisons

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Chapter 10
- Dantzig, G.B. (1963). "Linear Programming and Extensions"

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
