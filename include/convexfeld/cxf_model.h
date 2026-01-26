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
};

/*******************************************************************************
 * Model API (stubs for tracer bullet)
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
 * @brief Optimize the model.
 * @param model Model to optimize
 * @return CXF_OK on success, error code otherwise
 */
int cxf_optimize(CxfModel *model);

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
