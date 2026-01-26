# Structure Definitions

## Complete Structure Checklist

| Structure | Spec File | Header | Implementation |
|-----------|-----------|--------|----------------|
| CxfEnv | `docs/specs/structures/cxf_env.md` | `include/convexfeld/cxf_env.h` | `src/api/env.c` |
| CxfModel | `docs/specs/structures/cxf_model.md` | `include/convexfeld/cxf_model.h` | `src/api/model.c` |
| SparseMatrix | `docs/specs/structures/sparse_matrix.md` | `include/convexfeld/cxf_matrix.h` | `src/matrix/sparse_matrix.c` |
| SolverContext | `docs/specs/structures/solver_context.md` | `include/convexfeld/cxf_solver.h` | `src/solver_state/context.c` |
| BasisState | `docs/specs/structures/basis_state.md` | `include/convexfeld/cxf_basis.h` | `src/basis/basis_state.c` |
| EtaFactors | `docs/specs/structures/eta_factors.md` | `include/convexfeld/cxf_basis.h` | `src/basis/eta_factors.c` |
| PricingContext | `docs/specs/structures/pricing_context.md` | `include/convexfeld/cxf_pricing.h` | `src/pricing/context.c` |
| CallbackContext | `docs/specs/structures/callback_context.md` | `include/convexfeld/cxf_callback.h` | `src/callbacks/context.c` |

---

## CxfEnv

```c
struct CxfEnv {
    uint32_t magic;           /* 0xC0FEFE1D for validation */
    int active;               /* 1 if environment is active */
    char error_buffer[512];   /* Last error message */
    double feasibility_tol;   /* Primal feasibility tolerance */
    double optimality_tol;    /* Dual optimality tolerance */
    double infinity;          /* Infinity threshold */
};
```

---

## CxfModel

```c
struct CxfModel {
    uint32_t magic;           /* 0xC0FEFE1D for validation */
    CxfEnv *env;              /* Parent environment */
    char name[CXF_MAX_NAME_LEN + 1];
    int num_vars;             /* Number of variables */
    int num_constrs;          /* Number of constraints */
    double *obj_coeffs;       /* Objective coefficients [num_vars] */
    double *lb;               /* Lower bounds [num_vars] */
    double *ub;               /* Upper bounds [num_vars] */
    double *solution;         /* Solution values [num_vars] */
    int status;               /* Optimization status */
    double obj_val;           /* Objective value */
};
```

---

## SparseMatrix

```c
/* CSC (Compressed Sparse Column) format */
struct SparseMatrix {
    int num_rows;             /* Number of rows (m) */
    int num_cols;             /* Number of columns (n) */
    int64_t nnz;              /* Number of non-zeros */
    int64_t *col_ptr;         /* Column pointers [num_cols + 1] */
    int *row_idx;             /* Row indices [nnz] */
    double *values;           /* Non-zero values [nnz] */
    /* Optional CSR (lazy conversion) */
    int64_t *row_ptr;         /* Row pointers (NULL if not built) */
    int *col_idx;             /* Column indices (NULL if not built) */
    double *row_values;       /* Row-major values (NULL if not built) */
};
```

---

## SolverContext

```c
struct SolverContext {
    CxfModel *model_ref;      /* Back-pointer to model */
    int phase;                /* 0=setup, 1=phase I, 2=phase II */
    int num_vars;
    int num_constrs;
    int64_t num_nonzeros;
    int solve_mode;           /* 0=primal, 1=dual, 2=barrier */
    int max_iterations;
    double tolerance;
    double obj_value;
    /* Working arrays */
    double *work_lb;          /* Working lower bounds [num_vars] */
    double *work_ub;          /* Working upper bounds [num_vars] */
    double *work_obj;         /* Working objective [num_vars] */
    double *work_x;           /* Current solution [num_vars] */
    double *work_pi;          /* Dual values [num_constrs] */
    double *work_dj;          /* Reduced costs [num_vars] */
    /* Subcomponents */
    BasisState *basis;
    PricingContext *pricing;
};
```

---

## BasisState

```c
struct BasisState {
    int m;                    /* Number of basic variables */
    int *basic_vars;          /* Indices of basic variables [m] */
    int *var_status;          /* Status of each variable [n] */
    int eta_count;            /* Number of eta vectors */
    int eta_capacity;         /* Capacity for eta vectors */
    EtaFactors **eta_list;    /* Array of eta factor pointers */
    double *work;             /* Working array [m] */
    int refactor_freq;        /* Refactorization frequency */
    int pivots_since_refactor;/* Pivots since last refactor */
};
```

---

## EtaFactors

```c
struct EtaFactors {
    int type;                 /* 1=column, 2=row based */
    int pivot_row;            /* Row index for pivot */
    int nnz;                  /* Non-zeros in eta vector */
    int *indices;             /* Row indices [nnz] */
    double *values;           /* Values [nnz] */
    double pivot_elem;        /* Pivot element */
};
```

---

## PricingContext

```c
struct PricingContext {
    int current_level;        /* Active pricing level (0=full) */
    int max_levels;           /* Number of levels (typically 3-5) */
    int *candidate_counts;    /* Candidates at each level */
    int **candidate_arrays;   /* Variable indices per level */
    int *cached_counts;       /* Cached result count (-1=invalid) */
    int last_pivot_iteration;
    int64_t total_candidates_scanned;
    int level_escalations;
};
```

---

## CallbackContext

```c
struct CallbackContext {
    uint32_t magic;                    /* 0xCA11BAC7 */
    uint64_t safety_magic;             /* 0xF1E1D5AFE7E57A7E */
    int (*callback_func)(CxfModel *, void *);
    void *user_data;
    int terminate_requested;
    double start_time;
    int iteration_count;
    double best_obj;
};
```
