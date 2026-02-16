# Test Oracle Guidance

## Overview

This document provides actionable guidance for testing an LP solver reimplementation. It covers unit testing of individual components, integration testing against reference solvers, standard benchmark problems with known optimal values, numerical stability stress testing, and regression testing methodology.

The central challenge in testing an LP solver is that there is no single "correct" answer for most problems. Due to floating-point arithmetic and algorithmic differences, two correct solvers may return different optimal bases, different dual solutions, and slightly different objective values -- all of which are valid. Testing must therefore focus on **solution quality metrics** (feasibility residuals, optimality gaps, objective agreement) rather than bitwise output comparison.

This document is organized so that a developer new to LP solver testing can work through the sections in order, building confidence from small hand-crafted problems up to the full Netlib benchmark suite.

---

## A. Unit Testing Strategy

Unit tests verify that individual solver components produce correct results on small, analytically tractable problems. These tests should run in milliseconds and catch the most common implementation errors early.

### A.1 Matrix Operations

Test the sparse matrix infrastructure (CSC storage, column access, element retrieval) with small matrices where results can be verified by hand.

**Test cases:**
- Identity matrix (3x3, 5x5): verify column extraction returns unit vectors
- Single dense column: verify all elements retrieved in order
- Rectangular matrix (3x5): verify row counts, column counts, nonzero counts
- Empty columns: verify sparse representation handles zero columns correctly
- Matrix-vector product: for A = [[1,2],[3,4]] and x = [1,1], verify Ax = [3,7]

### A.2 Eta Vector and Basis Update Arithmetic

The Product Form of the Inverse (PFI) represents B^{-1} as a product of elementary matrices (eta vectors). Each eta vector is an identity matrix with one column replaced.

**Test cases:**
- Single eta vector: construct an eta vector for a known pivot, apply it to a vector, verify the result against hand computation
- Chain of two eta vectors: apply sequentially, verify the result matches B^{-1} * b computed directly from the 2x2 or 3x3 basis matrix
- Pivot element near zero rejection: verify that the solver rejects pivot elements below the minimum pivot threshold (approximately 1e-13)
- Eta accumulation: apply 10+ eta vectors to a known vector, verify the answer still agrees with a direct solve of the corresponding linear system to within 1e-10

### A.3 Pricing

Pricing selects the entering variable (non-basic variable with the most attractive reduced cost). For testing, construct a small problem where reduced costs can be computed by hand.

**Test case: 2-variable LP**

```
minimize -x1 - 2*x2
subject to:
  x1 + x2 <= 4
  x1      <= 3
       x2 <= 3
  x1, x2 >= 0
```

At the initial basis (all slacks basic), the reduced costs are c1 = -1 and c2 = -2. The pricing module should select x2 (most negative reduced cost in Dantzig pricing) or x2 (steepest edge will also prefer x2 here because the objective improvement rate is higher).

**Additional pricing tests:**
- All reduced costs non-negative (at optimality): verify pricing returns "no candidate"
- Exactly one negative reduced cost: verify that variable is selected
- Tied reduced costs: verify a candidate is selected (either is valid)
- Partial pricing: verify that candidates from the examined portion are correct, even if not globally best

### A.4 Ratio Test (Harris Two-Pass)

The ratio test selects the leaving variable. Given the entering column (updated by B^{-1}), compute the maximum step length before any basic variable violates a bound.

**Test case: simple step**

For the 2-variable LP above, when x2 enters the basis:
- Updated column (B^{-1} * a2) gives the rates of change for each basic variable
- The ratio test should identify the most restrictive bound, producing a step length of 3 (x2 goes from 0 to 3, hitting the x2 <= 3 bound)

**Harris relaxation test:**
- Construct a case where the strict ratio test would select a degenerate pivot (zero step length) but Harris relaxation (using the primal feasibility tolerance, approximately 1e-6) allows a non-degenerate pivot on a different row
- Verify the selected leaving variable differs from the strict test and the feasibility violation is within tolerance

**Edge cases:**
- Unbounded ray: all elements of the updated column are <= 0 (no leaving variable exists). Verify the solver reports unboundedness.
- All elements are zero: updated column is numerically zero. Verify the solver does not divide by zero and handles this gracefully.

