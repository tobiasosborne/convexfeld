# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: Remediation plan complete. 3 root causes found for 27/30 Netlib failures.

### What Was Done This Session

**Investigation of 30 non-timeout Netlib failures using holistic architectural analysis:**

1. **Built Component Interface Contract Map** (`docs/architecture_contract_map.md`)
   - 3-scale architectural representation: macro flow, iteration anatomy, interface contracts
   - 8 interface contracts checked for producer/consumer consistency
   - 6 mismatches flagged, 5 v2 spec violations cataloged
   - Revised after diagnostic evidence confirmed/refuted hypotheses

2. **Built diagnostic tool** (`tools/diagnose.c`)
   - Standalone harness that calls internal solver functions directly
   - Traces per-iteration: pivot columns, ratio test decomposition, bound violations,
     diag_coeff consistency, BFRT flip counts, basic variable feasibility audits
   - Build: `gcc -std=c99 -O2 -I include -o build/diagnose tools/diagnose.c -L build -lconvexfeld -lm`

3. **Ran diagnostic traces on 4 failing instances:**
   - `recipe` (false UNBOUNDED) — 12 BFRT flips → 12 basic vars past bounds → UNBOUNDED
   - `grow7` (false UNBOUNDED) — 8 flips → worst infeas 3.58e8 → UNBOUNDED
   - `boeing2` (false UNBOUNDED) — 262 flips → worst infeas 1.45e5 → UNBOUNDED
   - `scorpion` (false INFEASIBLE) — 53 diag_coeff mismatches → Phase I stalls

4. **Wrote remediation plan** (`docs/remediation_plan.md`)

### Three Confirmed Root Causes

| # | Root Cause | Failures | Fix |
|---|-----------|----------|-----|
| **RC1** | `negate_constraint_row` in BFRT corrupts matrix without updating LU | 18 false UNBOUNDED | Delete the function (standard BFRT doesn't need it) |
| **RC2** | Phase I uses diag=+1 for violated >= (no surplus variable) | 6 false INFEASIBLE | Always use diag=-1 for >= |
| **RC3** | MPS parser doesn't handle OBJSENSE | 3 wrong objective (stand*) | Add OBJSENSE parsing |

### Important: V2 Spec May Be Wrong

P3.5 spec ("harris_ratio_test.md Stage 3 Step 6c") prescribes constraint row negation
after BFRT flips. **Standard BFRT does NOT do this.** The spec either describes a
technique that requires BOTH negation AND factorization update (but only negation was
implemented), or the spec is simply incorrect. Either way, removing the negation is
the correct fix.

---

## Next Steps — Execute Remediation Plan

**MANDATORY: Read `docs/remediation_plan.md` for full details.**
**MANDATORY: Read `docs/architecture_contract_map.md` for architectural context.**

### Execution Order:

1. **RC1 (BFRT fix)** — Delete `negate_constraint_row()` from step.c
   - Delete function (lines 96-140)
   - Delete call site (lines 576-577)
   - Run unit tests: `cd build && ctest`
   - Run 18 false UNBOUNDED instances: `build/bench_netlib --filter recipe` etc.
   - Expected: 18 instances change from UNBOUNDED to OPTIMAL or different status

2. **RC2 (Phase I >= fix)** — Fix diag_coeff in phase_one.c
   - Change >= handling to always use diag = -1.0 (lines 95-106)
   - Simplify Phase I→II transition code (lines 225-228)
   - Run unit tests
   - Run scorpion, bandm, bore3d, e226, stair, tuff
   - Expected: 6 instances change from INFEASIBLE to OPTIMAL

3. **RC3 (OBJSENSE)** — Add to MPS parser
   - Add obj_sense field to CxfModel
   - Parse OBJSENSE section in mps_parse.c
   - Negate objective in solve_lp.c for maximization
   - Run standata, standgub, standmps

4. **Final retest** — Full Netlib suite
   - Expected: ~46+ pass (up from 19)
   - Remaining ~3 wrong objective may also resolve after RC1

---

## File Locations

| Item | Path |
|------|------|
| **Remediation plan** | `docs/remediation_plan.md` |
| **Architecture contract map** | `docs/architecture_contract_map.md` |
| **Diagnostic tool** | `tools/diagnose.c` |
| BFRT bug location | `src/simplex/step.c:96-140, 576-577` |
| Phase I >= bug | `src/simplex/phase_one.c:95-106` |
| MPS parser | `src/api/mps_parse.c` |
| Main solver loop | `src/simplex/solve_lp.c` |
| Step iteration engine | `src/simplex/step.c` |
| Ratio test | `src/simplex/ratio_test.c` |
| Phase I setup + transition | `src/simplex/phase_one.c` |
| V2 compliance roadmap | `docs/v2_compliance_roadmap.md` |
| Unit tests | `tests/unit/` (40 tests) |
| Netlib benchmark | `benchmarks/bench_netlib.c` |

## DO NOT
- Skip reading the remediation plan before implementing fixes
- Apply fixes out of order (RC1 → RC2 → RC3)
- Skip unit tests between fixes
- Run full Netlib with >1s timeout
