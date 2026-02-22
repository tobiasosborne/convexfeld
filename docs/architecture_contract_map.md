# Component Interface Contract Map

*Purpose: Multi-scale diagnostic reference for identifying subtle architectural bugs and v2 spec violations.*

---

## SCALE 1 — Macro Flow (Solve Pipeline)

```
presolve ──► crash ──► phase_one_setup ──► MAIN LOOP (Phase I) ──► transition ──► MAIN LOOP (Phase II) ──► postsolve
   │            │            │                    │                     │                   │                  │
   │            │            │                    ▼                     │                   ▼                  │
   │            │            │            ┌──────────────┐             │           ┌──────────────┐           │
   │            │            │            │  step.c      │             │           │  step.c      │           │
   │            │            │            │  (iteration) │             │           │  (iteration) │           │
   │            │            │            └──────────────┘             │           └──────────────┘           │
   │            │            │                    │                     │                   │                  │
   ▼            ▼            ▼                    ▼                     ▼                   ▼                  ▼
 ASSUMES:    ASSUMES:     ASSUMES:          ASSUMES:              ASSUMES:           ASSUMES:           ASSUMES:
 model       CSR matrix   crash results     phase=1               all artificials    phase=2            solution in
 valid       available    in row_status     work_obj=artificial   at zero            work_obj=original  work_x[]
                                            diag_coeff set        LU valid           reduced costs      saved_lb/ub
 PRODUCES:   PRODUCES:    PRODUCES:                                                  recomputed         available
 obvious     row_status   artificials in    PRODUCES:             PRODUCES:
 infeas/     col_nz_cnt   basis             leaving/entering      phase=2            PRODUCES:          PRODUCES:
 unbound     num_basic    work_obj=surrog   basis updates         fresh LU           OPTIMAL/           original
 detected                 phase=1           obj_value updated     pricing reset      UNBOUNDED/         bounds
                          obj_value=sum_a                         work_obj=original  ITER_LIMIT         restored
```

### Macro Interface Contracts

| Edge | Producer | Consumer | Data Passed | Sign/Index Convention |
|------|----------|----------|-------------|----------------------|
| presolve→crash | solve_lp.c | crash.c | CSR matrix, row_status[m]=0 | Row indices 0..m-1 |
| crash→phase_one | crash.c | phase_one.c | row_status[m] (BASIC_LOWER/UPPER/UNASSIGNED) | Crash marks rows, Phase I reads them |
| phase_one→main_loop | phase_one.c | step.c | diag_coeff[m], basic_vars[m], var_status[n+m], work_obj[n+m] | diag_coeff: +1 for <=, -1 for >= |
| main_loop→transition | phase_loop.c | phase_one.c | Phase I obj <= tol | Sum of artificial values |
| transition→main_loop | phase_one.c | step.c | Fresh LU, original obj, phase=2, pricing reset | Reduced costs recomputed |
| main_loop→postsolve | step.c | cleanup.c | work_x[n+m], work_lb/ub[n+m], saved_lb/ub[n] | Nonbasic at bounds, basic within bounds |

---

## SCALE 2 — Iteration Anatomy (One Step)

```
            ┌─────────────────────────────────────────────────────────────┐
            │                     cxf_simplex_step()                     │
            │                                                             │
            │  ┌──────────┐    ┌──────┐    ┌───────────┐    ┌────────┐  │
            │  │ PRICING  │───►│ FTRAN│───►│ RATIO TEST│───►│ PIVOT  │  │
            │  │          │    │      │    │           │    │        │  │
            │  │candidates│    │B⁻¹·Aj│    │leaving_row│    │basis   │  │
            │  │entering_j│    │      │    │pivot_elem │    │exchange│  │
            │  └──────────┘    └──────┘    └───────────┘    └────────┘  │
            │       │                                            │       │
            │       │         ┌──────────────────────────────────┘       │
            │       │         │                                          │
            │       │    ┌────┴───┐    ┌──────────┐    ┌────────────┐   │
            │       │    │ BTRAN  │───►│ RC UPDATE│───►│  PRICING   │   │
            │       │    │        │    │          │    │  CASCADE   │   │
            │       │    │B⁻ᵀ·eᵣ │    │ dj[j] ∀j │    │  dirty     │   │
            │       │    └────────┘    └──────────┘    └────────────┘   │
            │       │                                        │          │
            │       │    ┌──────────┐    ┌──────────┐        │          │
            │       └───►│  BFRT    │───►│ PERTURB  │        │          │
            │            │(optional)│    │(optional)│        │          │
            │            │flip vars │    │anti-cycle│        │          │
            │            └──────────┘    └──────────┘        │          │
            │                                                │          │
            │  ┌──────────────┐                              │          │
            │  │REFACTOR CHECK│◄─────────────────────────────┘          │
            │  │  (periodic)  │                                         │
            │  └──────────────┘                                         │
            └───────────────────────────────────────────────────────────┘
```

