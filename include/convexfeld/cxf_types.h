/**
 * @file cxf_types.h
 * @brief Core types, enums, and constants for ConvexFeld LP solver.
 *
 * This header defines all fundamental types used throughout the library:
 * - Status codes (CxfStatus)
 * - Variable types (CxfVarType)
 * - Constraint senses (CxfSense)
 * - Numerical constants and tolerances
 * - Magic numbers for structure validation
 * - Forward declarations for all core structures
 */

#ifndef CXF_TYPES_H
#define CXF_TYPES_H

#include <stdint.h>
#include <stddef.h>

/*******************************************************************************
 * Status Codes
 ******************************************************************************/

/**
 * @brief Return status codes for all ConvexFeld functions.
 *
 * Non-negative values indicate success or optimization outcome.
 * Negative values indicate errors.
 */
typedef enum {
    /* Success codes */
    CXF_OK              = 0,   /**< Operation completed successfully */
    CXF_OPTIMAL         = 1,   /**< Optimal solution found */
    CXF_INFEASIBLE      = 2,   /**< Problem is infeasible */
    CXF_UNBOUNDED       = 3,   /**< Problem is unbounded */
    CXF_INF_OR_UNBD     = 4,   /**< Problem is infeasible or unbounded */
    CXF_ITERATION_LIMIT = 5,   /**< Iteration limit reached */
    CXF_TIME_LIMIT      = 6,   /**< Time limit reached */
    CXF_NUMERIC         = 7,   /**< Numerical difficulties encountered */

    /* Error codes */
    CXF_ERROR_OUT_OF_MEMORY     = -1,  /**< Memory allocation failed */
    CXF_ERROR_NULL_ARGUMENT     = -2,  /**< NULL pointer passed as argument */
    CXF_ERROR_INVALID_ARGUMENT  = -3,  /**< Invalid argument value */
    CXF_ERROR_DATA_NOT_AVAILABLE = -4  /**< Requested data not available */
} CxfStatus;

/*******************************************************************************
 * Variable Types
 ******************************************************************************/

/**
 * @brief Variable type indicators.
 *
 * Character values match common LP file formats.
 */
typedef enum {
    CXF_CONTINUOUS = 'C',  /**< Continuous variable */
    CXF_BINARY     = 'B',  /**< Binary variable (0 or 1) */
    CXF_INTEGER    = 'I',  /**< Integer variable */
    CXF_SEMICONT   = 'S',  /**< Semi-continuous variable */
    CXF_SEMIINT    = 'N'   /**< Semi-integer variable */
} CxfVarType;

/*******************************************************************************
 * Constraint Senses
 ******************************************************************************/

/**
 * @brief Constraint sense indicators.
 *
 * Character values match common LP file formats.
 */
typedef enum {
    CXF_LESS_EQUAL    = '<',  /**< Less than or equal (<=) */
    CXF_GREATER_EQUAL = '>',  /**< Greater than or equal (>=) */
    CXF_EQUAL         = '='   /**< Equal (=) */
} CxfSense;

/*******************************************************************************
 * Objective Sense
 ******************************************************************************/

/**
 * @brief Optimization direction.
 */
typedef enum {
    CXF_MINIMIZE = 1,   /**< Minimize the objective */
    CXF_MAXIMIZE = -1   /**< Maximize the objective */
} CxfObjSense;

/*******************************************************************************
 * Variable Basis Status
 ******************************************************************************/

/**
 * @brief Variable status in the current basis.
 */
typedef enum {
    CXF_BASIC      = 0,  /**< Variable is basic */
    CXF_NONBASIC_L = 1,  /**< Variable is at lower bound */
    CXF_NONBASIC_U = 2,  /**< Variable is at upper bound */
    CXF_SUPERBASIC = 3,  /**< Variable is superbasic (between bounds) */
    CXF_FIXED      = 4   /**< Variable is fixed (lb == ub) */
} CxfVarStatus;

/*******************************************************************************
 * Numerical Constants
 ******************************************************************************/

/** @brief Representation of infinity for bounds */
#define CXF_INFINITY        1e100

/** @brief Default primal feasibility tolerance */
#define CXF_FEASIBILITY_TOL 1e-6

/** @brief Default dual optimality tolerance */
#define CXF_OPTIMALITY_TOL  1e-6

/** @brief Pivot element tolerance (reject pivots below this) */
#define CXF_PIVOT_TOL       1e-10

/** @brief Zero tolerance for numerical comparisons */
#define CXF_ZERO_TOL        1e-12

/** @brief Maximum length of names (variables, constraints, model) */
#define CXF_MAX_NAME_LEN    255

/*******************************************************************************
 * Magic Numbers for Structure Validation
 ******************************************************************************/

/** @brief Magic number for CxfEnv validation */
#define CXF_ENV_MAGIC       0xC0FEFE1DU

/** @brief Magic number for CxfModel validation */
#define CXF_MODEL_MAGIC     0xC0FEFE1DU

/** @brief Magic number for CallbackContext validation (32-bit) */
#define CXF_CALLBACK_MAGIC  0xCA11BAC7U

/** @brief Magic number for CallbackContext validation (64-bit safety) */
#define CXF_CALLBACK_MAGIC2 0xF1E1D5AFE7E57A7EULL

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/

/**
 * @brief Environment structure - manages global state and settings.
 * @see include/convexfeld/cxf_env.h
 */
typedef struct CxfEnv CxfEnv;

/**
 * @brief Model structure - contains LP problem data.
 * @see include/convexfeld/cxf_model.h
 */
typedef struct CxfModel CxfModel;

/**
 * @brief Sparse matrix in CSC format with optional CSR.
 * @see include/convexfeld/cxf_matrix.h
 */
typedef struct SparseMatrix SparseMatrix;

/**
 * @brief Solver context - working state during optimization.
 * @see include/convexfeld/cxf_solver.h
 */
typedef struct SolverContext SolverContext;

/**
 * @brief Basis state - current basis and factorization.
 * @see include/convexfeld/cxf_basis.h
 */
typedef struct BasisState BasisState;

/**
 * @brief Eta factors for basis updates.
 * @see include/convexfeld/cxf_basis.h
 */
typedef struct EtaFactors EtaFactors;

/**
 * @brief Pricing context - partial pricing state.
 * @see include/convexfeld/cxf_pricing.h
 */
typedef struct PricingContext PricingContext;

/**
 * @brief Callback context - user callback state.
 * @see include/convexfeld/cxf_callback.h
 */
typedef struct CallbackContext CallbackContext;

#endif /* CXF_TYPES_H */
