# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: Phase 0-1 done, Phase 2 5/6, Phase 3 done, Phase 4: 5/9 (critical path done)

### Session Summary

- **P4.2 (95ny)**: `cxf_pricing_update_var` — CSC column traversal producer
- **P4.3 (bjy8)**: `cxf_pricing_update_constr` — CSR row traversal producer
- **P4.4 (pt31)**: `cxf_pricing_update_queues` — V2 queue consumer with promote/demote
- **P4.5 (v35i)**: `cxf_pricing_candidates_v2` — adaptive retrieval (full scan + partial expansion)
- **P4.8 partial**: Wired V2 pricing into step.c iteration loop (producers + consumer + candidates)
- Shared V2 queue insertion helpers in `queue_insert.c`
- 11 new V2 pricing unit tests (all passing)
- 40/40 total tests pass

### Closed This Session

| Phase | Issues Closed |
|-------|---------------|
| P4 | P4.2 (update_var), P4.3 (update_constr), P4.4 (update_queues), P4.5 (candidates_v2) |

### What Changed

**V2 Queue Insertion Protocol (`queue_insert.c` — new file):**
- `v2_insert_constr()` / `v2_insert_var()` — 4-bit flag-based insertion into levels 1-2
- Committed/pending split: committed if level inactive, pending if active
- O(1) duplicate prevention via flag bits (0x01/0x02 for L1, 0x04/0x08 for L2)

**P4.2 — update_var (`update_var.c` — new file):**
- Traverses CSC column for entering variable, calls `v2_insert_constr` per row
- Also maintains V1 dirty flags for backward compat

**P4.3 — update_constr (`update_constr.c` — new file):**
- Traverses CSR row for leaving constraint, calls `v2_insert_var` per column
- CSC fallback when CSR not available

**P4.4 — update_queues (`update.c` — added V2 consumer):**
- Phase activation guard (first call just activates, returns)
- Level 0: status-based filter + compact (keep nonbasic vars, keep all constraints)
- Levels 1-2: flag promote (pending→committed) / demote (stale→discard)
- Invalidates all 6 cache slots at levels 1-2

**P4.5 — candidates_v2 (`candidates.c` — added V2 retrieval):**
- Level 0 fast path: return committed queue directly (O(1))
- Cache hit: return cached result
- Cache miss: 3-threshold adaptive decision (expansion_multiplier=2.0, coverage=0.5, work_factor=5e-4)
- Full scan: all nonbasic vars → output buffer
- Partial expansion: seed from var queue → expand via constr queue CSR → filter
- Uses bit 4 (0x10) of var_flags as temporary selection marker, cleaned in filter step

**P4.8 partial — step.c wiring:**
- Phase 8: `cxf_pricing_update_var(entering)` + `cxf_pricing_update_constr(leaving_row)` replace old cascade
- V1 cascade calls retained for backward compat (step2/step3/perturbation)
- `cxf_pricing_update_queues()` called before pricing evaluation
- V2 candidates retrieved first; V1 full scan as fallback if V2 queue empty

---

## Next Steps — Critical Path

```
P4.5 (done) → P5.1 (6wgv) → Phase 6
```

### Priority order:
1. `6wgv` **P5.1**: EXPAND perturbation (now unblocked by P4.5)
2. `52go` **P4.7**: Mark dirty with 4-bit flags (upgrade V1 mark_dirty)
3. `fevq` **P4.6**: Rewrite end_level with queue compaction
4. `txab` **P4.8**: Complete V2 wiring (remove V1 fallbacks)
5. `9ef0` **P4.9**: Steepest edge weight updates

### Also ready (parallel tracks):
- `huah` P3.2: SolutionData struct
- `dk0i` P3.3: Missing SolverState fields
- `hyi2` P3.4: Missing BasisState fields
- `auj4` P2.3: Eta memory pool
- `uyfk` P2.4: fix_variables_at_bounds
- `6js6` Refactor update.c to < 200 LOC
- `p3sl` Refactor candidates.c to < 200 LOC

### DO NOT
- Run Netlib benchmarks — waste of time until more phases complete
- Skip the dependency chain

---

## File Locations

| Item | Path |
|---|---|
| V2 compliance roadmap | `docs/v2_compliance_roadmap.md` |
| V2 queue insertion | `src/pricing/queue_insert.c` |
| V2 update_var (P4.2) | `src/pricing/update_var.c` |
| V2 update_constr (P4.3) | `src/pricing/update_constr.c` |
| V2 update_queues (P4.4) | `src/pricing/update.c` |
| V2 candidates (P4.5) | `src/pricing/candidates.c` |
| Step (V2 wiring) | `src/simplex/step.c` |
| PricingState header | `include/convexfeld/cxf_pricing.h` |
| V2 pricing tests | `tests/unit/test_pricing_v2.c` |
| Beads issues | `bd ready` / `bd list --status=open -n 100` |