### A.5 Edge Case Problems

Every LP solver must handle these degenerate or special cases correctly.

**Infeasible problem:**
```
minimize x1
subject to:
  x1 >= 2
  x1 <= 1
  x1 >= 0
```
Expected status: INFEASIBLE.

**Unbounded problem:**
```
minimize -x1
subject to:
  x1 - x2 <= 0
  x1, x2 >= 0
```
Expected status: UNBOUNDED (x1 and x2 can grow together without bound).

**Degenerate problem (many basic variables at bounds):**
```
minimize -x1 - x2
subject to:
  x1 + x2 <= 1
  x1      <= 1
       x2 <= 1
  x1, x2 >= 0
```
At the optimum (x1=0, x2=1) or (x1=1, x2=0) or (x1=1/2, x2=1/2), multiple constraints are active simultaneously. The solver should converge without cycling.

**Empty constraint matrix:**
```
minimize -3*x1 + 2*x2
subject to:
  0 <= x1 <= 5
  0 <= x2 <= 5
```
No constraints other than bounds. Optimal solution: x1=5, x2=0, obj=-15.

**Single variable:**
```
minimize x1
subject to:
  1 <= x1 <= 3
```
Optimal: x1=1, obj=1. Trivial but verifies the solver handles 1x1 systems.

**Fixed variables:**
```
minimize x1 + x2
subject to:
  x1 = 3
  x2 = 4
```
Optimal: x1=3, x2=4, obj=7. Verifies handling of equality constraints / fixed bounds.

---

## B. Integration Testing with Reference Solvers

Once unit tests pass, integration testing verifies that the complete solver produces correct results on real problems by comparing against known-good reference implementations.

### B.1 Recommended Reference Solvers

Use at least one of the following open-source LP solvers as a comparison oracle:

| Solver | Language | License | Install | Strengths |
|--------|----------|---------|---------|-----------|
| **HiGHS** | C++ | MIT | `pip install highspy` | Modern, fast, well-maintained. Best open-source LP solver as of 2025. Has Python bindings via highspy. Also available through SciPy (`scipy.optimize.linprog` with method='highs'). |
| **GLPK** | C | GPL-3 | `apt install glpk-utils` | Mature, widely available. Command-line tool `glpsol` reads MPS files directly. Slower than HiGHS but extremely well-tested. |
| **CLP** (COIN-OR) | C++ | EPL-2.0 | Build from source or `conda install -c conda-forge coin-or-clp` | Part of the COIN-OR suite. Strong simplex implementation. Good for validating simplex-specific behavior. |

**Recommendation:** Use HiGHS as the primary reference (best accuracy and speed) and GLPK as a secondary reference (independent codebase, different algorithms).

### B.2 Comparison Methodology

For each test problem, solve with both your implementation and the reference solver, then compare:

**1. Objective value comparison (primary check):**

```
relative_error = |obj_ours - obj_ref| / max(1.0, |obj_ref|)
```

Pass criterion: `relative_error < 1e-6`

The `max(1.0, |obj_ref|)` denominator prevents division by near-zero objectives from producing spuriously large relative errors. For problems with objective values near zero, use an absolute tolerance of 1e-8 instead.

**2. Solution status comparison:**
- Both solvers must agree on the problem status: OPTIMAL, INFEASIBLE, UNBOUNDED, or INF_OR_UNBD.
- Exception: some solvers report INF_OR_UNBD where others distinguish INFEASIBLE from UNBOUNDED. Treat INF_OR_UNBD as compatible with either.
- Exception: for problems near the boundary of feasibility, one solver may report OPTIMAL with a tiny constraint violation while another reports INFEASIBLE. Flag these for manual review.

**3. Iteration count comparison (informational, not pass/fail):**
- Iteration counts should be within the same order of magnitude.
- If your solver takes 10x more iterations than the reference on a particular problem, investigate -- this suggests an algorithmic issue (poor pricing, degenerate cycling, bad crash basis) rather than an incorrect answer.
- Iteration counts will differ between primal and dual simplex, and between different pricing strategies. Compare like-for-like when possible.

