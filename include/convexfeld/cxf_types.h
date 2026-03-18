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
 * @brief Optimization status codes (1-19 range per spec).
 *
 * Returned by cxf_optimize / cxf_solve_lp to indicate the solver outcome.
 * CXF_OK (0) indicates a successful API call with no optimization result.
 */
typedef enum {
    CXF_OK              = 0,   /**< Operation completed successfully */
    CXF_LOADED          = 1,   /**< Model loaded, no solution yet */
    CXF_OPTIMAL         = 2,   /**< Optimal solution found */
    CXF_INFEASIBLE      = 3,   /**< Problem is infeasible */
    CXF_INF_OR_UNBD     = 4,   /**< Problem is infeasible or unbounded */
    CXF_UNBOUNDED       = 5,   /**< Problem is unbounded */
    CXF_CUTOFF          = 6,   /**< Optimal objective worse than cutoff */
    CXF_ITERATION_LIMIT = 7,   /**< Iteration limit reached */
    CXF_NODE_LIMIT      = 8,   /**< Node limit reached (MIP) */
    CXF_TIME_LIMIT      = 9,   /**< Time limit reached */
    CXF_SOLUTION_LIMIT  = 10,  /**< Solution limit reached (MIP) */
    CXF_INTERRUPTED     = 11,  /**< User interrupt */
    CXF_NUMERIC         = 12,  /**< Numerical difficulties encountered */
    CXF_SUBOPTIMAL      = 13,  /**< Sub-optimal solution available */
    CXF_INPROGRESS      = 14,  /**< Async call still in progress */
    CXF_USER_OBJ_LIMIT  = 15,  /**< User objective limit reached */
    CXF_WORK_LIMIT      = 16,  /**< Work limit exceeded */
    CXF_MEM_LIMIT       = 17,  /**< Memory limit exceeded */
    CXF_LOCALLY_OPTIMAL = 18,  /**< Locally optimal (non-convex) */
    CXF_LOCALLY_INFEASIBLE = 19 /**< Locally infeasible (non-convex) */
} CxfOptimStatus;

/**
 * @brief Error codes (10001+ range per spec).
 *
 * Returned by API functions to indicate specific error conditions.
 */
typedef enum {
    CXF_ERROR_OUT_OF_MEMORY       = 10001, /**< Memory allocation failed */
    CXF_ERROR_NULL_ARGUMENT       = 10002, /**< NULL pointer passed */
    CXF_ERROR_INVALID_ARGUMENT    = 10003, /**< Invalid argument value */
    CXF_ERROR_UNKNOWN_ATTRIBUTE   = 10004, /**< Unknown attribute name */
    CXF_ERROR_DATA_NOT_AVAILABLE  = 10005, /**< Data not accessible */
    CXF_ERROR_INDEX_OUT_OF_RANGE  = 10006, /**< Index outside valid range */
    CXF_ERROR_UNKNOWN_PARAMETER   = 10007, /**< Unknown parameter name */
    CXF_ERROR_VALUE_OUT_OF_RANGE  = 10008, /**< Parameter value out of range */
    CXF_ERROR_NO_LICENSE          = 10009, /**< License validation failed */
    CXF_ERROR_SIZE_LIMIT_EXCEEDED = 10010, /**< Model exceeds size limit */
    CXF_ERROR_CALLBACK            = 10011, /**< Error in user callback */
    CXF_ERROR_FILE_READ           = 10012, /**< Failed to read file */
    CXF_ERROR_FILE_WRITE          = 10013, /**< Failed to write file */
    CXF_ERROR_NUMERIC             = 10014, /**< Numerical error encountered */
    CXF_ERROR_IIS_NOT_INFEASIBLE  = 10015, /**< IIS on feasible model */
    CXF_ERROR_NOT_FOR_MIP         = 10016, /**< Operation invalid for MIP */
    CXF_ERROR_OPTIMIZATION_IN_PROGRESS = 10017, /**< Concurrent access */
    CXF_ERROR_DUPLICATES          = 10018, /**< Duplicate indices */
    CXF_ERROR_NODEFILE            = 10019, /**< Node file I/O error */
    CXF_ERROR_Q_NOT_PSD           = 10020, /**< Q matrix not PSD */
    CXF_ERROR_QCP_EQUALITY        = 10021, /**< Non-convex QCP equality */
    CXF_ERROR_NETWORK             = 10022, /**< Network error */
    CXF_ERROR_JOB_REJECTED        = 10023, /**< Server rejected job */
    CXF_ERROR_NOT_SUPPORTED       = 10024, /**< Feature not supported */
    CXF_ERROR_EXCEED_2B_NONZEROS  = 10025, /**< Matrix exceeds 2B nonzeros */
    CXF_ERROR_INVALID_PIECEWISE   = 10026, /**< Invalid piecewise objective */
    CXF_ERROR_UPDATEMODE_CHANGE   = 10027, /**< UpdateMode modification */
    CXF_ERROR_CLOUD               = 10028, /**< Cloud computing error */
    CXF_ERROR_MODEL_MODIFICATION  = 10029, /**< Model modification invalid */
    CXF_ERROR_CSWORKER            = 10030, /**< Client-server worker error */
    CXF_ERROR_TUNE_MODEL_TYPES    = 10031, /**< Multi-type tuning */
    CXF_ERROR_SECURITY            = 10032, /**< Authentication failure */
    CXF_ERROR_NOT_IN_MODEL        = 20001, /**< Element not in model */
    CXF_ERROR_FAILED_TO_CREATE    = 20002, /**< Failed to create model */
    CXF_ERROR_INTERNAL            = 20003  /**< Internal error */
} CxfErrorCode;

