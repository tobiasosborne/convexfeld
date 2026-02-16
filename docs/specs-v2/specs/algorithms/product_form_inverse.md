# Product Form of the Inverse (PFI) Basis Update

## Published Reference

- **Primary:** Dantzig, G.B. and Orchard-Hays, W. (1954). "The Product Form for the Inverse in the Simplex Method." *Mathematical Programming Study*, RAND Report P-440. Reprinted in Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press, Chapter 11.
- **LU factorization for initial basis:** Bartels, R.H. and Golub, G.H. (1969). "The simplex method of linear programming using LU decomposition." *Communications of the ACM*, 12(5):266-268.
- **Sparse triangular update:** Forrest, J.J.H. and Tomlin, J.A. (1972). "Updated triangular factors of the basis to maintain sparsity in the product form simplex method." *Mathematical Programming*, 2(1):263-278.
- **Sparse LU refinements:** Reid, J.K. (1982). "A sparsity-exploiting variant of the Bartels-Golub decomposition for linear programming bases." *Mathematical Programming*, 24(1):55-69.
- **Large-scale sparse factorization:** Suhl, U.H. and Suhl, L.M. (1990). "Computing sparse LU factorizations for large-scale linear programming bases." *ORSA Journal on Computing*, 2(4):325-335.
- **Textbook treatment:** Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer, Chapters 5 and 9.

The technique implemented here is the classical product form of the inverse augmented with periodic LU refactorization of the basis matrix. Eta vectors are accumulated during simplex iterations to represent incremental basis changes, and a full LU factorization is recomputed when the eta chain grows too long or numerical accuracy degrades.

## Purpose

In the revised simplex method, every iteration requires solving two linear systems involving the basis matrix B:

- **FTRAN** (forward transformation): Solve Bx = a for x, where a is the column of the entering variable.
- **BTRAN** (backward transformation): Solve yB = c_B for y, where c_B is the vector of basic variable costs.

Maintaining an explicit dense inverse of B is prohibitively expensive for large sparse problems, both in memory (O(m^2) storage for m constraints) and in computation (O(m^3) per update). The product form of the inverse avoids this by representing B^{-1} implicitly as a product of elementary matrices, each differing from the identity in only one column. Each simplex pivot appends exactly one such elementary matrix (called an "eta matrix") to the product. FTRAN and BTRAN are then performed by applying these elementary matrices sequentially.

The PFI method sits at the core of the simplex solver, mediating between the pricing system, the ratio test, and the pivot operations. It determines how efficiently the solver can perform each iteration, and its numerical health directly affects solution quality.

## Inputs

### For Eta Vector Creation (at each pivot)

- **Pivot column** a_q: the column of the constraint matrix corresponding to the entering variable q, represented in sparse format (indices and values of nonzero entries).
- **Pivot row index** p: the row of the leaving variable.
- **Pivot element** alpha_{pq}: the value A[p, q] after transformation, which must be nonzero.
- **Entering variable index** q and **leaving variable index**.
- **Current basis header**: the mapping from each constraint row to its basic variable.

### For FTRAN

- **Right-hand side vector** a (sparse): the vector to be transformed by B^{-1}.
- **Current LU factorization** of the initial basis B_0.
- **Eta vector chain**: the ordered sequence of eta vectors accumulated since the last refactorization.

### For BTRAN

- **Row vector** c (sparse): the vector to be transformed by B^{-T}.
- **Current LU factorization** of B_0 (transpose access).
- **Eta vector chain**: same as FTRAN, traversed in reverse order.

### Preconditions

