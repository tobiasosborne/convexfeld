Finding: The specs are NOT internally contradictory

  The report confused three distinct operations that all involve "bound flipping" but work differently:

  The Three Operations

  Operation: Non-basic bound flip
  Where Described: revised_simplex.md lines 161-165
  Row Negation?: No
  What Happens: A non-basic variable moves from lb to ub (or vice versa). Basic var values updated. No basis exchange.
  ────────────────────────────────────────
  Operation: BFRT long-step flip
  Where Described: harris_ratio_test.md lines 146-150, simplex_iteration.md line 85
  Row Negation?: Yes
  What Happens: During ratio test, a basic blocking variable flips to its opposite bound so the step can continue. Row coefficients, RHS, and activity bounds are all
    negated/swapped.
  ────────────────────────────────────────
  Operation: Variable fixing at bound
  Where Described: pivot_operations.md lines 214, 249
  Row Negation?: No
  What Happens: A variable is permanently fixed at a bound and removed from the active problem via GRBpivot_bound.

  Source Code Confirmation

  The analyzed source confirms row negation is real for the BFRT case. When a bound flip occurs on an equality constraint:

  1. All row coefficients in the CSR representation are negated
  2. The RHS value is negated
  3. The unbounded count arrays are swapped
  4. The activity bound arrays (min/max) are negated and swapped
  5. Constraint sense is changed from equality to less-than-or-equal

  Verdict

  The specs are consistent. Two of three docs (harris_ratio_test.md and simplex_iteration.md) describe the BFRT with row negation. The other two (revised_simplex.md
  and pivot_operations.md) describe different operations that happen to share the word "flip" but don't involve row negation. The report's claim of "internal
  contradiction" and "the spec is wrong" is incorrect — it conflated three separate mechanisms.

  No spec changes needed.