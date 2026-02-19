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
    CXF_ERROR_DATA_NOT_AVAILABLE = -4, /**< Requested data not available */
    CXF_ERROR_NOT_SUPPORTED     = -5   /**< Feature not supported */
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
 * @brief Vector container for sparse vectors with indices and values.
 *
 * Used throughout ConvexFeld for storing sparse vectors, index lists,
 * and coefficient arrays.
 */
typedef struct VectorContainer {
    int *indices;      /**< Array of indices (may be NULL) */
    double *values;    /**< Array of values (may be NULL) */
    void *auxData;     /**< Auxiliary data pointer (may be NULL) */
    int capacity;      /**< Allocated capacity */
    int size;          /**< Current number of elements */
} VectorContainer;

/**
 * @brief Chunk in the eta buffer arena allocator.
 *
 * Chunks are linked in a singly-linked list for bulk deallocation.
 */
typedef struct EtaChunk {
    char *data;              /**< Chunk data buffer */
    size_t capacity;         /**< Chunk capacity in bytes */
    struct EtaChunk *next;   /**< Next chunk in chain */
} EtaChunk;

/**
 * @brief Arena allocator for eta vectors.
 *
 * Provides efficient bump-pointer allocation with exponential chunk growth.
 * All memory is freed in bulk at refactorization.
 */
typedef struct EtaBuffer {
    EtaChunk *firstChunk;    /**< Head of chunk chain */
    EtaChunk *activeChunk;   /**< Current chunk being allocated from */
    size_t bytesUsed;        /**< Bytes used in active chunk */
    size_t currentChunkSize; /**< Current chunk size for growth */
    size_t minChunkSize;     /**< Minimum chunk size */
} EtaBuffer;

/** @brief Maximum chunk size for eta buffer (64KB) */
#define CXF_MAX_CHUNK_SIZE 65536

/** @brief Default minimum chunk size for eta buffer (4KB) */
#define CXF_MIN_CHUNK_SIZE 4096

#endif /* CXF_TYPES_H */