1. The basis matrix B is nonsingular (guaranteed by the simplex algorithm's pivot selection).
2. The pivot element alpha_{pq} is nonzero and not too small (above a numerical tolerance).
3. The LU factorization of B_0 is available and numerically valid.
4. The eta vector chain is well-formed (a null-terminated linked list with a consistent count).

## Outputs

### From Eta Vector Creation

- A new eta vector appended to the front of the eta chain, representing the elementary basis change from the current pivot.
- Updated eta vector count (total count and row-type count).

### From FTRAN

- The transformed vector x = B^{-1} a, stored in-place or in a result array.

### From BTRAN

- The transformed vector y = c^T B^{-1} (equivalently, y^T = B^{-T} c), stored in-place or in a result array.

### From Refactorization

- A fresh LU factorization of the current basis matrix B.
- The eta vector chain is cleared (reset to empty).
- Eta vector counts reset to zero.

### Postconditions

1. After eta creation: the eta chain represents the complete sequence of basis changes since the last refactorization.
2. After FTRAN: x satisfies Bx = a to within numerical tolerance.
3. After BTRAN: y satisfies yB = c to within numerical tolerance.
4. After refactorization: the product B_0^{-1} with zero eta vectors yields the same result as the previous full chain.

## Algorithm Description

### Overview

The basis matrix B of an LP with m constraints is an m-by-m nonsingular matrix formed by selecting m columns from the constraint matrix A. At the start of the simplex solve (or after a refactorization), B is factorized into LU form: B_0 = LU, where L is lower triangular and U is upper triangular. This factorization serves as the "initial basis inverse."

Each simplex pivot replaces one column of B. Rather than refactorizing from scratch, the PFI method records the change as an "eta vector" -- the transformed pivot column with entries divided by the pivot element. The k-th eta vector defines an elementary matrix E_k that differs from the identity matrix in column p_k (the pivot row). After k pivots, the current basis inverse is:

    B_k^{-1} = E_k^{-1} * E_{k-1}^{-1} * ... * E_1^{-1} * B_0^{-1}

where B_0^{-1} is applied via the LU factorization (solving LUx = b by forward and back substitution), and each E_i^{-1} is applied by a simple column operation.

When the eta chain grows too long (degrading performance) or numerical accuracy drops below acceptable levels, the solver performs a **refactorization**: it computes a fresh LU decomposition of the current basis B_k and clears all eta vectors.

### Detailed Steps

#### Step 1: Initial Basis Factorization

At the beginning of the simplex solve, compute the LU factorization of the initial basis matrix B_0:

    B_0 = L * U

where L is unit lower triangular and U is upper triangular. This factorization is computed using sparse Gaussian elimination with partial pivoting for numerical stability (Bartels and Golub, 1969). Sparse fill-reducing orderings (such as Markowitz ordering) are applied to preserve sparsity in the factors (Suhl and Suhl, 1990).

Initialize the eta vector chain to empty (null head pointer, zero count).

#### Step 2: Eta Vector Creation (at each pivot)

When variable q enters the basis replacing the variable basic in row p, compute the eta vector as follows:

1. **Obtain the transformed pivot column.** The entering column a_q has already been transformed by FTRAN to produce the updated column d = B^{-1} a_q. The pivot element is alpha = d_p.

2. **Construct the eta vector.** The eta vector eta has m components defined by:

       eta_i = -d_i / alpha,  for i != p
       eta_p = 1 / alpha

   In practice, only the nonzero entries are stored. The eta vector is recorded in sparse format as a list of (index, value) pairs, plus the pivot row index p.

3. **Determine storage mode.** Two storage modes are supported:

   - **Full mode:** The eta vector stores the complete sparse representation including indices of all nonzero entries and the associated column data from the constraint matrix. This supports both FTRAN and BTRAN operations and is required for operations such as crossover and basis warm-starting.

   - **Compact mode:** A minimal record storing only the variable index and the value at which it was fixed. This mode is used for variable-fixing operations where a variable is set to one of its bounds and removed from the basis. These records are smaller and faster to process.

4. **Allocate memory from the eta pool.** Eta vectors are allocated from a region-based memory pool using a bump-allocation strategy (see Memory Management below). The allocation size depends on the storage mode and the number of nonzero entries.

5. **Link the new eta vector** at the head of the eta chain (prepend to the singly-linked list).

6. **Increment counters.** Both the total eta count and the row-type eta count are incremented.

#### Step 3: FTRAN (Forward Transformation)

To solve Bx = a (computing x = B^{-1} a):

1. **Apply the initial factorization.** Solve B_0 x_0 = a using the LU factors:
   - Forward substitution: solve Lw = a for w.
   - Back substitution: solve Ux_0 = w for x_0.

   Both substitutions exploit sparsity by skipping zero entries.

2. **Apply eta vectors in forward order** (from oldest to newest, i.e., E_1 through E_k). For each eta vector E_i with pivot row p_i:

       Let tau = x_{i-1, p_i}   (the current value at the pivot position)
       For each nonzero entry (j, eta_j) in the eta vector where j != p_i:
           x_{i, j} = x_{i-1, j} + eta_j * tau
       x_{i, p_i} = eta_{p_i} * tau   (= tau / alpha_i)

   The final result x_k = B_k^{-1} a.

3. **Sparse FTRAN optimization.** If tau = 0 for a given eta vector (the value at the pivot position is zero), the entire eta vector application is skipped. This is particularly effective when the right-hand side a is sparse (as described by Hall, 2005, for hyper-sparse systems).

#### Step 4: BTRAN (Backward Transformation)

To solve yB = c (computing y = c^T B^{-1}, equivalently y^T = B^{-T} c):

1. **Apply eta vectors in reverse order** (from newest to oldest, E_k through E_1). For each eta vector E_i with pivot row p_i:

       Let sigma = sum over nonzero entries (j, eta_j) in E_i of y_{i, j} * eta_j
       Note: this sum includes the pivot position p_i.
       y_{i-1, p_i} = sigma
       For each nonzero entry (j, eta_j) where j != p_i:
           y_{i-1, j} = y_{i, j}   (unchanged)

   More precisely, the transformation for BTRAN at eta vector i is:

       sigma = y_{p_i} * eta_{p_i} + sum_{j != p_i, eta_j != 0} y_j * eta_j
       y_{p_i} = sigma

   All other entries of y remain unchanged.

2. **Apply the initial factorization transpose.** After processing all eta vectors, solve B_0^T y_0 = y_remaining:
   - Back substitution with U^T: solve U^T w = y for w.
   - Forward substitution with L^T: solve L^T y_0 = w for y_0.

3. **Sparse BTRAN optimization.** If y_{p_i} = 0 and all entries of y at positions corresponding to nonzero entries in the eta vector are zero, the entire application is skipped.

#### Step 5: Variable-Fixing Eta Records

In addition to pivot eta vectors, the system records **variable-fixing** operations as a distinct type of eta record. When a variable is identified as being at one of its bounds and can be fixed:

1. A compact or full eta record is created (depending on the storage mode flag).
2. The record stores the variable index, the value at which it is fixed, its previous reduced cost, and its bound status (at lower bound, at upper bound, fixed, or superbasic).
3. In full mode, the record also stores the column of the constraint matrix corresponding to the fixed variable, filtered to include only rows with active basic variables.
4. The objective function is updated to account for the fixed variable's contribution: objective += (reduced cost) * (fixed value).
5. The variable's reduced cost is set to zero, and it is removed from active consideration.

These records enable the solver to undo fixing operations if needed (e.g., during warm-start or crossover), by replaying the eta chain in reverse.

#### Step 6: Refactorization

Refactorization replaces the entire accumulated product form with a fresh LU factorization of the current basis:

1. **Assemble the current basis matrix B_k** by selecting the m columns from the constraint matrix A corresponding to the current basic variables (as identified by the basis header).

2. **Compute the LU factorization** B_k = L_new * U_new using sparse Gaussian elimination. Threshold partial pivoting is used to balance numerical stability with sparsity preservation (Reid, 1982). Markowitz-type ordering heuristics minimize fill-in.

3. **Clear the eta chain.** Set the eta list head to null and reset all eta counts to zero.

4. **Free eta memory.** The entire eta memory pool is released at once (region-based deallocation). No individual eta vectors need to be freed separately.

5. **Update B_0** to be B_k. All subsequent FTRAN/BTRAN operations use the new LU factors with zero initial eta vectors.

### Key Design Choices

- **Linked list for eta storage (prepend at head):** New eta vectors are linked at the head of the list. This makes BTRAN natural (traverse from head = newest to tail = oldest), while FTRAN requires traversing to the tail first and working back, or equivalently, collecting the list into an array for forward traversal. The choice of head-insertion is standard and minimizes insertion cost to O(1).

- **Two eta vector types:** Pivot transformations (which record a full column update with negated and scaled coefficients) are stored differently from variable-fixing records (which store minimal information about a variable being set to a bound). This distinction reduces memory usage for the common case of fixing variables during presolve or bound tightening.

- **Region-based memory allocation for eta vectors:** Rather than allocating each eta vector individually from the system heap, all eta vectors are allocated from a contiguous memory pool using a bump allocator. This eliminates per-allocation overhead and fragmentation. The entire pool is freed in bulk during refactorization or cleanup. Chunks in the pool grow exponentially (doubling up to a cap) to reduce the number of system allocations over the life of the solve.

- **Sparse storage of eta vectors:** Only the nonzero entries of each eta vector are stored, as (index, value) pairs. For the pivot column representation, entries are filtered to include only those corresponding to active basic variables. This keeps storage proportional to the number of nonzeros rather than the problem dimension m.

- **Dual-format extraction:** Pivot eta vectors may store data extracted from both the row-major (CSR) and column-major (CSC) representations of the constraint matrix. The row-major data provides the eta entries for FTRAN (iterating over the columns in a given row), while the column-major data provides an alternative view needed by the dual simplex method (iterating over the rows in a given column). Whether column data is included depends on the simplex variant in use.

## Numerical Considerations

### Tolerances

1. **Pivot tolerance:** The pivot element alpha_{pq} must satisfy |alpha_{pq}| > epsilon_pivot, where epsilon_pivot is a small positive value (typically around 1e-10 to 1e-12). If the pivot element is too small, the division in the eta vector computation amplifies rounding errors.

2. **Feasibility tolerance:** After FTRAN, the solution x = B^{-1}a must satisfy the constraint bounds to within the feasibility tolerance (typically 1e-6). Accumulated eta vector errors can cause the computed solution to drift outside these bounds.

3. **Optimality tolerance:** After BTRAN, the computed reduced costs must be accurate enough to correctly identify improving variables. Errors exceeding the optimality tolerance (typically 1e-6) can cause the algorithm to miss the optimal solution or cycle.

### Numerical Degradation

As eta vectors accumulate, the product form representation suffers from two sources of degradation:

1. **Rounding error accumulation:** Each eta vector application involves floating-point arithmetic that introduces small errors. After k eta vectors, the accumulated error grows roughly as O(k * epsilon_machine), where epsilon_machine is approximately 2.2e-16 for IEEE 754 double precision. In practice, error growth can be worse due to ill-conditioned pivots.

2. **Fill-in growth:** Even when individual eta vectors are sparse, the product of many sparse matrices can produce a dense result. After k pivots, the effective density of B^{-1} may increase, making each subsequent FTRAN/BTRAN more expensive.

### Stability Monitoring

The solver monitors numerical accuracy by:

- Checking the residual ||Bx - a|| after FTRAN operations. If this exceeds a threshold, a refactorization is triggered.
- Detecting large reduced cost changes that are inconsistent with the current basis. Sudden jumps in the objective value or infeasibility measures may indicate numerical problems.
- Tracking the condition of recent pivot elements. A sequence of small pivots signals potential ill-conditioning.

## Termination

### Refactorization Triggers

The PFI method does not have a standalone termination condition; rather, the key control decision is **when to refactorize**. Refactorization is triggered by any of the following conditions:

1. **Eta count threshold:** When the total number of accumulated eta vectors exceeds a threshold, typically proportional to the problem size. Common choices are between 50 and 200 eta vectors, or a fraction of m (the number of constraints). The threshold balances the cost of refactorization (O(m^2) to O(m^3) depending on sparsity) against the growing per-iteration cost of applying the eta chain.

2. **Numerical accuracy degradation:** When the residual of an FTRAN or BTRAN operation exceeds a tolerance, indicating that rounding errors in the product form have become unacceptable. This is the most robust trigger and catches cases where the eta count threshold alone is insufficient.

3. **Fill-in growth:** When the total number of nonzero entries across all eta vectors exceeds a storage threshold, indicating that the sparse advantage is being lost. This prevents excessive memory consumption and slow eta applications.

4. **Explicit request:** Certain solver operations (e.g., transitioning from Phase I to Phase II, or starting crossover) may force a refactorization to ensure a clean numerical state.

### Simplex Termination

The PFI system operates within the simplex iteration loop. The simplex algorithm terminates when:

- **Optimality:** All reduced costs satisfy the optimality conditions (no improving pivot exists).
- **Infeasibility:** Phase I fails to find a feasible solution.
- **Unboundedness:** The ratio test finds no finite step length.
- **Iteration limit:** The maximum iteration count is reached.

At termination, the eta chain (and the underlying memory pool) are freed during simplex cleanup.

## Complexity

### Per-Iteration Costs

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| Eta vector creation | O(nnz_col) | Proportional to nonzeros in pivot column |
| FTRAN (after k updates) | O(nnz(L) + nnz(U) + sum_{i=1}^{k} nnz(eta_i)) | LU solve plus k eta applications |
| BTRAN (after k updates) | O(nnz(L) + nnz(U) + sum_{i=1}^{k} nnz(eta_i)) | Same as FTRAN, reverse order |
| Refactorization | O(nnz(B) + fill-in) | Sparse LU with pivoting |

### Space Complexity

| Component | Space | Notes |
|-----------|-------|-------|
| LU factors of B_0 | O(nnz(L) + nnz(U)) | Typically 2-10x nnz(B) due to fill-in |
| k eta vectors | O(sum of nnz(eta_i)) | Each eta has nnz proportional to column density |
| Eta memory pool | O(total eta bytes) | Region-based, freed in bulk |

### Growth Characteristics

Without refactorization, the cost of FTRAN and BTRAN grows linearly with the number of pivots k. With refactorization every T pivots, the amortized cost per iteration is:

    O(nnz(LU-solve) + T * avg_nnz(eta))

where T is the refactorization interval. The optimal T balances refactorization cost against the growing FTRAN/BTRAN cost.

## Edge Cases

### Empty or Trivial Basis

If the problem has zero constraints (m = 0), no basis factorization is needed and no eta vectors are created. The PFI system is not initialized.

### Singleton Columns

If the entering column has only one nonzero entry (a singleton), the eta vector is trivially the scalar 1/alpha at position p. These eta vectors are extremely cheap to apply and do not contribute significantly to fill-in.

### Nearly Singular Basis

If the pivot element alpha_{pq} is very small (close to machine epsilon), the eta vector entries -d_i / alpha become very large, amplifying rounding errors. The simplex algorithm's pivot selection should avoid such pivots via threshold pivoting. If an unavoidable small pivot occurs, an immediate refactorization should follow.

### Degenerate Pivots

A degenerate pivot (where the step length is zero, meaning the entering variable does not change value) still produces an eta vector. The basis changes but the solution point does not move. These pivots contribute to eta chain growth without improving the objective, making the refactorization threshold particularly important on highly degenerate problems.

### Memory Exhaustion

If the eta memory pool cannot allocate space for a new eta vector (out of memory), the system returns an error code. The caller should either trigger an immediate refactorization (to clear the pool and try again) or terminate the solve with an out-of-memory status.

### Warm Start After Refactorization

When refactorization occurs, the LU factors are recomputed from the current basis. If the basis has changed significantly since the last factorization (many pivots), the new L and U factors may have a different sparsity pattern. The eta memory pool is reset, so all old eta vectors become invalid. Any cached results depending on old eta vectors must be recomputed.

### Variable-Fixing with Quadratic Objectives

When a variable with quadratic objective terms is fixed at a bound, additional updates are required beyond the standard eta record. The diagonal quadratic coefficient is absorbed into the objective constant, and off-diagonal coefficients are linearized into the reduced costs of neighboring variables. The variable-fixing eta record must store enough information to reverse these updates if the fixing is later undone.

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All algorithms cite published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```

---

## References

- Bartels, R.H. and Golub, G.H. (1969). "The simplex method of linear programming using LU decomposition." *Communications of the ACM*, 12(5):266-268.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Dantzig, G.B. and Orchard-Hays, W. (1954). "The product form for the inverse in the simplex method." *RAND Report P-440*.
- Forrest, J.J.H. and Tomlin, J.A. (1972). "Updated triangular factors of the basis to maintain sparsity in the product form simplex method." *Mathematical Programming*, 2(1):263-278.
- Hall, J.A.J. (2005). "Hyper-sparsity in the revised simplex method and how to exploit it." *Computational Optimization and Applications*, 32(3):173-194.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Reid, J.K. (1982). "A sparsity-exploiting variant of the Bartels-Golub decomposition for linear programming bases." *Mathematical Programming*, 24(1):55-69.
- Suhl, U.H. and Suhl, L.M. (1990). "Computing sparse LU factorizations for large-scale linear programming bases." *ORSA Journal on Computing*, 2(4):325-335.