**4. Solution vector comparison (optional, for debugging):**
- LP solutions are generally not unique (multiple optimal bases may exist).
- Comparing solution vectors is useful for debugging but should NOT be a pass/fail criterion.
- When solutions differ, verify both are feasible and have the same objective value.

### B.3 Example Comparison Script Outline (Python + HiGHS)

```python
import highspy
import math

def compare_with_highs(mps_file, our_obj, our_status):
    """Compare our solver result against HiGHS on the same MPS file."""
    h = highspy.Highs()
    h.setOptionValue("output_flag", False)  # suppress output
    h.readModel(mps_file)
    h.run()

    info = h.getInfoValue("objective_function_value")
    highs_obj = info[1]
    status = h.getModelStatus()

    # Status comparison
    status_ok = True
    if status == highspy.kOptimal:
        if our_status != "OPTIMAL":
            status_ok = False
    elif status == highspy.kInfeasible:
        if our_status not in ("INFEASIBLE", "INF_OR_UNBD"):
            status_ok = False
    elif status == highspy.kUnbounded:
        if our_status not in ("UNBOUNDED", "INF_OR_UNBD"):
            status_ok = False

    # Objective comparison (only meaningful if both found optimal)
    obj_ok = True
    if our_status == "OPTIMAL" and status == highspy.kOptimal:
        denom = max(1.0, abs(highs_obj))
        rel_error = abs(our_obj - highs_obj) / denom
        obj_ok = (rel_error < 1e-6)

    return status_ok, obj_ok
```

### B.4 Example Comparison with GLPK (Command Line)

```bash
# Solve an MPS file with GLPK
glpsol --freemps problem.mps --output solution.txt

# The output file contains the objective value and solution status.
# Parse it and compare against your solver's output.
```

GLPK's output format includes lines like:
```
Status:     OPTIMAL
Objective:  obj = -4.6475314286e+02 (MINimum)
```

---

## C. Standard Benchmark Problems

The Netlib LP test set is the standard benchmark for LP solver validation. It contains approximately 90 problems drawn from real-world applications (airline scheduling, refinery operations, network planning, resource allocation). Every serious LP solver is validated against this collection.

### C.1 Recommended Test Progression

Work through these problems in order. Each level adds difficulty. Do not move to the next level until the current level passes cleanly.

#### Level 1: Trivial (sanity check)

| Problem | Rows | Cols | Nonzeros | Optimal Value | Notes |
|---------|------|------|----------|---------------|-------|
| AFIRO | 28 | 32 | 88 | -4.6475314286E+02 | Smallest Netlib problem. If this fails, something is fundamentally broken. |
| SC50A | 51 | 48 | 131 | -6.4575077059E+01 | Small, well-conditioned. |
| SC50B | 51 | 48 | 119 | -7.0000000000E+01 | Similar to SC50A. |
| KB2 | 44 | 41 | 291 | -1.7499001299E+03 | Small, tests basic numeric precision. |

Expected iteration count range: 10--50 iterations per problem.

#### Level 2: Small but nontrivial

| Problem | Rows | Cols | Nonzeros | Optimal Value | Notes |
|---------|------|------|----------|---------------|-------|
| ADLITTLE | 57 | 97 | 465 | 2.2549496316E+05 | First problem with positive optimal value. |
| BLEND | 75 | 83 | 521 | -3.0812149846E+01 | Standard small LP. |
| STOCFOR1 | 118 | 111 | 474 | -4.1131976219E+04 | Stochastic programming origin. |
| BRANDY | 221 | 249 | 2,150 | 1.5185098965E+03 | Moderately dense. |

Expected iteration count range: 50--300 iterations per problem.

#### Level 3: Medium difficulty

| Problem | Rows | Cols | Nonzeros | Optimal Value | Notes |
|---------|------|------|----------|---------------|-------|
| SC205 | 206 | 203 | 552 | -5.2202061212E+01 | Tests scaling with larger dimension. |
| SHARE2B | 97 | 162 | 777 | -4.1573224074E+02 | Tests numerical precision. |
| E226 | 224 | 282 | 2,767 | -1.8751929066E+01 | Numerically challenging. |
| ISRAEL | 175 | 142 | 2,358 | -8.9664482186E+05 | Dense columns, tests pivot stability. |
| AGG | 489 | 163 | 2,541 | -3.5991767287E+07 | Aggregation structure. |
| BANDM | 306 | 472 | 2,659 | -1.5862801845E+02 | Banded matrix structure. |

