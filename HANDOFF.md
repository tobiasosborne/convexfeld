# Agent Handoff

*Last updated: 2026-02-16*

---

## STATUS: V2 Specs Fully Cleaned and Renamed

### Session Summary

Completed full v2 spec cleanup: MIP/license/compute-server content removal, misnomer renaming, and SPECIFICATION.md regeneration.

#### Commits this session:
1. **`ed8270e`** — Deep cleanup: removed MIP, license server, compute server content from 46 source specs (-1,043 net lines)
2. **`ec86025`** — Regenerated SPECIFICATION.md (v2.1, 22,222 lines) + added `assemble_spec.py`
3. **`381f192`** — Renamed 16 misnomer functions/structures across 23 files (285 replacements), updated commentary, regenerated SPECIFICATION.md

#### What was done:

**Phase 2: Deep content cleanup (complete)**
- Removed all MIP content: callback events (MIP_SOLUTION/MIP_NODE), MIPGap, integrality checks, MIP var types, MIP dispatch paths, MIP parameter sections
- Removed all license infrastructure: WLS/ISV/token server fields, license acquisition pipeline, license thread limits, license error codes
- Removed all compute server content: communication model, callback protocol, job termination, lock hierarchy, error codes, environment fields
- Fixed "remote remote solver" doubled-word artifact across 13+ files
- Bibliography citations to MIP papers intentionally retained

**Misnomer renames (complete)**
16 functions/structures renamed to match actual behavior:

| Old Name | New Name | Reason |
|----------|----------|--------|
| `cxf_simplex_iterate` | `cxf_log_iteration_progress` | Logs progress, doesn't iterate |
| `cxf_simplex_cleanup` | `cxf_simplex_postsolve` | Post-solve analysis, not just cleanup |
| `cxf_basis_refactor` | `cxf_fix_variables_at_bounds` | Variable fixing, not LU refactor |
| `cxf_basis_snapshot` | `cxf_progress_snapshot` | Scalar counters, not full basis |
| `cxf_sort_indices` | `cxf_sort_by_values` | Sorts by values, not indices |
| `cxf_check_nan_or_inf` | `cxf_is_finite` | Returns true for finite (inverted) |
| `cxf_cleanup_helper` | `cxf_propagate_bounds` | Constraint-based bound tightening |
| `cxf_setup_basis` | `cxf_free_warmstart_basis` | Destructor, not setup |
| `cxf_setup_work_arrays` | `cxf_free_work_arrays` | Destructor, not setup |
| `cxf_free_solver_state` | `cxf_free_attribute_table` | Frees attr table specifically |
| `cxf_errorlog` | `cxf_set_error_string` | Writes error buffer, not log |
| `cxf_pre_optimize_callback` | `cxf_pre_optimize_hook` | Lifecycle hook, not user callback |
| `cxf_post_optimize_callback` | `cxf_post_optimize_hook` | Lifecycle hook, not user callback |
| `cxf_acquire_solve_lock` | `cxf_save_locale_state` | Saves locale, no mutex |
| `cxf_set_thread_count` | `cxf_validate_thread_count` | Validates, doesn't set |
| `WorkArrays` | `SolutionData` | Solution output, not scratch buffers |

**Tools created:**
- `docs/specs-v2/assemble_spec.py` — Regenerates SPECIFICATION.md from source specs
- `docs/specs-v2/rename_misnomers.py` — Reference for the rename mappings

---

## Next Steps

### Immediate (spec-related)
1. **Rename misnomers in code** — The specs now use new names; implementation code still uses old names. Apply same renames to `src/` and `tests/`.
2. **Verify SPECIFICATION.md quality** — Spot-check TOC anchors work, section ordering is correct

### Implementation priorities (from v2 spec gap analysis, 2026-02-15)
- **P0:** Fix perturbation (stubs override real impl in context.c)
- **P1:** Matrix scaling
- **P2:** BFRT ratio test
- **P3:** Phase I→II transitions
- **P4:** Pricing system rewrite (~2,000 LOC)
- **P5:** Bound propagation rewrite (~1,500 LOC)

### Quality gates
- 35/36 tests pass, build clean
- Netlib: 11/27 pass

---

## File Locations

| Item | Path |
|------|------|
| V2 specs (clean) | `docs/specs-v2/specs/` (62 source files) |
| V1 specs (archived) | `docs/specs-v1/` |
| Consolidated spec | `docs/specs-v2/output/SPECIFICATION.md` (22,219 lines) |
| Assembly script | `docs/specs-v2/assemble_spec.py` |
| Rename reference | `docs/specs-v2/rename_misnomers.py` |
| FUNCTION_MAP | `docs/specs-v2/FUNCTION_MAP.md` |
| PLAN | `docs/specs-v2/PLAN.md` |