### Iteration Interface Contracts

| Edge | Producer → Consumer | Data on Edge | Contract |
|------|---------------------|-------------|----------|
| **PRICING → FTRAN** | candidates → extract_column | `entering_var` (0..n+m-1) | If j < n: extract CSC column. If j >= n: use diag_coeff[j-n] as singleton column |
| **FTRAN → RATIO TEST** | ftran → ratio_test | `pivotCol[m]` = B⁻¹·A_entering | Dense m-vector. Sign carries from matrix coefficients and LU factors |
| **RATIO TEST → PIVOT** | ratio_test → apply_pivot | `leavingRow` (0..m-1), `pivotElement` | pivotElement = pivotCol[leavingRow]. Must be nonzero (|piv| > PIVOT_TOL) |
| **PIVOT → BTRAN** | apply_pivot → btran | `leavingRow` (0..m-1) | Unit vector e_r passed to BTRAN. Eta vector appended to chain |
| **BTRAN → RC UPDATE** | btran → update_reduced_costs | `rho[m]` = B⁻ᵀ·e_r | Used for incremental dj update: dj[j] -= (rho·A_j / pivotElem) * dj[entering] |
| **RC UPDATE → PRICING** | reduced_costs → pricing_cascade | `work_dj[n+m]` updated | Pricing queues marked dirty for entering col and leaving row |
| **BFRT → MATRIX** | flip logic → CSR/CSC | Row negation for flipped vars | **MUST negate working copy, NOT model matrix** |

---

## SCALE 3 — Interface Contracts (Mismatch Detection)

### CONTRACT 1: Entering Direction Sign `s`

**V2 Spec says:** `s = +1` if AT_LOWER (variable increases), `s = -1` if AT_UPPER (variable decreases)

| Component | What it does | Status |
|-----------|-------------|--------|
| **step.c** (producer) | `entering_sign = (var_status == AT_UPPER) ? -1 : +1` | OK |
| **ratio_test.c** (consumer) | `s = (var_status == AT_UPPER) ? -1 : +1` (reads independently) | OK |
| **step.c** obj update | `obj_value += entering_sign * dj[entering] * stepSize` | OK |

**Verdict: CONSISTENT** — Both producer and consumer derive `s` independently from var_status. Convention matches v2 spec.

---

### CONTRACT 2: diag_coeff Sign for >= Constraints

**V2 Spec says:** `diag_coeff[i] = -1.0` for >= constraints (surplus variable has negative coefficient in standard form)

| Component | What it does | Status |
|-----------|-------------|--------|
| **phase_one.c** (initializer) | Sets diag_coeff based on slack feasibility direction. For >=: if surplus_val >= 0 → diag=-1, else diag=+1 | CONDITIONAL |
| **step.c** `get_auxiliary_coeff_fallback()` | Returns +1.0 for >= when rhs>0, -1.0 when rhs<=0 | !! INCONSISTENT |
| **reduced_costs.c** `get_auxiliary_coeff()` | Returns -1.0 for >= unconditionally | CORRECT per spec |
| **ftran.c** | Uses `diag_coeff[i]` from BasisState directly | Reads whatever was set |
| **btran.c** | Uses `diag_coeff[i]` from BasisState directly | Reads whatever was set |