Expected iteration count range: 200--2,000 iterations per problem.

#### Level 4: Hard problems

| Problem | Rows | Cols | Nonzeros | Optimal Value | Notes |
|---------|------|------|----------|---------------|-------|
| PILOT4 | 411 | 1,000 | 5,145 | -2.5811392589E+03 | Pilot scheduling. Tests stability. |
| BORE3D | 234 | 315 | 1,525 | 1.3730803942E+03 | Ill-conditioned for some solvers. |
| CAPRI | 272 | 353 | 1,786 | 2.6900129138E+03 | Capital budgeting model. |
| 25FV47 | 822 | 1,571 | 11,127 | 5.5018458883E+03 | Large, tests overall robustness. |
| STAIR | 357 | 467 | 3,857 | -2.5126695119E+02 | Staircase structure. |

Expected iteration count range: 1,000--10,000 iterations per problem.

#### Level 5: Stress tests

| Problem | Rows | Cols | Nonzeros | Optimal Value | Notes |
|---------|------|------|----------|---------------|-------|
| PILOT87 | 2,031 | 4,883 | 73,804 | 3.0171034733E+02 | Notoriously difficult. Bad scaling. Many solvers struggle. |
| DFL001 | 6,072 | 12,230 | 41,873 | 1.1266396047E+07 | Very large. Stress test for memory and performance. |
| 80BAU3B | 2,263 | 9,799 | 29,063 | 9.8723216072E+05 | Large, dense. |
| D2Q06C | 2,172 | 5,167 | 35,674 | 1.2278421081E+05 | Network-origin, numerically difficult. |
| GREENBEA | 2,393 | 5,405 | 31,499 | -7.2555248130E+07 | Large, requires good scaling. |
| MAROS-R7 | 3,137 | 9,408 | 151,120 | 1.4971851665E+06 | Very dense. Extreme stress test. |

Expected iteration count range: 5,000--100,000+ iterations per problem.

### C.2 Obtaining Netlib Problems

The Netlib problems are available in MPS format from several sources:

1. **Official Netlib archive:** https://www.netlib.org/lp/data/ -- Files are in compressed MPS format using a special encoding. Use the `emps` utility (available in the same directory) to decompress to standard MPS.

2. **SuiteSparse Matrix Collection:** https://sparse.tamu.edu/LPnetlib -- Problems available in multiple formats including standard MPS.

3. **GitHub mirrors:** Search for "netlib LP test problems" on GitHub. Several repositories provide the problems pre-converted to free-format MPS. For example, the LP-Test-Problems repository by YimingYAN provides the problems in MATLAB MAT format with documented optimal values.

4. **Via COIN-OR:** The COIN-OR project distributes Netlib problems as part of its test infrastructure. The Data directory of the CLP repository contains MPS files.

### C.3 Passing Criteria for Netlib Problems

A Netlib problem is considered "passed" when all of the following hold:

1. **Status agreement:** The solver reports OPTIMAL (matching the known status for all standard Netlib problems -- none are infeasible or unbounded).

2. **Objective accuracy:** The relative objective error is below 1e-6 compared to the published optimal value:
   ```
   |obj_computed - obj_published| / max(1.0, |obj_published|) < 1e-6
   ```

3. **Primal feasibility:** The maximum constraint violation is below the primal feasibility tolerance (1e-6):
   ```
   max_i |a_i^T x - b_i|  (for equality constraints)
   max_i max(0, a_i^T x - b_i)  (for <= constraints)
   max_j max(0, l_j - x_j, x_j - u_j)  (for bound constraints)
   ```
   All of these must be less than 1e-6.

4. **No crashes, hangs, or excessive runtime:** The solver completes without error. A rough guideline: Level 1--3 problems should solve in under 1 second on modern hardware; Level 4 in under 10 seconds; Level 5 in under 60 seconds.

### C.4 Published Optimal Values Reference

The definitive reference for exact optimal values of all Netlib problems is:

> Koch, T. (2004). "The final NETLIB-LP results." *Operations Research Letters*, 32(2):138--142.

This paper reports optimal values computed with extended-precision arithmetic, eliminating any doubt about the target values. The values listed in Section C.1 above are drawn from this paper and from the archived values at the Netlib repository, cross-checked against results from the William Hager objective value comparison page at the University of Florida.

---

## D. Numerical Stability Testing

Numerical stability tests push the solver into regimes where floating-point arithmetic limitations become critical. These tests catch subtle bugs that do not manifest on well-conditioned problems.

### D.1 Klee-Minty Cube Variants

The Klee-Minty cube (Klee and Minty, 1972) is a family of LP problems where the simplex method with Dantzig's most-negative-reduced-cost rule visits all 2^n vertices of an n-dimensional cube before finding the optimum. It is the standard worst-case test for simplex pivot rules.

**Construction for dimension n:**

```
maximize  sum_{j=1}^{n}  2^{n-j} * x_j

subject to:
  2 * sum_{i=1}^{j-1} 2^{j-i} * x_i + x_j  <=  5^j    for j = 1, ..., n
  x_j >= 0                                               for j = 1, ..., n
```

**Test protocol:**
1. Generate Klee-Minty cubes for n = 3, 4, 5, 6, 8, 10.
2. Solve each with your solver. Verify the optimal objective value: it is 5^n (all variables at their upper bounds in the optimal solution).
3. Record the iteration count. For n=3, Dantzig pricing requires 2^3 - 1 = 7 iterations (visiting all vertices). For n=10, naive Dantzig pricing requires 1023 iterations.
4. Verify that steepest-edge or Devex pricing solves these problems in far fewer iterations than the theoretical worst case (typically O(n) or O(n^2) iterations).
5. Verify that the solver completes without numerical issues for n up to at least 10.

**What this tests:** Pricing rule effectiveness, anti-cycling mechanisms, numerical stability with exponentially growing coefficients (5^n grows rapidly).

### D.2 Anti-Cycling Verification

Cycling occurs when the simplex method visits the same basis repeatedly, making zero progress. Construct problems that are provably degenerate (multiple basic variables at their bounds) to verify that the anti-cycling mechanism works.

**Beale's cycling example (1955):**

```
minimize  -0.75*x4 + 150*x5 - 0.02*x6 + 6*x7

subject to:
  0.25*x4 - 8*x5 - x6 + 9*x7       = 0
  0.5*x4  - 12*x5 - 0.5*x6 + 12*x7 = 0
  x3                                  = 1
  x1, ..., x7 >= 0
```

This problem was historically shown to cause cycling with certain pivot rules. The optimal objective value is -0.375 (at x4 = 0.5, x5 = 0, x6 = 0, x7 = 0, x3 = 1).

**Test protocol:**
1. Solve the problem. Verify optimal value = -0.375.
2. Set an iteration limit (e.g., 1000). If the solver does not terminate within this limit, the anti-cycling mechanism is failing.
3. Monitor whether the perturbation mechanism activates. A well-implemented solver will detect stalling after a few iterations and trigger bound perturbation (the EXPAND procedure).

**Additional degenerate problems:**
- Create a problem with many equality constraints: these force all corresponding basic variables to specific values, maximizing degeneracy.
- Create a problem where 90% of basic variables are at bounds. This simulates the degeneracy levels found in real-world problems (transportation and network problems are notoriously degenerate).

### D.3 Refactorization Stability

The PFI (Product Form of the Inverse) accumulates numerical error as eta vectors are appended. After many pivots without refactorization, the accumulated error can corrupt the solution.

**Test protocol:**
1. Take a medium-sized problem (e.g., BLEND or ADLITTLE from Netlib).
2. Set the refactorization interval very high (e.g., 10000) so that no refactorization occurs during the solve.
3. Solve and record the solution.
4. Set the refactorization interval to a normal value (e.g., 50--100).
5. Solve again and record the solution.
6. Compare the two solutions:
   - Objective values should agree within 1e-6.
   - Primal feasibility residuals with the long eta chain should be noticeably larger than with normal refactorization.
   - If the long-chain solution has feasibility violations above 1e-4 or objective disagreement above 1e-4, the solver's eta arithmetic may have stability issues.

