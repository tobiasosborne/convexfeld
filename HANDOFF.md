# Agent Handoff

*Last updated: 2026-02-21*

---

## STATUS: Phase 0-1 done, Phase 2 mostly done (5/6), Phase 3 done (5/5), Phase 4 started (1/9)

### Session Summary

- **P3.1 (zirq)**: CSR/CSC working copies on SolverState — 11 files updated
- **P3.5 (h790)**: BFRT matrix coefficient negation
- **P4.1 (706o)**: PricingState v2 struct rebuild (all new fields added)
- Also fixed `cxf_pricing_cascade_update` to use state's CSC (P3.1 follow-up)
- 39/39 unit tests pass throughout

### Closed This Session

| Phase | Issues Closed |
|-------|---------------|
| P3 | P3.1 (CSR/CSC working copies), P3.5 (BFRT coefficient negation) |
| P4 | P4.1 (PricingState v2 struct rebuild) |

### What Changed

**P3.1 — Matrix isolation (zirq):**
- Added 8 fields to SolverState: `csc_col_ptr`, `csc_row_idx`, `csc_values`, `csr_row_ptr`, `csr_col_idx`, `csr_values`, `work_rhs`, `work_sense`
- `cxf_simplex_init` copies model matrix into state-owned arrays
- `cxf_simplex_final` frees them
- Updated 11 files to read from `state->csc_*`/`state->csr_*` instead of `model->matrix`
- **Files NOT changed**: presolve.c (runs before state), solve_lp.c pre-init checks

**P3.5 — BFRT negation (h790):**
- Added `negate_constraint_row()` static function in step.c
- Negates CSR + CSC coefficients, RHS, and diag_coeff for flipped rows
- Called from BFRT section after flips are determined

**P4.1 — PricingState v2 (706o):**
- Added `CXF_MAX_PRICING_LEVELS` (3) to cxf_pricing.h
- Added ~20 new fields: `level_active[]`, `var_flags`/`constr_flags` (uint8_t), per-level queue arrays with committed/total counts, 3-slot caches, output buffers
- V1 fields retained for backward compat until P4.2-P4.8 rewrite functions
- Allocation in init.c (variable side) and constr_init.c (constraint side)
- Deallocation in context.c

---

## Next Steps — Critical Path

```
P4.1 (done) → P4.5 (v35i) → P5.1 (6wgv) → Phase 6
```

P4.5 depends on P4.1 (done). But P4.2-P4.4 should be done first (they provide the producer/consumer functions that P4.5's adaptive strategy uses).

### Priority order:
1. `95ny` **P4.2**: Implement cxf_pricing_update_var (producer)
2. `bjy8` **P4.3**: Implement cxf_pricing_update_constr (producer)
3. `pt31` **P4.4**: Implement cxf_pricing_update (queue consumer)
4. `v35i` **P4.5**: Rewrite cxf_pricing_candidates (adaptive strategy) — CRITICAL PATH
5. `52go` **P4.7**: Mark dirty with 4-bit flags
6. `fevq` **P4.6**: Rewrite end_level with queue compaction
7. `txab` **P4.8**: Wire v2 pricing into iteration loop

### Also ready (parallel tracks):
- `huah` P3.2: SolutionData struct
- `dk0i` P3.3: Missing SolverState fields (note: `constraintSense` and `constraintRHS` copies are now done via P3.1's `work_sense`/`work_rhs`)
- `hyi2` P3.4: Missing BasisState fields
- `auj4` P2.3: Eta memory pool
- `uyfk` P2.4: fix_variables_at_bounds

### DO NOT
- Run Netlib benchmarks — waste of time until more phases complete
- Skip the dependency chain

---

## File Locations

| Item | Path |
|---|---|
| V2 compliance roadmap | `docs/v2_compliance_roadmap.md` |
| SolverState (P3.1 CSC/CSR fields) | `include/convexfeld/cxf_solver.h` |
| PricingState (P4.1 v2 fields) | `include/convexfeld/cxf_pricing.h` |
| Step (P3.5 BFRT negation) | `src/simplex/step.c` |
| Pricing init | `src/pricing/init.c`, `src/pricing/constr_init.c` |
| Pricing lifecycle | `src/pricing/context.c` |
| Pricing queue ops | `src/pricing/queue.c` |
| Beads issues | `bd ready` / `bd list --status=open -n 100` |
