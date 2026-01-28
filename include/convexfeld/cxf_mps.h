/**
 * @file cxf_mps.h
 * @brief MPS file format parser for LP problems.
 *
 * Supports standard MPS format sections:
 * - NAME: Problem name
 * - ROWS: Constraint types (N=objective, E/L/G=constraints)
 * - COLUMNS: Variable coefficients
 * - RHS: Right-hand side values
 * - BOUNDS: Variable bounds (LO/UP/FX/FR/MI/PL/BV)
 * - RANGES: Range constraints (optional)
 */

#ifndef CXF_MPS_H
#define CXF_MPS_H

#include "cxf_types.h"

/**
 * @brief Read an MPS file and populate the model.
 *
 * Parses the MPS file and adds variables/constraints to the model.
 * The model should be empty or newly created before calling this.
 *
 * @param model Model to populate
 * @param filename Path to MPS file
 * @return CXF_OK on success, error code otherwise
 */
int cxf_readmps(CxfModel *model, const char *filename);

#endif /* CXF_MPS_H */