**What this tests:** Accumulated floating-point error in the basis representation. In a healthy implementation, the solution should remain acceptable (within 1e-4 of optimal) even without refactorization on small-to-medium problems. Frequent refactorization (every 50--100 iterations) should keep residuals well below 1e-10.

### D.4 Ill-Conditioned Problems

Ill-conditioned problems have basis matrices with large condition numbers, causing amplification of rounding errors.

**Hilbert matrix LP:**

Construct an LP whose constraint matrix is a Hilbert matrix: A_{ij} = 1/(i+j-1). Hilbert matrices are famously ill-conditioned (condition number grows exponentially with dimension).

```
minimize  sum(x_j)
subject to:
  H * x = b     (where H is n x n Hilbert matrix, b = H * ones(n))
  x >= 0
```

The known optimal solution is x = ones(n) with objective = n.

**Test for n = 5, 8, 10, 12:**
- For n=5, any reasonable solver should find the correct answer.
- For n=8--10, the condition number exceeds 10^10 and solvers may struggle.
- For n=12, the condition number exceeds 10^16 (beyond double precision). The solver should either return NUMERIC status or an answer with visible feasibility violations. It should NOT return OPTIMAL with a completely wrong answer.

**What this tests:** The solver's ability to detect and report numerical difficulty rather than silently returning garbage.

### D.5 Near-Infeasible and Near-Unbounded Problems

Problems at the boundary of feasibility stress-test tolerance handling.

**Near-infeasible:**
```
minimize x1
subject to:
  x1 >= 1 + epsilon
  x1 <= 1 - epsilon
  x1 >= 0
```

For epsilon = 1e-3: clearly infeasible, solver should report INFEASIBLE.
For epsilon = 1e-7: near the feasibility tolerance boundary (1e-6). Solver may report INFEASIBLE or OPTIMAL depending on tolerance handling.
For epsilon = 1e-10: within feasibility tolerance. Solver will likely treat this as feasible (effectively x1 = 1).

**Test protocol:** Run with epsilon values of 1e-3, 1e-5, 1e-7, 1e-9, and 1e-11. Verify:
- Large epsilon: always reports INFEASIBLE.
- Small epsilon (below feasibility tolerance): reports OPTIMAL.
- Near-boundary: either result is acceptable, but the solver must not crash or hang.

**Near-unbounded:**
```
minimize -x1
subject to:
  x1 <= M
  x1 >= 0
```

For M = 1e6: bounded, optimal x1 = 1e6.
For M = 1e30: very large bound, should still be bounded but may cause numerical issues.
For M = 1e100: at or beyond the solver infinity threshold (typically 1e100). Solver should treat x1 as unbounded.

---

## E. Regression Testing Guidance

Regression testing ensures that solver improvements do not break previously working functionality. An LP solver has many interacting components, and a change to one (e.g., pricing) can cause regressions in another (e.g., cycling on degenerate problems).

### E.1 Solution Archive

Maintain a file of known-good solutions for regression testing. For each test problem, store:

```json
{
    "problem": "afiro",
    "mps_file": "test_data/netlib/afiro.mps",
    "expected_status": "OPTIMAL",
    "expected_objective": -4.6475314286e+02,
    "objective_tolerance": 1e-6,
    "max_iterations": 100,
    "max_time_seconds": 1.0,
    "primal_feasibility_bound": 1e-6,
    "known_good_date": "2026-02-15"
}
```

### E.2 Tracked Metrics

For each test run, record the following metrics and compare against the previous run:

**Hard pass/fail metrics (any regression is a bug):**
- Solution status
- Objective value within tolerance
- Primal feasibility residual within tolerance
- No crashes, assertion failures, or hangs

**Soft metrics (changes are informational, not automatic failures):**
- Iteration count: flag if it changes by more than 20% from baseline. Sudden jumps indicate algorithmic regressions. Sudden drops may indicate improvements or may indicate the solver is terminating early.
- Wall-clock time: flag if it increases by more than 50% from baseline (on the same hardware).
- Peak memory usage: flag if it increases by more than 25% from baseline.

### E.3 Residual Norms

