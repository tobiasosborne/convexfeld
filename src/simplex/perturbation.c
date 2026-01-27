/**
 * @file perturbation.c
 * @brief Simplex perturbation for anti-cycling (M7.1.13)
 *
 * Implements Wolfe perturbation method to prevent simplex cycling in
 * degenerate LPs. Adds small deterministic perturbations to bounds.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <string.h>

/* Base perturbation scale relative to feasibility tolerance */
#define PERTURB_BASE_SCALE 1e-6

/* Maximum perturbation scale (relative to feasibility tolerance) */
#define PERTURB_MAX_SCALE 1e-3

/* Minimum objective coefficient magnitude for scaling */
#define MIN_OBJ_COEFF 1e-8

/* Global flag to track perturbation state (shared between functions) */
static int g_perturbation_applied = 0;

/**
 * @brief Generate deterministic pseudo-random value in [0, 1).
 *
 * Uses simple LCG-style hash based on variable index for determinism.
 *
 * @param index Variable index (seed)
 * @return Pseudo-random value in [0, 1)
 */
static double pseudo_random(int index) {
    /* Simple deterministic pseudo-random using multiplicative hash */
    unsigned int x = (unsigned int)index;
    x = x * 2654435761U;  /* Golden ratio prime */
    x ^= (x >> 16);
    x *= 0x85ebca6bU;
    x ^= (x >> 13);
    x *= 0xc2b2ae35U;
    x ^= (x >> 16);

    /* Convert to [0, 1) */
    return (double)x / 4294967296.0;
}

/**
 * @brief Apply anti-cycling perturbation to bounds.
 *
 * Implements the Wolfe perturbation method to break degeneracy and
 * prevent cycling in the simplex algorithm. Adds small deterministic
 * perturbations to variable bounds.
 *
 * Algorithm:
 * 1. Skip if already applied
 * 2. Calculate base perturbation scale from feasibility tolerance
 * 3. For each variable:
 *    - Skip unbounded variables
 *    - Scale based on objective coefficient
 *    - Generate deterministic perturbation
 *    - Apply to bounds (lb increases, ub decreases)
 *    - Handle bound crossing with midpoint
 *
 * @param state Solver context with working bounds
 * @param env Environment with tolerances
 * @return 0 on success, CXF_ERROR_OUT_OF_MEMORY on allocation failure
 */
int cxf_simplex_perturbation(SolverContext *state, CxfEnv *env) {
    /* Validate inputs */
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Check if already applied */
    if (g_perturbation_applied) {
        return CXF_OK;
    }

    int n = state->num_vars;
    if (n == 0) {
        g_perturbation_applied = 1;
        return CXF_OK;
    }

    /* Extract parameters */
    double feas_tol = env->feasibility_tol;
    if (feas_tol <= 0.0) {
        feas_tol = CXF_FEASIBILITY_TOL;
    }
    double infinity = env->infinity;
    if (infinity <= 0.0) {
        infinity = CXF_INFINITY;
    }

    /* Calculate base perturbation scale */
    double base_scale = feas_tol * PERTURB_BASE_SCALE;
    double max_scale = feas_tol * PERTURB_MAX_SCALE;

    double *lb = state->work_lb;
    double *ub = state->work_ub;
    double *obj = state->work_obj;

    /* Apply perturbations to each variable */
    for (int j = 0; j < n; j++) {
        /* Skip unbounded variables */
        if (lb[j] <= -infinity && ub[j] >= infinity) {
            continue;
        }

        /* Compute variable-specific scale based on objective coefficient */
        double scale;
        double abs_obj = fabs(obj[j]);
        if (abs_obj > MIN_OBJ_COEFF) {
            scale = base_scale / abs_obj;
        } else {
            scale = base_scale;
        }

        /* Clamp scale to maximum */
        if (scale > max_scale) {
            scale = max_scale;
        }

        /* Generate deterministic perturbations using index as seed */
        double rand1 = pseudo_random(j);
        double rand2 = pseudo_random(j + n);  /* Different seed for ub */

        double eps_lb = rand1 * scale;
        double eps_ub = rand2 * scale;

        /* Store original bounds for bound crossing check */
        double orig_lb = lb[j];
        double orig_ub = ub[j];

        /* Apply perturbations (conservative: shrink feasible region) */
        if (lb[j] > -infinity) {
            lb[j] += eps_lb;
        }
        if (ub[j] < infinity) {
            ub[j] -= eps_ub;
        }

        /* Handle bound crossing: use midpoint */
        if (lb[j] > ub[j]) {
            double mid = (orig_lb + orig_ub) * 0.5;
            lb[j] = mid - eps_lb * 0.5;
            ub[j] = mid + eps_ub * 0.5;
        }
    }

    /* Set flag to indicate perturbation has been applied */
    g_perturbation_applied = 1;

    return CXF_OK;
}

/**
 * @brief Remove perturbations and restore original bounds.
 *
 * Restores working bounds from the original model bounds, undoing
 * the perturbations applied by cxf_simplex_perturbation.
 *
 * @param state Solver context
 * @param env Environment (unused but required by signature)
 * @return 0 on success, 1 if no perturbation was applied
 */
int cxf_simplex_unperturb(SolverContext *state, CxfEnv *env) {
    /* Validate inputs */
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Check if perturbation was applied */
    if (!g_perturbation_applied) {
        return 1;  /* Nothing to remove */
    }

    /* Restore original bounds from model */
    int n = state->num_vars;
    if (n > 0 && state->model_ref != NULL) {
        CxfModel *model = state->model_ref;

        /* Copy original bounds back to working bounds */
        if (state->work_lb != NULL && model->lb != NULL) {
            memcpy(state->work_lb, model->lb, (size_t)n * sizeof(double));
        }
        if (state->work_ub != NULL && model->ub != NULL) {
            memcpy(state->work_ub, model->ub, (size_t)n * sizeof(double));
        }
    }

    /* Clear perturbation flag */
    g_perturbation_applied = 0;

    return CXF_OK;
}