/** @brief Check if a return code is an error (10001+ range) */
#define CXF_IS_ERROR(code) ((code) >= 10001)

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
 * Variable Status Encoding
 *
 * Uses the standard revised simplex convention:
 *   >= 0 : BASIC — value is the constraint row index
 *   -1   : AT_LOWER — nonbasic at lower bound
 *   -2   : AT_UPPER — nonbasic at upper bound
 *   -3   : SUPERBASIC — nonbasic between bounds (free variables)
 *   -4   : FIXED — nonbasic, lb == ub
 *
 * This encoding stores basis membership and bound status in a single int
 * array, eliminating the need for a separate lookup structure.
 *
 * Invariant: for every row i, varStatus[basisHeader[i]] == i
 ******************************************************************************/

/** @brief Nonbasic at lower bound */
#define CXF_VAR_AT_LOWER     (-1)

/** @brief Nonbasic at upper bound */
#define CXF_VAR_AT_UPPER     (-2)

/** @brief Nonbasic between bounds (free/superbasic) */
#define CXF_VAR_SUPERBASIC   (-3)

/** @brief Nonbasic, lower bound == upper bound */
#define CXF_VAR_FIXED        (-4)

/** @brief Check if a variable status indicates basic (row index >= 0) */
#define CXF_VAR_IS_BASIC(status) ((status) >= 0)

/*******************************************************************************
 * Row Status Encoding (v2 Crash Basis — P2.5)
 *
 * Row status tracks crash basis classification:
 *   0    : UNASSIGNED — not yet classified
 *   > 0  : CANDIDATE — pre-classified for potential removal
 *   -1   : BASIC_LOWER — slack assigned to basis at lower bound
 *   -2   : BASIC_UPPER — slack assigned to basis at upper bound (removed row)
 ******************************************************************************/

/** @brief Row not yet classified by crash */
#define CXF_ROW_UNASSIGNED   0

/** @brief Row's slack assigned to basis at lower bound (feasible) */
#define CXF_ROW_BASIC_LOWER  (-1)

/** @brief Row's slack assigned to basis at upper bound (removed candidate) */
#define CXF_ROW_BASIC_UPPER  (-2)

/*******************************************************************************
 * Numerical Constants
 ******************************************************************************/

/** @brief Representation of infinity for bounds */
#define CXF_INFINITY        1e100

/** @brief Default primal feasibility tolerance */
#define CXF_FEASIBILITY_TOL 1e-6

/** @brief Default dual optimality tolerance */
#define CXF_OPTIMALITY_TOL  1e-6

/** @brief Harris pivot tolerance — minimum |element| for ratio test candidacy */
#define CXF_PIVOT_TOL       1e-9

/** @brief Small pivot threshold for refactorization trigger (§A.3) */
#define CXF_SMALL_PIVOT_THRESHOLD  1e-7

/** @brief Zero tolerance for numerical comparisons (significant bound change) */
#define CXF_ZERO_TOL        1e-12

/** @brief Absolute floor on pivot magnitude during LU factorization */
#define CXF_MIN_PIVOT       1e-13

/** @brief Markowitz pivot tolerance (1/128) for sparse LU */
#define CXF_MARKOWITZ_TOL   0.0078125

/** @brief Bound gap below which a variable is treated as fixed */
#define CXF_BOUND_EQUALITY_TOL  1e-10

