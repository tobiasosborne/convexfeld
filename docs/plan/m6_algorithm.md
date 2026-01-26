# Milestone 6: Algorithm Layer (Level 4)

**Goal:** Complete Pricing module
**Parallelizable:** All steps can run in parallel
**Spec Reference:** `docs/specs/modules/08_pricing.md`
**Structure Spec:** `docs/specs/structures/pricing_context.md`

---

## Module: Pricing (6 functions)

**Spec Directory:** `docs/specs/functions/pricing/`

### Step 6.1.1: Pricing Tests

**LOC:** ~180
**File:** `tests/unit/test_pricing.c`

Tests for all pricing strategies including steepest edge.

### Step 6.1.2: PricingContext Structure

**LOC:** ~120
**Files:** `include/convexfeld/cxf_pricing.h`, `src/pricing/context.c`
**Spec:** `docs/specs/structures/pricing_context.md`

```c
struct PricingContext {
    int current_level;        /* Active pricing level (0=full) */
    int max_levels;           /* Number of levels (typically 3-5) */
    int *candidate_counts;    /* Candidates at each level */
    int **candidate_arrays;   /* Variable indices per level */
    int *cached_counts;       /* Cached result count (-1=invalid) */
    int last_pivot_iteration;
    int64_t total_candidates_scanned;
    int level_escalations;
};
```

### Step 6.1.3: cxf_pricing_init

**LOC:** ~100
**File:** `src/pricing/init.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_init.md`

### Step 6.1.4: cxf_pricing_candidates

**LOC:** ~120
**File:** `src/pricing/candidates.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_candidates.md`

### Step 6.1.5: cxf_pricing_steepest

**LOC:** ~150
**File:** `src/pricing/steepest.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_steepest.md`

Steepest edge pricing implementation.

### Step 6.1.6: cxf_pricing_update, cxf_pricing_invalidate

**LOC:** ~100
**File:** `src/pricing/update.c`
**Specs:**
- `docs/specs/functions/pricing/cxf_pricing_update.md`
- `docs/specs/functions/pricing/cxf_pricing_invalidate.md`

### Step 6.1.7: cxf_pricing_step2

**LOC:** ~80
**File:** `src/pricing/phase.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_step2.md`
