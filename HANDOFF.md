# Agent Handoff

*Last updated: 2026-02-16*

---

## STATUS: Full V2 Spec Audit Complete + Remediation Plan Created

### Session Summary

Ran 20 parallel Sonnet audit agents comparing entire codebase against v2 specs (ground truth). Created remediation plan and 26 beads issues with dependencies.

#### Key Result: ~25-30% overall spec compliance

#### Deliverables Created:
1. **20 audit reports** — `docs/audit/01_*.md` through `docs/audit/20_*.md` (512KB total)
2. **Remediation plan** — `docs/audit/REMEDIATION_PLAN.md` (8 phases, 45-67 days estimated)
3. **26 new beads issues** with dependency graph

#### Top 5 Findings:
1. Tolerances off by orders of magnitude (perturbation 10,000x wrong)
2. Core simplex algorithm only ~40% complete (missing BTRAN, Phase I/II, steepest edge)
3. Pricing is completely different architecture (0% compliance)
4. solve_lp.c is 1262-line monolith with 10 hallucinated functions
5. Variable status encoding fundamentally wrong (breaks simplex)

#### What Works (keep these):
- Basis/LU math (~70% compliant, best module)
- CSC/CSR sparse matrix format + SpMV
- Callback basics (~70%)
- MPS parser, test framework, build system

---

## Next Steps

### Immediate (Phase 0 — do first)
1. **Fix tolerance values** — `convexfeld-nso9` (P0) — perturbation, pivot, Markowitz constants
2. **Fix variable status encoding** — `convexfeld-clow` (P0) — enum → row indices + negative codes

### Then (Phase 1 — mechanical renames)
3. **Apply 16 function/struct renames** — `convexfeld-b7ow` (P1)
4. **Rename core structures** — `convexfeld-dv0k` (P1)

### Then (Phase 2-3 — structural)
5. **Decompose solve_lp.c** — `convexfeld-23p6` (blocked by renames)
6. **Align data structures** — multiple P2 issues (blocked by renames)

### Full dependency chain:
```
P0 tolerances + var status → P1 renames → P2 structs → P3 decompose solve_lp → P4 algorithms + pricing → P5 infrastructure → P6 modules → P7 signatures
```

Run `bd ready` to see unblocked work.

---

## File Locations

| Item | Path |
|------|------|
| Audit reports (20) | `docs/audit/01_*.md` through `20_*.md` |
| Remediation plan | `docs/audit/REMEDIATION_PLAN.md` |
| V2 specs (ground truth) | `docs/specs-v2/specs/` |
| V1 specs (archived, hallucinated) | `docs/specs-v1/` |
| FUNCTION_MAP | `docs/specs-v2/FUNCTION_MAP.md` |