/** @brief Minimum perturbation magnitude (perturbation floor) */
#define CXF_PERTURB_FLOOR   1e-10

/** @brief Maximum perturbation magnitude (perturbation ceiling) */
#define CXF_PERTURB_CEILING 1e-6

/** @brief Effectively infinite bound threshold for heuristic decisions */
#define CXF_LARGE_BOUND_MARKER  1e20

/*******************************************************************************
 * Ratio Test Status (V2 harris_ratio_test.md — Outputs)
 ******************************************************************************/

/** @brief Ratio test outcome classification */
typedef enum {
    CXF_RT_NORMAL_PIVOT    = 0,  /**< Normal pivot with positive step */
    CXF_RT_DEGENERATE_PIVOT = 1, /**< Pivot with zero/negligible step */
    CXF_RT_UNBOUNDED       = 2,  /**< No blocking variable found */
    CXF_RT_BOUND_FLIP_ONLY = 3   /**< All blockers flipped, no basis exchange */
} CxfRatioStatus;

/** @brief Maximum length of names (variables, constraints, model) */
#define CXF_MAX_NAME_LEN    255

/*******************************************************************************
 * Magic Numbers for Structure Validation
 ******************************************************************************/

/** @brief Magic number for CxfEnv validation */
#define CXF_ENV_MAGIC       0xC0FEFE1DU

/** @brief Magic number for CxfModel validation */
#define CXF_MODEL_MAGIC     0xC0FE0D31U

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
 * @brief Callback function signature (V2: resides on Environment).
 * @param model  The model being optimized
 * @param cbdata Callback data pointer (typically CallbackContext)
 * @param where  Context code indicating invocation point (CXF_CB_*)
 * @param usrdata User-provided data pointer
 * @return 0 to continue, non-zero to terminate
 */
typedef int (*CxfCallbackFunc)(CxfModel *model, void *cbdata,
                                int where, void *usrdata);

/**
 * @brief Sparse matrix in CSC format with optional CSR.
 * @see include/convexfeld/cxf_matrix.h
 */
typedef struct MatrixData MatrixData;

/**
 * @brief Solver context - working state during optimization.
 * @see include/convexfeld/cxf_solver.h
 */
typedef struct SolverState SolverState;

/**
 * @brief Basis state - current basis and factorization.
 * @see include/convexfeld/cxf_basis.h
 */
typedef struct BasisState BasisState;

/**
 * @brief Eta factors for basis updates.
 * @see include/convexfeld/cxf_basis.h
 */
typedef struct EtaVector EtaVector;

/**
 * @brief Pricing context - partial pricing state.
 * @see include/convexfeld/cxf_pricing.h
 */
typedef struct PricingState PricingState;

/**
 * @brief Callback context - user callback state.
 * @see include/convexfeld/cxf_callback.h
 */
typedef struct CallbackContext CallbackContext;

/**
 * @brief Vector container for sparse vectors (forward declaration).
 * Full definition in src/memory/internal_types.h (solver internal).
 */
typedef struct VectorContainer VectorContainer;

/**
 * @brief Chunk in the eta buffer arena allocator (forward declaration).
 * Full definition in src/memory/internal_types.h (solver internal).
 */
typedef struct EtaChunk EtaChunk;

/**
 * @brief Arena allocator for eta vectors (forward declaration).
 * Full definition in src/memory/internal_types.h (solver internal).
 */
typedef struct EtaBuffer EtaBuffer;

/*******************************************************************************
 * Eta Vector Type Constants (V2 eta_vector.md)
 ******************************************************************************/

/** @brief Eta type: pivot transformation (entering/leaving variable exchange) */
#define CXF_ETA_PIVOT         2

/** @brief Eta type: variable fixing (bound snap, presolve, etc.) */
#define CXF_ETA_VARIABLE_FIX  3

/** @brief Eta type: quadratic warm-start record */
#define CXF_ETA_WARM_START    4

/** @brief Eta type: bound-change record (step2/step3 propagation) */
#define CXF_ETA_BOUND_CHANGE  5

/*******************************************************************************
 * Eta Buffer Size Constants
 ******************************************************************************/

/** @brief Maximum chunk size for eta buffer (64KB) */
#define CXF_MAX_CHUNK_SIZE 65536

/** @brief Default minimum chunk size for eta buffer (4KB) */
#define CXF_MIN_CHUNK_SIZE 4096

#endif /* CXF_TYPES_H */
