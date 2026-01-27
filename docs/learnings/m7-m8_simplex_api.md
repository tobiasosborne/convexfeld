# M7-M8: Simplex Engine & API Layer Learnings

## M8: API Layer (Level 0)

### 2026-01-27: M8.1.10 Model Creation API

**Files modified:**
- `src/api/model.c` (229 LOC) - Added cxf_copymodel and cxf_updatemodel
- `include/convexfeld/cxf_model.h` - Added function declarations
- `tests/unit/test_api_model.c` (471 LOC) - Added 8 new tests

**Functions implemented:**
- `cxf_copymodel(model)` - Create independent copy of model
- `cxf_updatemodel(model)` - Apply pending modifications (stub)

**Tests added (8 new, 37 total):**
- test_copymodel_basic - Verify copy creates new model with same name/env
- test_copymodel_null_returns_null - NULL handling
- test_copymodel_copies_variables - Deep copy of variable arrays
- test_copymodel_copies_status - Copy status, obj_val, initialized
- test_copymodel_independent_modification - Verify copies are independent
- test_updatemodel_null_returns_error - NULL handling
- test_updatemodel_valid_model_returns_ok - Stub returns CXF_OK
- test_updatemodel_idempotent - Can call multiple times safely

**Implementation approach - Simplified vs Spec:**

The spec defines complex behavior including:
- Copy lock acquisition via `cxf_env_acquire_copy_lock`
- Pending change detection via `cxf_check_pending_changes`
- Two copy paths: `cxf_model_copy_standard` vs `cxf_model_copy_with_callbacks`
- Environment allowCopy flag management
- Callback preservation

**Implemented simplified version for M8.1.10:**
- Direct deep copy without locking (single-threaded for now)
- Skips pending buffer handling (not yet implemented)
- Skips matrix copying (matrix field currently NULL)
- Skips callback handling (callback system basic)
- Uses cxf_newmodel + manual field copy pattern

**Key design decisions:**
- Reallocation strategy: Free and reallocate arrays if capacity mismatch
- Array-by-array copy using for loops (clear, explicit)
- Returns NULL on any error (strong error handling)
- cxf_updatemodel is stub (validates model, returns CXF_OK)

**Pattern for lazy update (to be expanded later):**
```c
int cxf_updatemodel(CxfModel *model) {
    if (cxf_checkmodel(model) != CXF_OK) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    // Future: Process pending_buffer to apply queued modifications
    // Future: Rebuild CSC matrix if structure changed
    // Future: Invalidate cached solution data
    return CXF_OK;
}
```

**File size issue:**
- model.c now 229 LOC (exceeds 200 LOC limit)
- Created issue convexfeld-447: "Refactor src/api/model.c to < 200 LOC"
- Possible refactoring: Extract cxf_copymodel to separate model_copy.c

**Dependencies:**
- Uses cxf_newmodel, cxf_freemodel, cxf_checkmodel (already implemented)
- Uses cxf_malloc, cxf_free (memory module)
- Accesses CxfModel fields directly (defined in cxf_model.h)

---

### 2026-01-27: M8.1.12 Constraint API with Validation

**Files modified:**
- `src/api/constr_stub.c` (119 LOC) - Enhanced validation, added cxf_chgcoeffs
- `tests/unit/test_api_constrs.c` (356 LOC) - Added 6 new tests

**Functions enhanced:**
- `cxf_addconstr()` - Added validation for modification_blocked, sense, indices, coefficients
- `cxf_addconstrs()` - Added validation for batch constraints
- `cxf_chgcoeffs()` - New function to change constraint matrix coefficients (stub)

**Tests added (6 new, 19 total):**
- test_addconstr_validates_sense - Rejects invalid sense characters
- test_addconstr_validates_indices - Catches out-of-range variable indices
- test_addconstr_validates_finite - Rejects NaN/Inf coefficients
- test_chgcoeffs_null_model_fails - NULL model handling
- test_chgcoeffs_empty_is_ok - Empty operation returns CXF_OK
- test_chgcoeffs_basic - Basic coefficient change validates correctly
- test_chgcoeffs_validates_indices - Catches invalid constraint/variable indices

**Validation pattern implemented:**
1. Check model != NULL
2. Check model->modification_blocked (return CXF_ERROR_INVALID_ARGUMENT if blocked)
3. Validate sense is '<', '>', or '='
4. Validate rhs is not NaN using isnan()
5. If numnz > 0, validate cind/cval not NULL
6. Validate all cind[i] in range [0, num_vars)
7. Validate all cval[i] finite using isfinite()

**Key learnings:**
- `CXF_ERROR_MODEL_MODIFICATION` doesn't exist - use `CXF_ERROR_INVALID_ARGUMENT` instead
- Variable API (model_stub.c) doesn't check modification_blocked - constraint API is more strict
- `isfinite()` from math.h checks for both NaN and Inf (better than separate checks)
- Condensed style (one-line returns) keeps file under 200 LOC while maintaining readability

**File size management:**
- Initial implementation: 252 LOC with verbose style
- Condensed to 119 LOC by removing docstring repetition and using inline returns
- Pattern: Keep detailed docstrings for complex functions, minimal for stubs

**Pattern for compact validation:**
```c
if (model == NULL) return CXF_ERROR_NULL_ARGUMENT;
if (model->modification_blocked) return CXF_ERROR_INVALID_ARGUMENT;
if (sense != '<' && sense != '>' && sense != '=') return CXF_ERROR_INVALID_ARGUMENT;
```

**cxf_chgcoeffs stub behavior:**
- Validates model, indices, and values
- Returns CXF_OK without actually changing anything
- Allows cnt <= 0 (returns CXF_OK immediately)
- Checks both constraint indices [0, num_constrs) and variable indices [0, num_vars)

**Test patterns observed:**
- Empty operations (cnt=0, numnz=0) return CXF_OK (success)
- NULL arrays allowed only when count is 0
- Validation happens before capacity checks
- Tests verify error codes match exact expected values

---
