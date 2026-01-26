/**
 * @file cxf_model.h
 * @brief CxfModel structure - optimization problem instance.
 *
 * The model contains the complete problem formulation (objective, constraints,
 * bounds, types) and solution data after optimization.
 */

#ifndef CXF_MODEL_H
#define CXF_MODEL_H

#include "cxf_types.h"

/**
 * @brief Optimization model structure.
 *
 * Contains problem formulation and solution data.
 * Must be associated with a CxfEnv environment.
 */
struct CxfModel {
    uint32_t magic;           /**< Validation magic (CXF_MODEL_MAGIC) */
    CxfEnv *env;              /**< Parent environment */
    char name[CXF_MAX_NAME_LEN + 1]; /**< Model name */

    /* Problem dimensions */
    int num_vars;             /**< Number of variables */
    int num_constrs;          /**< Number of constraints */
    int var_capacity;         /**< Allocated capacity for variable arrays */

    /* Variable data */
    double *obj_coeffs;       /**< Objective coefficients [num_vars] */
    double *lb;               /**< Lower bounds [num_vars] */
    double *ub;               /**< Upper bounds [num_vars] */
    char *vtype;              /**< Variable types [num_vars] (NULL = all continuous) */

    /* Constraint matrix (CSC format) */
    SparseMatrix *matrix;     /**< Constraint matrix */

    /* Solution data */
    double *solution;         /**< Solution values [num_vars] */
    double *pi;               /**< Dual values [num_constrs] */
    int status;               /**< Optimization status (CxfStatus) */
    double obj_val;           /**< Objective value */

    /* Model state */
    int initialized;          /**< 1 if ready for optimization */
    int modification_blocked; /**< 1 if optimization in progress */

    /* Extended fields (M8.1.8 spec) */
    uint32_t fingerprint;     /**< Determinism checksum */
    double update_time;       /**< Time spent in cxf_updatemodel */
    void *pending_buffer;     /**< Batched modifications before update (may be NULL) */
    void *solution_data;      /**< Extended solution data (may be NULL) */
    void *sos_data;           /**< SOS constraint data (may be NULL) */
    void *gen_constr_data;    /**< General constraint data (may be NULL) */

    /* Self-reference and parent tracking */
    CxfModel *primary_model;  /**< Root model for callbacks (self or parent) */
    CxfModel *self_ptr;       /**< Points to self during optimization */

    /* Bookkeeping */
    int callback_count;       /**< Number of registered callbacks */
    int solve_mode;           /**< Special solve mode flag */
    int env_flag;             /**< Environment-related flag for cleanup */
};

/*******************************************************************************
 * Model Lifecycle API
 ******************************************************************************/

/**
 * @brief Create a new empty model.
 * @param env Parent environment
 * @param modelP Output pointer to created model
 * @param name Model name (may be NULL)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name);

/**
 * @brief Free a model and all associated resources.
 * @param model Model to free (may be NULL)
 */
void cxf_freemodel(CxfModel *model);

/**
 * @brief Validate model pointer and state.
 * @param model Model to validate
 * @return CXF_OK if valid, error code otherwise
 */
int cxf_checkmodel(CxfModel *model);

/**
 * @brief Check if model modifications are blocked.
 * @param model Model to check
 * @return 1 if blocked, 0 if modifiable, -1 on error
 */
int cxf_model_is_blocked(CxfModel *model);

/*******************************************************************************
 * Variable API
 ******************************************************************************/

/**
 * @brief Add a single variable to the model.
 * @param model Target model
 * @param lb Lower bound
 * @param ub Upper bound
 * @param obj Objective coefficient
 * @param vtype Variable type (CXF_CONTINUOUS, etc.)
 * @param name Variable name (may be NULL)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_addvar(CxfModel *model, double lb, double ub, double obj,
               char vtype, const char *name);

/**
 * @brief Add multiple variables to the model.
 * @param model Target model
 * @param numvars Number of variables to add
 * @param numnz Total non-zero coefficients (unused currently)
 * @param vbeg Column start indices (unused currently)
 * @param vind Constraint indices (unused currently)
 * @param vval Coefficient values (unused currently)
 * @param obj Objective coefficients (NULL = all 0.0)
 * @param lb Lower bounds (NULL = all 0.0)
 * @param ub Upper bounds (NULL = all infinity)
 * @param vtype Variable types (unused currently)
 * @param varnames Variable names (unused currently)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_addvars(CxfModel *model, int numvars, int numnz,
                const int *vbeg, const int *vind, const double *vval,
                const double *obj, const double *lb, const double *ub,
                const char *vtype, const char **varnames);

/**
 * @brief Mark variables for deletion.
 * @param model Target model
 * @param numdel Number of variables to delete
 * @param ind Indices of variables to delete
 * @return CXF_OK on success, error code otherwise
 */
int cxf_delvars(CxfModel *model, int numdel, const int *ind);

/*******************************************************************************
 * Optimization API
 ******************************************************************************/

/**
 * @brief Optimize the model.
 * @param model Model to optimize
 * @return CXF_OK on success, error code otherwise
 */
int cxf_optimize(CxfModel *model);

/*******************************************************************************
 * Attribute API
 ******************************************************************************/

/**
 * @brief Get an integer attribute value.
 * @param model Model to query
 * @param attrname Attribute name
 * @param valueP Output value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getintattr(CxfModel *model, const char *attrname, int *valueP);

/**
 * @brief Get a double attribute value.
 * @param model Model to query
 * @param attrname Attribute name
 * @param valueP Output value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getdblattr(CxfModel *model, const char *attrname, double *valueP);

#endif /* CXF_MODEL_H */