After every solve, compute and log the following residual norms. These are the ground truth for solution quality.

**Primal residual (constraint satisfaction):**

```
r_primal = max over all constraints i of:
  |a_i^T x - b_i|           for equality constraints
  max(0, a_i^T x - b_i)     for <= constraints
  max(0, b_i - a_i^T x)     for >= constraints
```

Acceptable: r_primal < 1e-6 (the primal feasibility tolerance).
Warning: 1e-6 < r_primal < 1e-4.
Failure: r_primal > 1e-4.

**Dual residual (optimality condition satisfaction):**

For a minimization problem in standard form, the dual residual measures violation of the condition c - A^T y - s = 0 where y are dual variables and s are dual slacks:

```
r_dual = ||c - A^T y - s||_inf
```

Acceptable: r_dual < 1e-6 (the dual feasibility / optimality tolerance).
Warning: 1e-6 < r_dual < 1e-4.
Failure: r_dual > 1e-4.

**Complementary slackness residual:**

For each variable j:
```
cs_j = |x_j * s_j|
```
where x_j is the primal value and s_j is the corresponding dual slack. At optimality, complementary slackness requires cs_j = 0 for all j.

Acceptable: max_j cs_j < 1e-6.

**Bound residual:**

```
r_bound = max over all variables j of:
  max(0, l_j - x_j)    (lower bound violation)
  max(0, x_j - u_j)    (upper bound violation)
```

Acceptable: r_bound < 1e-6.

### E.4 Iteration Count Monitoring

Track iteration counts as a time series across solver revisions. Plot them for each benchmark problem.

**Patterns that indicate problems:**
- **Sudden spike** (e.g., from 200 to 2000 iterations on the same problem): likely a regression in pricing, ratio test, or anti-cycling.
- **Gradual upward trend** (e.g., 200 -> 220 -> 250 -> 300 over multiple revisions): possible slow degradation of numerical stability or accumulation of minor regressions.
- **Sudden drop** (e.g., from 500 to 50 iterations): verify the solution is still correct -- the solver may be terminating early due to a bug rather than finding a shortcut.
- **High variance between runs** (different results each time): if the solver uses any source of non-determinism (threading, memory addresses in pointer comparisons), this needs investigation.

### E.5 Regression Test Tiers

Organize tests into tiers by execution time:

| Tier | Problems | Run When | Expected Time |
|------|----------|----------|---------------|
| **Smoke** | 5 hand-crafted + AFIRO + SC50A | Every commit | < 5 seconds |
| **Standard** | All Level 1--3 Netlib + edge cases | Every PR / nightly | < 60 seconds |
| **Full** | All Level 1--5 Netlib + Klee-Minty + stability tests | Weekly / before release | < 30 minutes |
| **Extended** | Full + ill-conditioned + near-boundary + parameter sweeps | Before major release | < 2 hours |

### E.6 Parameter Sensitivity Testing

For the Full and Extended tiers, also sweep solver parameters to verify robustness:

- **Pricing strategy:** test with Dantzig, steepest edge, Devex, and partial pricing. All should converge to the same objective on well-conditioned problems.
- **Scaling mode:** test with no scaling, standard equilibration, and aggressive scaling. The scaled solve should produce the same objective as the unscaled solve (within tolerance), but may differ in iteration count.
- **Feasibility tolerance:** test with 1e-6 (default), 1e-8 (tight), and 1e-4 (loose). Solutions from tight tolerance should satisfy constraints more precisely, but all should agree on objective value.
- **Refactorization interval:** test with intervals of 20, 50, 100, and 500. All should produce the same objective; iteration counts may differ slightly due to accumulated numerical drift.

---

## F. Problem Format Reference

Test problems are typically stored in MPS (Mathematical Programming System) format, the de facto standard for LP problems.

### F.1 MPS Format Overview

MPS is a column-oriented fixed-format text file with the following sections:

