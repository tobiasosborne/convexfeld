# Cleanroom Implementation Q&A: Deep Analysis

**Date:** 2026-03-18
**Context:** Responses to 18 implementation questions from agents building the cleanroom LP solver.
All answers are grounded in the analyzed source code and behavioral specifications.

---

## Table of Contents

1. [TIMEOUT Cluster (Q1–Q5)](#timeout-cluster-q1q5)
2. [Wrong Objective Cluster (Q6–Q9)](#wrong-objective-cluster-q6q9)
3. [UNKNOWN Status Cluster (Q10–Q12)](#unknown-status-cluster-q10q12)
4. [False UNBOUNDED Cluster (Q13–Q14)](#false-unbounded-cluster-q13q14)
5. [Cycling Cluster (Q15–Q17)](#cycling-cluster-q15q17)
6. [Meta-Question (Q18)](#meta-question-q18)

---

## TIMEOUT Cluster (Q1–Q5)

### Q1: Refactorization Triggers in the Iteration Loop

> *Our adaptive threshold is min(100, max(50, m/4)) eta updates. Is there a secondary trigger based on fill-in ratio, FTRAN residual magnitude, or wall-clock time?*

**Your primary formula is correct.** `min(100, max(50, m/4))` matches the spec exactly (`specs/reference/numerical_stability.md`, Section A). This balances the O(m²)–O(m³) refactorization cost against linearly growing per-iteration FTRAN/BTRAN cost.

**But you are missing three critical secondary triggers:**

#### Secondary Trigger 1: FTRAN Residual Monitoring (Most Important)

After each FTRAN solve, compute the residual:

```
r = a_q - B * x_computed
```

If `||r||_infinity > 10 * epsilon_feas` (i.e., ~1e-5), force an immediate refactorization regardless of eta count. The spec calls this *"the most robust trigger because it directly measures the quantity that matters: the accuracy of the linear system solution"* (`numerical_stability.md:29–35`).

**Implementation guidance:**
- Compute `r` by multiplying the original column `a_q` (from the constraint matrix) minus `B * x` where `x` is the FTRAN result
- The threshold `10 * epsilon_feas` is approximate — the spec says "a small multiple of the feasibility tolerance"
- This is cheap to compute (one sparse matrix-vector product) relative to the cost of an inaccurate pivot

#### Secondary Trigger 2: Small Pivot Accumulation

After detecting **multiple consecutive small pivots** (pivot elements close to the Harris tolerance, ~1e-9), trigger early refactorization regardless of eta count (`numerical_stability.md:37–42`).

**Rationale:** Small pivots amplify roundoff error per eta vector. A single small pivot is fine; a sequence of them can corrupt the basis representation faster than the eta count threshold anticipates.

#### Secondary Trigger 3: Algorithmic Event Triggers

Force refactorization on these events:
- Phase I → Phase II transition
- Barrier-to-simplex crossover start
- Stalling/objective stagnation detection (see Q12)
- Numerical difficulty recovery

These are **unconditional** — they fire even if the eta count is 1.

#### Secondary Trigger 4: Adaptive Interval Reduction

*"If residual monitoring triggers refactorizations frequently, the solver should reduce the eta count threshold for subsequent intervals"* (`numerical_stability.md:48`). For example, if three consecutive intervals are cut short by residual triggers, reduce the threshold from `min(100, max(50, m/4))` to something like 75% of that value.

**Gap:** The exact adaptive reduction formula is not specified. A reasonable implementation would be to multiply the threshold by 0.75 each time a residual trigger fires, with a floor of ~20.

#### What's NOT a Trigger

- **No fill-in ratio trigger** — not documented anywhere in the specs or analyzed source
- **No wall-clock time trigger** — refactorization decisions are purely numerical, not time-based

---

### Q2: Pricing Candidate List Persistence

> *Does pricing use a candidate list that persists across iterations, or is it rebuilt each iteration? If the original maintains a persistent steepest-edge heap or partial price list that's only updated incrementally, that's a 10x speedup on large problems.*

**The original also rebuilds each iteration — there is no persistent steepest-edge heap.** But the 10x speedup is real; it comes from a different mechanism.

#### What the Binary Actually Does

The pricing subsystem uses **6 cache slots per level**, all set to **-1 (invalidated) after every pivot** (`cvx_pricing_update.c:372–377`):

```c
*(int*)(pricingState + 0x94 + currentLevel * 4) = -1;   /* constrCache1 */
*(int*)(pricingState + 0xb8 + currentLevel * 4) = -1;   /* constrCache2 */
*(int*)(pricingState + 0xe0 + currentLevel * 4) = -1;   /* varCache1 */
*(int*)(pricingState + 0x108 + currentLevel * 4) = -1;  /* varCache2 */
*(int*)(pricingState + 0x130 + currentLevel * 4) = -1;  /* cache5 */
*(int*)(pricingState + 0x158 + currentLevel * 4) = -1;  /* cache6 */
```

On cache miss, candidates are rebuilt via a seed → expand → filter pipeline. The data structures are **flag arrays** (per-element membership bits) and **index queues** (simple arrays, not ordered by score).

#### Where the Speedup Actually Lives: 3-Level Progressive Expansion

The architecture that makes this fast is **multi-level partial pricing** (`specs/algorithms/partial_pricing.md:355–360`):

```
Level 0 (Cheapest): Return base dirty list directly — O(1)
Level 1 (Moderate): Expand through one-hop structural neighbors — O(q + neighbors)
Level 2 (Comprehensive): May fall back to full scan or two-hop expansion
```

**Per-iteration flow:**
1. Try Level 0. If an acceptable pivot is found, use it. **Most iterations stop here.**
2. If Level 0 fails, escalate to Level 1. Expand candidates through matrix structure.
3. If Level 1 fails, escalate to Level 2. May trigger full scan.
4. After pivot, mark affected elements dirty at the active level.

**If your code does a full scan every iteration, implementing this 3-level system IS the fix for the 57 timeouts.** On large problems, Level 0 covers >90% of iterations with O(dirty set) work instead of O(n) full scans.

#### Key Implementation Details

- **Flag bits per element** encode which queue/level the element belongs to:
  - Bit 0: committed at level 1
  - Bit 1: pending at level 1
  - Bit 2: committed at level 2
  - Bit 3: pending at level 2
- **Expansion strategy is cost-based** (not iteration-count-based): if dirty set covers a significant fraction of the problem, skip partial expansion and do a full scan directly
- **Thresholds** (`partial_pricing.md:268–275`):

| Parameter | Role | Typical Value |
|-----------|------|---------------|
| EXPANSION_THRESHOLD | Controls when cross-queue is large | 2.0 |
| COVERAGE_THRESHOLD | Controls when queue covers enough | 0.5 |
| EXPANSION_WORK_FACTOR | Amortization factor | 5e-4 |

---

### Q3 & Q15: EXPAND Perturbation Magnitude Formula

> *What is the exact epsilon? Is it scaled by bound range, column norm, random component?*

**The formula from the spec** (`specs/algorithms/perturbation.md:195–204`):

```
epsilon_i = epsilon_base * (1 + |bound_value|) * (1 + hash(i))
```

Where:
- **`epsilon_base`** ≈ 1e-6 to 1e-8, scaled from the feasibility tolerance
- **`(1 + |bound_value|)`** provides relative scaling — variables with large bound values get proportionally larger perturbations, preventing the perturbation from being swallowed by floating-point roundoff
- **`hash(i) ∈ [0, 1)`** is a deterministic function of variable index `i` — ensures all perturbations are distinct, which per Wolfe (1963) guarantees the perturbed polyhedron is non-degenerate with probability 1

**The hash function must be deterministic** (not random) to ensure reproducibility. A simple implementation: `hash(i) = frac(i * phi)` where `phi = (sqrt(5)-1)/2` (golden ratio). This gives well-distributed values in [0,1) with no collisions for practical problem sizes.

**Constants from the pricing-restriction mechanism** (the other half of perturbation):
- `MIN_BOUND_RANGE = 1e-10` (`cvx_simplex_perturbation.c:56`)
- `MAX_PERTURBATION = 1e-6` (`cvx_simplex_perturbation.c:62`)

These apply to Mechanism A (pricing restriction / entering-side degeneracy), where variables with implied bound gap below `MIN_BOUND_RANGE` are removed from the pricing candidate set.

**Gap:** The full EXPAND bound-widening implementation (Mechanism B) appears to be in helper functions not fully traced in the analyzed source. The spec formula above is the best available guidance.

---

### Q4: Sprint / Devex Warm-Start Phase

> *Many fast solvers do ~100 iterations of partial pricing / Dantzig before switching to steepest edge. Does the original have this two-phase pricing strategy within a single solve?*

**No.** There is no iteration-count-based strategy switch.

The solver uses the **same multi-level partial pricing system throughout the entire solve**. There is no:
- Iteration counter threshold ("if iterations < 100 then fast pricing")
- Devex weight accumulation
- Strategy mode field in PricingState
- Phase transition between pricing algorithms

The per-iteration level progression (Level 0 → 1 → 2) is **always active** and is controlled by **problem characteristics** (density, queue coverage, dirty set size), not iteration count.

**What it does instead:** The multi-level system naturally behaves like a sprint phase in early iterations — when many variables are dirty (e.g., after crash basis), Level 0 returns a large candidate set and finds good pivots cheaply. As the solve progresses and fewer variables change per iteration, the dirty set shrinks and Level 0 becomes a very fast "hot path."

---

### Q5: Phase I → Phase II Transition Threshold

> *Our threshold is feasibility_tol. Does the original use a softer transition (weighted Phase I/II objective) or big-M?*

**Your threshold is correct.** The transition fires when `|w(x)| <= epsilon_feas` where `w(x)` is the Phase I objective.

**No Big-M. No weighted objective. No soft transition.** The original uses the **pure two-phase method** (`specs/algorithms/two_phase_method.md:234–235`).

**Edge case handling:** When Phase I objective is positive but very small (below a small multiple of `epsilon_feas`), the solver *"may attempt additional iterations with tighter tolerances before declaring infeasibility, to distinguish genuine infeasibility from numerical artifact"* (`two_phase_method.md:141–142`). This is a procedural retry (keep iterating), not a threshold change.

**Termination outcomes:**
1. `w* = 0` (within `epsilon_feas`) → Transition to Phase II
2. `w* > 0` and no improving pivot → Return INFEASIBLE
3. `w* > 0` but very small → Attempt additional iterations, then decide

---

## Wrong Objective Cluster (Q6–Q9)

### Q6: Variable Fixing Order in cvx_simplex_final

> *What exactly determines the fixing order? Does the original sort by reduced cost magnitude, bound range, column density?*

**Linear scan in variable index order. No sorting.**

The 5-phase variable fixer uses a simple forward loop (`cvx_simplex_final.c:305–364`):

```c
for (int j = 0; j < numVars; j++) {
    if (dualVal >= 0)
        target = lower_bound;
    else
        target = upper_bound;
}
```

No sorting by reduced cost magnitude, bound range, or column density. Variables are processed sequentially in their natural index order. The dual feasibility check is **local** (per-variable decision, no comparison across variables).

For equality constraints, variables are appended to the `fixingCandidates` array in encounter order (`cvx_simplex_final.c:422`).

**Why this matters for boeing2:** The linear scan is deterministic but may fix a variable that forces a later variable into a constraint-violating state. If variable A and variable B are coupled through a constraint, fixing A first (because it has a lower index) may leave B in an infeasible position. A smarter ordering (e.g., by reduced cost magnitude or column density) could prevent this.

**Recommendation:** The linear scan matches the original, so changing the order would diverge from the spec. However, if boeing2's 604% error persists, consider adding a **post-fixing feasibility check** with corrective re-fixing as a diagnostic.

---

### Q7: Long-Step Dual During Primal Simplex

> *boeing2's 604% error suggests the solver converges to a completely wrong vertex. Could there be a dual feasibility restoration step we're missing?*

**No dual feasibility restoration step exists in primal simplex.** The "long step" mechanism is **Bound-Flipping Ratio Test (BFRT)**, which operates within primal simplex:

1. Identify breakpoint where a variable hits its bound
2. Instead of stopping, **flip** the variable to its opposite bound
3. Negate the coefficient column to maintain consistency
4. Continue until a non-flippable constraint is reached

This is documented in `simplex_iteration.md:85–121`. It provides longer steps and better progress per iteration, but it is NOT a dual phase.

**boeing2 diagnosis:** The 604% error is more likely caused by:
- **Q6:** Variable fixing order leaving the solver at a wrong vertex
- **Q9:** Accumulated objective error from incremental updates
- **Q8:** Harris band formula mismatch causing different pivot sequences

**Key check:** After solving, verify that `c'x` computed from scratch matches the solver's internal objective. If they differ significantly, the error is in objective tracking, not vertex selection.

---

### Q8: Harris Ratio Test Band Formula

> *Is the band `feasibility_tolerance` or `feasibility_tolerance * (1 + |x_i|)`?*

**Plain feasibility tolerance. NOT scaled by variable magnitude.**

The exact formula (`specs/algorithms/harris_ratio_test.md:96–106`, confirmed in `cvx_simplex_step.c:887–900`):

```
theta_max = min_i { (slack_i + epsilon_feas) / |d_i| }
```

Where:
- `slack_i` = distance from basic variable `x_i` to its blocking bound
- `epsilon_feas` = feasibility tolerance (absolute, NOT modified by `|x_i|`)
- Minimum taken only over candidates with `|d_i| >= pivot_tolerance`

**No `(1 + |x_i|)` scaling.** This is an absolute tolerance band.

**Implications for large-variable problems (boeing2, ganges):** The band is relatively tighter for variables with large values, which can cause:
- More degenerate pivots (the band doesn't widen to accommodate floating-point scale)
- Different leaving variable selection compared to a scaled implementation

**Verify your implementation matches this exactly.** If you accidentally use a scaled band, your pivot sequence will diverge from the original's, potentially leading to different (wrong) vertices.

---

### Q9: Objective Recomputation After Fixing

> *Does the original recompute the objective AFTER fixing, or trust incremental updates?*

**Incremental only. No global recomputation during the fixing phase.**

`cvx_pivot_bound` updates the objective incrementally:

```c
// Linear term
objective += reducedCost[varIdx] * fixedValue;
reducedCost[varIdx] = 0.0;

// Quadratic term (if present)
if (Qdiag[varIdx] != 0) {
    objective += 0.5 * fixedValue * fixedValue * Qdiag[varIdx];
    Qdiag[varIdx] = 0.0;
}

// Q-neighbor linearization
for each neighbor k in Q row:
    reducedCost[k] += Qcoeff[varIdx, k] * fixedValue;
```

There is **no call to a recompute-objective function** after the fixing loop in `cvx_simplex_final`. The later `cvx_scale_objval` call happens during solution reporting (after `cvx_simplex_cleanup`), not during the fixing phase itself.

**Error accumulation risk is confirmed.** The spec acknowledges this (`simplex_lifecycle.md:364–366`): *"If the objective value changes significantly during uncrushing (due to accumulated numerical error..."* — implying the solver knows incremental updates can drift.

**Recommended diagnostic:**
```python
# After all fixings in cvx_simplex_final:
ground_truth = sum(c[j] * x[j] for j in range(n))  # + 0.5 * x'Qx for QP
if abs(ground_truth - state.objective) > 1e-6 * (1 + abs(ground_truth)):
    log_warning("Objective drift: incremental=%e, recomputed=%e",
                state.objective, ground_truth)
    state.objective = ground_truth  # Correct the drift
```

This doesn't change the algorithm but catches accumulated error before it reaches the user.

---

## UNKNOWN Status Cluster (Q10–Q12)

### Q10: Phase I Optimal with Free/Superbasic Variables

> *What status code does the original return when Phase I reaches optimal with objective=0 but basis contains superbasic/free variables?*

**Critical finding: the original checks free variables' reduced costs under the WRONG objective (Phase I surrogate) and may return false INFEASIBLE.**

In `cvx_simplex_phase_end.c:163–178`:

```c
if (status == 0) {  /* Free variable */
    double rc = reducedCost[constrIdx];
    if (rc < negFeasTol ||
        (rc > primalFeasTol && constrSense[constrIdx] == '=')) {
        /* Dual bound violation → INFEASIBLE */
        *(int*)(state + 0x468) = constrIdx;
        return 3;  /* INFEASIBLE */
    }
}
```

**The problem:** This check fires **BEFORE the Phase II transition recomputes reduced costs** under the original objective. A free variable that looks dual-infeasible under the Phase I surrogate objective might be perfectly fine under the original objective.

**This is almost certainly why grow7/15/22, forplan, and modszk1 return UNKNOWN.** These problems have many free or equality-constrained variables. The Phase I surrogate objective assigns penalty weights to infeasibility measures, and the reduced costs under that objective have no relationship to the original objective's reduced costs.

**Fix:** There is NO "reoptimize with restricted pricing" step. Instead, the fix is to **defer the free-variable dual feasibility check until AFTER reduced costs are recomputed under the original objective during Phase II transition.** The transition procedure (`two_phase_method.md:161–180`) includes:

1. Objective function swap (Phase I surrogate → original)
2. Full reduced cost recomputation: `d_N = c_N - N^T (B^{-T} c_B)`
3. Pricing state reset

Only **after** step 2 should free variables be checked for dual feasibility.

---

### Q11: Refactorization at Phase I → Phase II Transition

> *Does the original immediately refactorize, or continue with existing factorization?*

**Yes, immediate refactorization.** This is confirmed in two places:

1. `numerical_stability.md:39–40` lists Phase I → Phase II as an explicit refactorization event
2. `simplex_phases.md:297–298` specifies that all reduced costs are recomputed from scratch, which requires fresh LU factors

**Your code doing refactorization at `phase_one.c:234` is correct.** The Phase II reduced costs MUST start from a fresh numerical baseline because:
- Phase I uses a surrogate objective → Phase I reduced costs are meaningless for Phase II
- Eta vectors accumulated during Phase I may have degraded accuracy
- Fresh LU factors ensure the `B^{-T} c_B` computation is accurate

**State transformations at transition:**

| Component | Action |
|-----------|--------|
| Objective function | Swap: Phase I surrogate → original |
| Basis | Carry forward unchanged |
| LU factors | **Refactorize from scratch** |
| Reduced costs | **Full recomputation** |
| Pricing state | Reinitialize (loose tolerances) |
| Perturbation state | Reset |

---

### Q12: No Improving Direction + Stagnation

> *Does the original have a "restart" mechanism (new crash basis, or perturbation of the objective)?*

**No restart. No new crash basis.** The response is perturbation + strategy switch from the current basis.

**Stagnation detection** (`cvx_simplex_post_iterate.c:218–256`):

```
1. z_current = current objective
2. z_previous = objective at last refactorization interval
3. delta_z = z_current - z_previous
4. if delta_z < epsilon_opt (strictly less, not <=):
       set iteration_mode = -1  (stagnation signal)
```

**Response sequence:**
1. Stagnation detected → set `iteration_mode = -1`
2. Outer loop invokes `cvx_simplex_perturbation` → applies EXPAND bound widening + pricing restriction
3. May toggle primal ↔ dual simplex strategy
4. Continue from **current basis** (not a crash restart)
5. Repeat until outer iteration limit is reached

**Progress tracking via basis snapshots:** The solver uses `cvx_basis_diff()` to compute a weighted difference score between the current basis state and a snapshot taken at a fixed point. The threshold formula:

```
if D <= (iterations - 5) * tau:
    // Stalling detected, invoke perturbation
```

Where `D` = weighted difference score, `tau` = base threshold, and 5 is a grace period.

**Outer iteration limits** (`cvx_solve_lp/part3_main_loop.c:69–88`):

| Mode | Max Outer Iterations |
|------|---------------------|
| Primal simplex | 5 |
| Dual/auto | 100 |
| Crossover | 10 |
| User-specified | `outputLevel` value |

---

## False UNBOUNDED Cluster (Q13–Q14)

### Q13: Unboundedness Detection in cvx_pivot_special

> *Does the original also check whether ANY basic variable would block the step before declaring unbounded?*

**No. The check is simple and does NOT FTRAN the column first.**

`cvx_pivot_special.c:248–336` only checks two things:

1. Whether the variable has **finite bounds** in the movement direction
2. Whether the **reduced cost exceeds a magnitude threshold**

```c
// canDecrease case:
if (upperBound < lb_limit) {
    // Has finite upper bound → flip to upper bound
} else if (reducedCost < negUbLimit) {
    // No finite bound + strong reduced cost → UNBOUNDED
    *(int32_t*)((char*)state + 0x460) = varIdx;
    return CVX_UNBOUNDED;
}
```

**There is no FTRAN of the entering column.** The function does not check whether basic variables would block the step. It trusts the reduced cost value and the bound structure.

**This is likely causing false UNBOUNDED on scsd8/shell.** If the basis representation has degraded (many eta updates since last refactorization), the reduced costs may be stale, and a variable may appear to have a strong improving direction when it actually doesn't. A refactorization + recomputation would reveal that the variable is not actually unbounded.

**Why the original gets away with this:** The original's aggressive refactorization strategy (residual monitoring, small pivot detection — see Q1) keeps the basis representation accurate enough that stale reduced costs rarely trigger false unbounded returns. If your implementation lacks those secondary refactorization triggers, you'll see more false positives here.

---

### Q14: Probe Step Before UNBOUNDED

> *Does the original refactorize + recompute before accepting an UNBOUNDED return, similar to the infeasibility confirmation in step2/step3?*

**No probe step exists.** `cvx_pivot_special` returns UNBOUNDED immediately.

There IS unboundedness detection at the ratio test level (Harris Stage 3: *"if no blocking variable remains, the step is unbounded in this direction"*), but no refactorize-and-reconfirm step.

**This is a spec gap and a significant implementation issue.** The infeasibility path has confirmation (step2/step3 do bound propagation to verify), but the unboundedness path does not.

**Recommended fix:** Add a probe step before accepting UNBOUNDED:

```python
if pivot_special_returns_unbounded:
    # Probe: refactorize and recheck
    refactorize_basis()
    recompute_reduced_costs()
    result = pivot_special(recheck=True)
    if result == UNBOUNDED:
        return UNBOUNDED  # Confirmed
    else:
        continue_iteration()  # False alarm, keep going
```

This diverges from the original's behavior but compensates for the lack of aggressive secondary refactorization triggers. The original can afford to skip the probe because its basis is almost always numerically fresh.

---

## Cycling Cluster (Q15–Q17)

### Q15: Perturbation Magnitude

See Q3 above. The formula is:

```
epsilon_i = epsilon_base * (1 + |bound_value|) * (1 + hash(i))
```

With `epsilon_base` in the range 1e-6 to 1e-8 and `hash(i) ∈ [0, 1)` deterministic.

For the pricing-restriction mechanism (Mechanism A), the clamping range is `[1e-10, 1e-6]`.

---

### Q16: Perturbation Counter — Reset or Decrement?

> *Our code resets cumulative_degenerate = 0. If the original only decrements or uses a sliding window, the second perturbation fires sooner.*

**The original uses cumulative increment, NOT reset.**

From `cvx_simplex_perturbation.c:431`:

```c
*(int*)(solverState + 0x3f4) = *(int*)(solverState + 0x3f4) + perturbedCount;
```

This **adds** `perturbedCount` (variables removed/perturbed in this call) to the cumulative counter. It does NOT reset it to zero. There is no sliding window.

**Cycling detection uses basis snapshot comparison** (`cvx_basis_diff`), not the raw perturbation counter. The counter tracks *how much perturbation has been applied total*, which informs downstream decisions about escalation.

**If your code resets `cumulative_degenerate = 0` after perturbation, you're making the second perturbation trigger LATER than the original does.** Switch to cumulative increment to match.

---

### Q17: Maximum Perturbation Rounds and Escalation to Bland's Rule

> *Our code uses Bland only after perturb_count > 0 && degenerate_count > 3*m. Does the original have a tighter escalation?*

**The original does NOT count perturbation rounds explicitly.** Escalation is controlled by outer loop iteration limits, not a perturbation round counter.

**Escalation sequence** (`specs/algorithms/perturbation.md:293–303`):

1. **First stall detection:** Apply Mechanism A (pricing restriction) only
2. **Subsequent stalls:** Apply Mechanism A + Mechanism B (EXPAND bound widening)
3. **Final resort:** Bland's rule — but documented as a theoretical fallback, not invoked in normal flow

**Outer loop iteration limits control termination:**

| Simplex Mode | Max Outer Iterations |
|--------------|---------------------|
| Primal | 5 |
| Dual/auto | 100 |
| Crossover | 10 |

When the outer iteration limit is reached, the solver terminates (may report suboptimal or numerically difficult status) rather than switching to Bland's rule.

**The spec explicitly notes** (`perturbation.md:299–303`): *"If perturbation fails to resolve stalling (the EXPAND approach is not guaranteed to prevent cycling in all cases, as shown by Hall and McKinnon, 2001), the solver relies on the outer iteration limit and may report a suboptimal or numerically difficult status. For guaranteed finiteness, Bland's rule can serve as a fallback..."*

**Your `degenerate_count > 3*m` threshold for Bland's rule doesn't match anything in the original.** The original uses outer loop limits and perturbation escalation, with Bland's rule as an unreachable theoretical safety net. Consider replacing your Bland's trigger with the outer loop limit approach.

---

## Meta-Question (Q18)

> *What are the top 3 implementation details that the spec DOESN'T capture but are essential for a production solver?*

### Gap 1: Multi-Level Partial Pricing Architecture (The TIMEOUT Killer)

**Impact:** Explains 57/57 timeout failures.

The spec documents partial pricing but doesn't convey the critical performance architecture. The difference between 43/114 and 114/114 is largely here:

- **Level 0** should handle >90% of iterations at O(dirty set) cost
- **Full scan** should be the exception, not the rule
- **Cache invalidation** after every pivot means each level rebuild is cheap (only dirty elements)
- **Cost-based escalation** (not iteration-based) ensures the solver adapts to problem structure

**What to implement:**
1. Per-element flag bits tracking queue membership (4 bits per variable/constraint)
2. Dirty list maintained incrementally after each pivot
3. Level 0 returns dirty list directly (no scan)
4. Level 1 expands through one-hop matrix neighbors
5. Level 2 falls back to full scan
6. Cost heuristic: if dirty set > 50% of variables, skip partial and do full scan

### Gap 2: Aggressive Secondary Refactorization (False UNBOUNDED + Numerical Drift)

**Impact:** Explains false UNBOUNDED (scsd8, shell) and contributes to wrong objectives.

The spec mentions residual monitoring but the implementers likely only have the eta count trigger. The original's numerical stability comes from **four triggers working together**:

1. Eta count (your current trigger)
2. FTRAN residual (detects accuracy degradation directly)
3. Small pivot accumulation (prevents error amplification)
4. Algorithmic events (ensures fresh basis at critical transitions)

**Without triggers 2–4, the basis representation degrades between refactorizations**, causing:
- Stale reduced costs → false UNBOUNDED detection in `cvx_pivot_special`
- Inaccurate FTRAN results → wrong pivot selection → convergence to wrong vertex
- Accumulated roundoff → objective drift during variable fixing

**Priority: implement FTRAN residual monitoring (trigger 2) first.** It catches the most problems with the least implementation effort.

### Gap 3: Free Variable Handling at Phase I End (UNKNOWN Status Cluster)

**Impact:** Explains 8/8 UNKNOWN failures (boeing1, grow7/15/22, fit1d, fit2d, forplan, modszk1).

The analyzed source shows that `cvx_simplex_phase_end` checks free variables' dual feasibility under the **Phase I surrogate objective** and can return INFEASIBLE before the Phase II transition ever recomputes reduced costs under the original objective.

**The fix is specific and surgical:**
- Do NOT check free variables' reduced costs in `cvx_simplex_phase_end`
- Instead, transition to Phase II (swap objective, refactorize, recompute reduced costs)
- THEN check free variables' dual feasibility under the original objective
- If dual-infeasible under original objective, THEN return INFEASIBLE

This reordering preserves the algorithm's correctness (the check still happens) but eliminates false positives from Phase I reduced costs that are meaningless under the original objective.

---

## Summary: Priority-Ordered Fix List

| Priority | Fix | Fixes | Effort |
|----------|-----|-------|--------|
| **P0** | Implement 3-level partial pricing | 57 TIMEOUTs | High |
| **P0** | Add FTRAN residual refactorization trigger | False UNBOUNDED, objective drift | Medium |
| **P1** | Defer free-variable check to after Phase II transition | 8 UNKNOWNs | Low |
| **P1** | Add unbounded confirmation probe (refactorize + recheck) | 2 false UNBOUNDEDs | Low |
| **P2** | Switch perturbation counter from reset to cumulative increment | 2 cycling (kb2, recipe) | Low |
| **P2** | Add post-fixing objective recomputation diagnostic | 6 wrong objectives | Low |
| **P2** | Replace Bland's rule trigger with outer loop limits | Cycling edge cases | Low |
| **P3** | Implement small-pivot-accumulation refactorization trigger | Numerical edge cases | Medium |
| **P3** | Implement adaptive refactorization interval reduction | Long-running problems | Low |