**!! MISMATCH DETECTED:** `step.c` and `reduced_costs.c` compute diag_coeff differently for >= constraints. This means:
- Reduced costs (pricing input) use one sign convention
- Column extraction in step (FTRAN input) may use another
- The FTRAN result and the reduced cost will be algebraically inconsistent

**Impact:** Could cause ratio test to compute wrong ratios → false UNBOUNDED or wrong leaving variable selection.

---

### CONTRACT 3: BFRT Coefficient Negation

**V2 Spec says:** When a variable bound-flips, negate its constraint row coefficients in the working CSR/CSC copies.

| Component | What it does | Status |
|-----------|-------------|--------|
| **step.c** `negate_constraint_row()` | Negates CSR+CSC values AND RHS AND diag_coeff | Exists but... |
| **step.c** BFRT path | Calls negate_constraint_row for each flip | ...modifies STATE directly |
| **Model matrix** | Should be read-only during solve | !! MUTATION if CSR/CSC point to model |

**!! POTENTIAL ISSUE:** If `state->csr_*` / `state->csc_*` are aliases to the model matrix (not working copies), BFRT negation corrupts the model. P3.1 requires working copies to be owned by SolverState.

**Impact:** Model corruption → subsequent solves on same model broken. During current solve: algebraic inconsistency if negated row interacts with non-negated auxiliary coefficient.

---

### CONTRACT 4: Phase I → Phase II Transition Completeness

**V2 Spec says:** Transition must: (1) force refactorization, (2) pivot out zero-valued artificials, (3) reset pricing, (4) swap objective, (5) recompute reduced costs.

| Step | Implementation | Status |
|------|---------------|--------|
| Force refactorization | `cxf_solver_refactor()` called at line 231 | OK |
| Pivot out zero artificials | Lines 233-284, degenerate pivots | OK |
| Reset pricing | Lines 301-307, invalidate + set level 0 | OK |
| Swap objective | Lines 207-220, restore original obj coeffs | OK |
| Recompute reduced costs | `cxf_compute_reduced_costs()` called | OK |
| Fix artificial bounds for equality | `work_ub[n+i] = 0` for equalities | OK |

**Verdict: APPEARS COMPLETE** — But the diag_coeff flip at line 290 (`diag_coeff[i] = -diag_coeff[i]` for >= with artificial in basis) needs investigation. Does this interact correctly with CONTRACT 2?

---

### CONTRACT 5: Ratio Test Bound Selection

**V2 Spec says:** Given entering direction `s` and pivot column `d`:
- If `s·d_i > 0`: basic variable decreases → hits LOWER bound → ratio = (x_i - lb_i) / (s·d_i)
- If `s·d_i < 0`: basic variable increases → hits UPPER bound → ratio = (x_i - ub_i) / (s·d_i)

| Component | What it does | Status |
|-----------|-------------|--------|
| **ratio_test.c** pass 1 | `sd = s * d_i; if sd > tol: ratio = (x - lb) / sd; if sd < -tol: ratio = (x - ub) / sd` | OK |
| **Infinite bounds** | Skips variables with `lb = -inf` (sd>0 case) or `ub = +inf` (sd<0 case) | CHECK NEEDED |
| **Free variables** (lb=-inf, ub=+inf) | Both bounds infinite → variable skipped entirely | OK (no ratio) |

**Verdict: LIKELY OK** — But need to verify: what happens when BOTH bounds are finite and ratio is negative? A negative ratio means the variable is already infeasible. If not handled, this could cause false UNBOUNDED.

---

### CONTRACT 6: Pricing Queue Dirty Notification After BFRT

**V2 Spec says:** After BFRT flips, ALL flipped variables must be cascade-updated in pricing.

| Component | What it does | Status |
|-----------|-------------|--------|
| **step.c** post-pivot | Calls `cxf_pricing_update_var(entering)` | OK |
| **step.c** post-pivot | Calls `cxf_pricing_update_constr(leavingRow)` | OK |
| **step.c** BFRT flips | Does NOT call pricing update for flipped vars | !! MISSING |