```
NAME          problem_name
ROWS
 N  obj           (objective row, N = no constraint)
 L  c1            (L = less-than-or-equal)
 G  c2            (G = greater-than-or-equal)
 E  c3            (E = equality)
COLUMNS
    x1  obj   -1.0   c1   1.0
    x1  c2    2.0
    x2  obj   -2.0   c1   1.0
RHS
    rhs   c1   4.0   c2   6.0
    rhs   c3   1.0
BOUNDS
 LO bnd   x1   0.0
 UP bnd   x1   10.0
 FX bnd   x3   5.0        (FX = fixed)
 FR bnd   x4              (FR = free, -inf to +inf)
ENDATA
```

**Key points:**
- Fields are in fixed column positions (classic MPS) or whitespace-delimited (free MPS).
- The ROWS section defines constraint types. Exactly one row should be type N (the objective).
- The COLUMNS section defines the constraint matrix by column (variable). Each line provides a variable name, a row name, and a coefficient value. Two entries per line are allowed.
- The RHS section provides right-hand-side values for constraints.
- The BOUNDS section defines variable bounds. Default bounds are 0 <= x < +infinity.
- The optimization direction (minimize/maximize) is NOT specified in classic MPS. Most solvers default to minimization; check the convention of your solver and any comparison solvers.

### F.2 LP Format

The CPLEX LP format is a human-readable alternative to MPS, useful for small hand-crafted test problems:

```
minimize
  obj: -x1 - 2 x2
subject to
  c1: x1 + x2 <= 4
  c2: x1 <= 3
  c3: x2 <= 3
bounds
  0 <= x1
  0 <= x2
end
```

Both HiGHS and GLPK can read LP format files.

---

## G. Diagnostic Checklist

When a test fails, work through this checklist in order:

1. **Check the problem data.** Re-read the MPS file. Verify row/column counts match expectations. Verify the objective sense (min vs. max) matches.

2. **Check the reference value.** Cross-check the published optimal value against a second source. Verify the reference solver also gets this value.

3. **Check feasibility residuals.** Compute ||Ax - b||_inf for the returned solution. If this is large (> 1e-4), the solution is not feasible -- the issue is in the simplex iteration, not in optimality checking.

4. **Check for status disagreement.** If the solver reports INFEASIBLE but the problem is known to be feasible, check: is the crash basis causing a Phase I failure? Is the feasibility tolerance too tight? Is there a sign error in the constraint loading?

5. **Check iteration count.** If the solver hit the iteration limit, it may be cycling. Enable stalling detection logging and examine the basis snapshots.

6. **Check for numerical warnings.** Examine the coefficient range of the constraint matrix. If the ratio of the largest to smallest nonzero coefficient exceeds 1e8, scaling issues are likely. Enable aggressive scaling and re-test.

7. **Isolate the component.** If the full solver fails, reduce the problem to the smallest failing case and run the individual components (pricing, ratio test, basis update) with logging to find the first point of divergence.

8. **Compare basis sequences.** Log the entering/leaving variable at each pivot. Compare against the reference solver's pivot log (GLPK can output this with `--log` option). Find the first pivot where the sequences diverge.

---

## References

- Beale, E.M.L. (1955). "Cycling in the dual simplex algorithm." *Naval Research Logistics Quarterly*, 2(4):269--275.
- Dantzig, G.B. (1963). *Linear Programming and Extensions.* Princeton University Press.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1--3):437--474.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1--28.
- Klee, V. and Minty, G.J. (1972). "How good is the simplex algorithm?" In *Inequalities III*, ed. O. Shisha, 159--175. Academic Press.
- Koch, T. (2004). "The final NETLIB-LP results." *Operations Research Letters*, 32(2):138--142.
- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer.
- Netlib LP test problem collection: https://www.netlib.org/lp/data/
- HiGHS optimization solver: https://highs.dev/ (MIT license)
- GLPK (GNU Linear Programming Kit): https://www.gnu.org/software/glpk/ (GPL-3)
- COIN-OR CLP: https://github.com/coin-or/Clp (EPL-2.0)
- Hans Mittelmann's LP benchmark page: https://plato.asu.edu/bench.html
- SuiteSparse Matrix Collection (Netlib LP problems): https://sparse.tamu.edu/LPnetlib

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All test problems use published benchmarks or hand-crafted examples
[x] All reference solvers are open-source with documented licenses
[x] All optimal values cite published sources
[x] Passes the Clean Room Test: this advice could come from any LP textbook
```
