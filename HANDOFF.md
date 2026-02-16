# Agent Handoff

*Last updated: 2026-02-16*

---

## STATUS: V2 Spec Migration — Phase 1 Complete, Phase 2 Needed

### Session Summary

Migrated v2 cleanroom specs to ConvexFeld. **Phase 1 (naming) is 100% complete. Phase 2 (deep content cleanup) remains.**

#### What was done:
1. **Archived v1 specs** to `docs/specs-v1/` (preserves all original v1 content)
2. **Copied 66 v2 spec files** to `docs/specs-v2/` (62 individual specs + SPECIFICATION.md + 5 supporting files)
3. **Excluded 2 MIP-only files**: `solve_mip.md`, `solve_multiobj.md`
4. **Naming transform: 100% CLEAN** — zero traces of upstream naming remain anywhere in specs-v2
5. **Transformation script**: stored in the upstream project (can be re-run)

#### What remains (Phase 2):

**A. License-server content removal (~370 hits across ~24 files)**

The initial transform replaced simple patterns (`license validation` → `initialization validation`, etc.) but missed deeply embedded license infrastructure: WLS, ISV, token servers, compute servers, license acquisition pipelines, license thread limits, license error codes.

**Heaviest files needing cleanup:**
- `specs/modules/environment_lifecycle.md` — ~50+ hits, entire license acquisition pipeline
- `specs/data-model/environment.md` — ~39 hits, license fields/WLS/ISV/license type enum
- `specs/modules/allocation_helpers.md` — license validation in cxf_setup_resources
- `specs/reference/parameters_defaults.md` — WLS/TokenServer/ComputeServer parameter sections
- `specs/integration/parameter_system.md` — license thread limits, Layer 2 license overrides
- `specs/integration/threading_model.md` — license thread limits
- `specs/modules/solve_barrier_concurrent.md` — distributed license locks
- `specs/reference/error_status_codes.md` — NO_LICENSE, license error codes
- ~16 more files with lighter license references

**B. MIP reference cleanup (~559 hits across ~30 files)**

MIP references are pervasive (parameter tables, callback events, function cross-refs, dispatch paths). Key cleanup:
- Remove MIP parameter sections from `parameters_defaults.md`
- Remove MIP dispatch paths from `solve_entry.md`, `solve_lp_core.md`
- Remove MIP callback events from `callback_protocol.md`
- Clean dangling cross-refs to excluded `solve_mip.md` and `solve_multiobj.md`
- Remove MIP functions from `FUNCTION_MAP.md` and `PLAN.md`
- Strip MIP-specific sections from `solution_processing.md`, `model_type_checking.md`

**C. Regenerate `output/SPECIFICATION.md`** after A+B are done (it mirrors all source specs)

---

## Recommended Approach for Phase 2

1. **Write a second-pass Python cleanup script** that handles:
   - Whole-section removal (sections with "License" headings)
   - MIP parameter table removal
   - Dangling cross-reference cleanup

2. **Use parallel subagents** — one per file, NO race conditions:
   - Group A: Heavy license files (environment_lifecycle.md, environment.md, allocation_helpers.md, parameters_defaults.md)
   - Group B: Moderate license + MIP files (solve_entry.md, threading_model.md, etc.)
   - Group C: Light cleanup (1-2 line fixes across ~16 files)

3. **Re-run SPECIFICATION.md assembly** after all source specs are clean

4. **Final verification** — grep for `license`, `License`, `MIP`, `compute.server`, `single.use`, `WLS`, `ISV`, `token.server`

---

## File Locations

| Item | Path |
|------|------|
| V2 specs (target) | `~/Projects/convexfeld/docs/specs-v2/` |
| V1 specs (archived) | `~/Projects/convexfeld/docs/specs-v1/` |
| Transform script | stored in upstream project |
| Original v2 specs | stored in upstream project |
| Consolidated output | `~/Projects/convexfeld/docs/specs-v2/output/SPECIFICATION.md` |

---

## Previous Handoff (V2 Spec Gap Analysis)

The v2 spec gap analysis from 2026-02-15 identified these rework areas for ConvexFeld implementation:

- **RED** (rewrite): Pricing system (~2,000 LOC), Bound propagation (~1,500 LOC), Perturbation
- **YELLOW** (significant rework): Data structures, BFRT ratio test, Phase I→II, Matrix scaling, Method selection
- **GREEN** (mostly correct): Basis operations, Support modules, Simplex core

Priority order: P0 Fix perturbation → P1 Matrix scaling → P2 BFRT → P3 Phase transitions → P4 Pricing → P5 Bound propagation

Quality gates: 35/36 tests pass, build clean, Netlib 11/27 pass.

Full v2 spec: `docs/specs-v2/output/SPECIFICATION.md` (24,070 lines, now with cxf_ naming)
