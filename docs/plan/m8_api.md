# Milestone 8: Public API (Level 6)

**Goal:** Complete Model API module (30 functions)
**Parallelizable:** All API functions can be implemented in parallel
**Spec Reference:** `docs/specs/modules/17_model_api.md`

---

## Module: Model API (30 functions)

### Step 8.1.1: API Tests - Environment

**LOC:** ~100
**File:** `tests/unit/test_api_env.c`

Tests for `cxf_loadenv`, `cxf_emptyenv`, `cxf_freeenv`.

### Step 8.1.2: API Tests - Model

**LOC:** ~150
**File:** `tests/unit/test_api_model.c`

Tests for `cxf_newmodel`, `cxf_freemodel`, `cxf_copymodel`, `cxf_updatemodel`.

### Step 8.1.3: API Tests - Variables

**LOC:** ~150
**File:** `tests/unit/test_api_vars.c`

Tests for `cxf_addvar`, `cxf_addvars`, `cxf_delvars`.

### Step 8.1.4: API Tests - Constraints

**LOC:** ~150
**File:** `tests/unit/test_api_constrs.c`

Tests for `cxf_addconstr`, `cxf_addconstrs`, `cxf_addqconstr`, `cxf_addgenconstrIndicator`.

### Step 8.1.5: API Tests - Optimize

**LOC:** ~200
**File:** `tests/unit/test_api_optimize.c`

Tests for `cxf_optimize`, `cxf_optimize_internal`, `cxf_terminate`.

### Step 8.1.6: API Tests - Queries

**LOC:** ~150
**File:** `tests/unit/test_api_query.c`

Tests for `cxf_getintattr`, `cxf_getdblattr`, `cxf_getconstrs`, `cxf_getcoeff`.

### Step 8.1.7: CxfEnv Structure (Full)

**LOC:** ~200
**File:** `src/api/env.c` (expand from stub)
**Spec:** `docs/specs/structures/cxf_env.md`

### Step 8.1.8: CxfModel Structure (Full)

**LOC:** ~200
**File:** `src/api/model.c` (expand from stub)
**Spec:** `docs/specs/structures/cxf_model.md`

### Step 8.1.9: Environment API

**LOC:** ~120
**File:** `src/api/env_api.c`
**Functions:** `cxf_loadenv`, `cxf_emptyenv`, `cxf_freeenv`

### Step 8.1.10: Model Creation API

**LOC:** ~150
**File:** `src/api/model_api.c`
**Functions:** `cxf_newmodel`, `cxf_freemodel`, `cxf_copymodel`, `cxf_updatemodel`

### Step 8.1.11: Variable API

**LOC:** ~150
**File:** `src/api/vars_api.c`
**Functions:** `cxf_addvar`, `cxf_addvars`, `cxf_delvars`

### Step 8.1.12: Constraint API

**LOC:** ~200
**File:** `src/api/constrs_api.c`
**Functions:** `cxf_addconstr`, `cxf_addconstrs`, `cxf_chgcoeffs`, `cxf_getconstrs`, `cxf_getcoeff`

### Step 8.1.13: Quadratic API

**LOC:** ~120
**File:** `src/api/quadratic_api.c`
**Functions:** `cxf_addqpterms`, `cxf_addqconstr`, `cxf_addgenconstrIndicator`

### Step 8.1.14: Optimize API

**LOC:** ~150
**File:** `src/api/optimize_api.c`
**Functions:** `cxf_optimize`, `cxf_optimize_internal`, `cxf_terminate`

### Step 8.1.15: Attribute API

**LOC:** ~120
**File:** `src/api/attrs_api.c`
**Functions:** `cxf_getintattr`, `cxf_getdblattr`

### Step 8.1.16: Parameter API

**LOC:** ~100
**File:** `src/api/params_api.c`
**Functions:** `cxf_setintparam`, `cxf_getintparam`

### Step 8.1.17: I/O API

**LOC:** ~150
**File:** `src/api/io_api.c`
**Functions:** `cxf_read`, `cxf_write`

### Step 8.1.18: Info API

**LOC:** ~80
**File:** `src/api/info_api.c`
**Functions:** `cxf_version`, `cxf_geterrormsg`, `cxf_setcallbackfunc`