**!! GAP DETECTED:** Flipped variables change their reduced costs (sign flip) but pricing doesn't know. On the next iteration, pricing may offer stale candidates or miss newly attractive ones.

**Impact:** Incorrect entering variable selection → suboptimal pivots → cycling or slow convergence. Could contribute to false UNBOUNDED if a good entering candidate is missed and the solver exhausts all levels.

---

### CONTRACT 7: Reduced Cost Auxiliary Computation Consistency

**V2 Spec says:** For auxiliary variable at row i: `dj[n+i] = work_obj[n+i] - pi[i] * diag_coeff[i]`

| Component | What it uses for diag_coeff | Source |
|-----------|---------------------------|--------|
| `reduced_costs.c` full recompute | `get_auxiliary_coeff()`: -1 for >=, +1 for <=, sign(rhs) for = | Function-local |
| `step.c` incremental update | `get_auxiliary_coeff_fallback()`: +1 for >= if rhs>0 | Function-local |
| `phase_one.c` initialization | Conditional on slack feasibility direction | Sets BasisState.diag_coeff |

**!! THREE DIFFERENT SOURCES:** The auxiliary coefficient is computed three different ways in three different places. If they disagree, the reduced costs computed by full recompute (after refactorization) will differ from the incremental updates (between refactorizations), causing numerical drift in reduced costs that compounds over iterations.

**Impact:** Gradual reduced cost drift → wrong entering variable → false UNBOUNDED or wrong objective. This is the #1 suspect for the 18 false UNBOUNDED failures.

---

### CONTRACT 8: Objective Value Update Consistency

**V2 Spec says:** `obj_value += s * dj[entering] * stepSize`

| Component | What it does | Status |
|-----------|-------------|--------|
| **step.c** standard path | `obj_value += entering_sign * work_dj[entering] * stepSize` | OK |
| **step.c** BFRT path | Accumulates across flips: `total_step += step; obj_value += entering_sign * dj * total_step` | CHECK: is this additive or does it use the final total? |

**Potential issue:** In BFRT, the objective should be updated with the TOTAL step for the entering variable, not incremental per-flip. Need to verify the accumulation logic.

---

## CONFIRMED ROOT CAUSES (Diagnostic Evidence)

### ROOT CAUSE 1: BFRT `negate_constraint_row` Corrupts Working Matrix → 18 False UNBOUNDED

**Evidence from diagnostic traces (recipe, grow7, boeing2):**

| Instance | UNBOUNDED iter | Phase | BFRT flips | Basic vars past bounds | Worst infeasibility |
|----------|---------------|-------|------------|----------------------|---------------------|
| recipe | 152 | II | 12 | 12 past ub | 109 |
| grow7 | 347 | II | 8 | 10 past ub | 3.58e8 |
| boeing2 | 156 | I | **262** | 49 past lb, 12 past ub | 1.45e5 |

**Causal chain:**
1. BFRT flips a basic variable between bounds
2. `negate_constraint_row()` negates the CSR/CSC row, RHS, AND diag_coeff
3. But the LU factorization (eta vectors) is NOT updated to reflect the negation
4. All subsequent FTRAN/BTRAN results are WRONG (they use stale factorization)
5. Wrong pivot columns → wrong step sizes → basic variables drift past bounds
6. Cumulative drift grows each iteration (compounding)
7. Eventually ALL basic variables with significant pivot entries are past bounds
8. Ratio test has 0 valid candidates → returns UNBOUNDED

**Key evidence at recipe iter 152:**
```
RATIO_DIAG: entering=62 dir=1 too_small=90 inf_bound=0 neg_ratio=1 valid=0
neg_row=75 bv=73 x=42.50 lb=10.00 ub=18.00 ratio=-24.50 d=-1.00
```
Variable 73 is 24.5 past its upper bound (18.0). The only ratio is negative → skipped → UNBOUNDED.

**CONFIRMED V2 SPEC BUG:** The spec (harris_ratio_test.md Stage 3, Step 6c) literally says:
"Negate the relevant row coefficients in the constraint matrix to maintain algebraic
consistency when the variable changes its bound direction." This is WRONG for three reasons:

