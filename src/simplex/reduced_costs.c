/**
 * @file reduced_costs.c
 * @brief Reduced cost computation: dj = cj - pi^T * Aj
 *
 * Extracted from solve_lp.c. Computes dual prices via BTRAN and
 * then reduced costs for all variables (original + auxiliary).
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

extern int cxf_btran_vec(BasisState *basis, const double *input, double *result);

/**
 * @brief Get the coefficient for slack/surplus/artificial variable.
 *
 * For <= constraints: +1 if RHS >= 0, -1 if RHS < 0
 * For >= constraints: always -1
 * For = constraints: +1 if RHS >= 0, -1 if RHS < 0
 */
static double get_auxiliary_coeff(const MatrixData *mat, int row) {
    if (mat == NULL || mat->sense == NULL) return 1.0;
    char sense = mat->sense[row];
    double rhs = (mat->rhs != NULL) ? mat->rhs[row] : 0.0;
    if (sense == '>' || sense == 'G') return -1.0;
    if (sense == '<' || sense == 'L') return (rhs < 0) ? -1.0 : 1.0;
    if (sense == '=') return (rhs < 0) ? -1.0 : 1.0;
    return 1.0;
}

int cxf_compute_reduced_costs(SolverState *state) {
    CxfModel *model = state->model_ref;
    MatrixData *mat = model->matrix;
    BasisState *basis = state->basis;
    int n = state->num_vars;
    int m = state->num_constrs;
    int total_vars = n + m;

    /* Step 1: Compute dual prices pi = B^(-T) * c_B via BTRAN */
    /* P0.5: propagate errors instead of silently substituting pi = c_B */
    double *cB = (double *)calloc((size_t)m, sizeof(double));
    if (cB == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }
    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        cB[i] = (bv >= 0 && bv < total_vars) ? state->work_obj[bv] : 0.0;
    }
    int rc = cxf_btran_vec(basis, cB, state->work_pi);
    free(cB);
    if (rc != CXF_OK) {
        return rc;
    }

    /* Step 2: Compute reduced costs for all variables */
    for (int j = 0; j < total_vars; j++) {
        if (basis->var_status[j] >= 0) {
            state->work_dj[j] = 0.0;
        } else {
            double dj = state->work_obj[j];
            if (j < n && mat != NULL) {
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++)
                    dj -= state->work_pi[mat->row_idx[k]] * mat->values[k];
            } else if (j >= n) {
                int row = j - n;
                if (row >= 0 && row < m) {
                    double coeff = (basis->diag_coeff != NULL) ?
                        basis->diag_coeff[row] :
                        get_auxiliary_coeff(mat, row);
                    dj -= state->work_pi[row] * coeff;
                }
            }
            state->work_dj[j] = dj;
        }
    }
    return CXF_OK;
}