1. **Dual/primal conflation.** The spec references Forrest & Goldfarb (1992) who developed
   BFRT for DUAL simplex where row operations are natural. ConvexFeld uses PRIMAL simplex
   where the correct bound-flip operation is a column substitution (x → u-x), not row negation.

2. **"Algebraic consistency" is undefined.** The spec claims negation "maintains the invariant"
   but is SILENT on LU/eta factor validity. The factorization is immediately invalid after
   row negation — B has changed but B^{-1} (stored as eta product) hasn't. This isn't
   "numerical drift" — it's a discrete algebraic break.

3. **Refactorization described as optional.** The spec says "periodic refactorization resets
   drift" as if the issue is gradual. But any FTRAN/BTRAN call after row negation produces
   wrong results, so the very next iteration is corrupted.

Standard primal BFRT (Koberstein 2005, Maros 2003) does NOT negate rows. It clamps the
flipped variable to the opposite bound, extends the step, and continues — no matrix modification.

**Fix:** Remove `negate_constraint_row()` calls entirely. Standard BFRT clamping is sufficient.

---

### ROOT CAUSE 2: Phase I >= Constraint Formulation → 6 False INFEASIBLE

**Evidence from scorpion diagnostic:**
- 388 constraints: 48 <=, 60 >=, 280 =
- 53 artificials — ALL from violated >= constraints
- 53 diag_coeff mismatches: diag=+1.0 but expected=-1.0 for >= sense
- Phase I stalls at obj=0.036 after 471 iterations with Bland's rule active
- No improving directions found (most negative rc = -1e-15)

**Analysis:**
For violated >= constraints, phase_one.c sets `diag_coeff = +1.0` (line 102).
This transforms `a'x >= b` into `a'x + aux = b` (artificial direction).
The correct surplus representation is `a'x - s = b` with `diag = -1.0`.

With `diag = +1`, there is no surplus variable. The solver has fewer degrees of
freedom — it can only reduce the artificial by increasing `a'x`, with no slack to
absorb intermediate infeasibilities. Combined with degeneracy and Bland's rule,
Phase I gets permanently stuck.

**Fix:** For >= constraints, ALWAYS use `diag = -1.0` (surplus direction).
When the constraint is violated, the surplus value is negative — this is OK
because Phase I must drive it to zero anyway. The artificial objective
coefficient (1.0) on the surplus variable forces Phase I to make it non-negative.

---

### ROOT CAUSE 3: Missing OBJSENSE Parsing → 3 Wrong Objective (stand*)

**Evidence:**
- standata, standgub, standmps all have sign-inverted objectives
- MPS parser does NOT handle OBJSENSE section
- CxfModel has no `obj_sense` field
- Solver always minimizes; maximization problems get wrong sign

**Fix:** Add OBJSENSE parsing to MPS parser. Negate objective coefficients when
OBJSENSE = MAX. Add `obj_sense` field to CxfModel.

---

### REMAINING: brandy, forplan, israel (Wrong Objective, Non-Sign)

These are likely numerical drift from ROOT CAUSE 1 (BFRT corruption). After fixing
BFRT, re-test to see if they resolve. If not, separate investigation needed.

---

## FLAGGED MISMATCHES — Revised Priority Order

| # | Mismatch | Severity | Confirmed? | Failure Mode | Files |
|---|----------|----------|-----------|--------------|-------|
| **M1** | ~~diag_coeff 3-way inconsistency~~ | ~~CRITICAL~~ | **REFUTED** — step.c and reduced_costs.c are now unified (P0.4 done) | N/A | N/A |
| **M2** | BFRT pricing notification missing | FIXED | **Already implemented** (step.c:673-677) | N/A | step.c |
| **M3** | BFRT `negate_constraint_row` corrupts matrix | **CONFIRMED** | **ROOT CAUSE for 18 false UNBOUNDED** | step.c:96-140, 576-577 |
| **M4** | Phase I diag_coeff=+1 for violated >= | **CONFIRMED** | **ROOT CAUSE for 6 false INFEASIBLE** | phase_one.c:101-102 |
| **M5** | BFRT objective accumulation | UNKNOWN | Likely masked by M3; recheck after fix | step.c |
| **M6** | Ratio test neg ratio = symptom of M3 | **CONFIRMED SYMPTOM** | Bound violations from M3 cause negative ratios | ratio_test.c |
| **M7** | Missing OBJSENSE parsing | **CONFIRMED** | **ROOT CAUSE for 3 wrong objective** | mps_parse.c |

---

## V2 SPEC VIOLATIONS CATALOG (Revised)

| ID | Spec Ref | Violation | Status | Required Action |
|----|----------|-----------|--------|-----------------|
| **V1** | P3.5 | `negate_constraint_row` prescribed by spec but wrong for primal simplex | **SPEC BUG** | Remove row negation; spec imported dual technique into primal context |
| **V2** | P0.9 | BFRT pricing notification | **FIXED** | Already in step.c:673-677 |
| **V3** | Phase I | Conditional diag_coeff based on initial feasibility | **SPEC OMISSION** | Spec doesn't mention diag_coeff; always use fixed sign per sense |
| **V4** | MPS | No OBJSENSE parsing | **MISSING FEATURE** | Add OBJSENSE handling |
| **V5** | MPS | RANGES section recognized but content ignored | **MISSING FEATURE** | Add RANGES handler |

---

## POST-RC1 FAILURE LANDSCAPE (BFRT disabled)

After disabling BFRT, re-running diagnostics reveals three distinct remaining failure mechanisms:

### Mechanism A: RC2 diag_coeff → Phase I stalls → false INFEASIBLE
Instances: boeing1(47 mismatches), boeing2, capri, finnis(88 mismatches), scorpion(53), bandm

Pattern: Wrong diag_coeff for >= and/or <= constraints → Phase I operates with
incorrect algebraic representation → Phase I stalls at non-zero objective →
declares INFEASIBLE on feasible problems. Fix: RC2.

### Mechanism B: RC2 diag_coeff → wrong FTRAN → bound violations → wrong obj or UNBOUNDED
Instances: vtp.base(32 mismatches, 29 past-bound vars), israel(8 constraint violations),
recipe(2.1% obj error)

Pattern: Wrong diag_coeff → every FTRAN/BTRAN uses wrong auxiliary column →
accumulated numerical errors → basic variables drift past bounds → either
wrong objective (if solver reaches "optimal") or UNBOUNDED (if ratio test fails).

### Mechanism C: Phase I degeneracy → numerical breakdown → UNBOUNDED
Instances: grow7(343 degenerate iters, all =), scsd1(169 degenerate iters, all =)

Pattern: All-equality constraints with no >= or <= → Phase I has all degenerate
pivots (ratio=0) → Bland's rule activates → basis representation degrades over
100+ degenerate pivots → FTRAN produces near-zero pivot columns → ratio test
finds no valid candidates → UNBOUNDED.

Root cause: not RC2 (no >= constraints). This is a Phase I degeneracy + basis
maintenance issue. Better perturbation or reduced refactorization interval
would help. Possibly also related to the Phase I formulation for = constraints
(conditional diag based on slack sign).

### Summary: RC2 is the dominant remaining issue

| Mechanism | Instances | Root Cause | Fix |
|-----------|-----------|-----------|-----|
| A (INFEASIBLE) | 6 | RC2 diag_coeff | Fix diag_coeff |
| B (wrong obj/UNBOUNDED) | 3 | RC2 diag_coeff | Fix diag_coeff |
| C (Phase I degeneracy) | 2 | Degeneracy + numerical | Perturbation, refactor freq |

---

## DIAGNOSTIC TOOL

Location: `tools/diagnose.c`
Build: `gcc -std=c99 -O2 -I include -o build/diagnose tools/diagnose.c -L build -lconvexfeld -lm`
Usage: `build/diagnose <mps_file> [max_iter]`

Traces: constraint senses, bounds, diag_coeff mismatches, pivot column stats,
ratio test decomposition (too_small/inf_bound/neg_ratio/valid), basic variable
bound violations, BFRT flip counts.
