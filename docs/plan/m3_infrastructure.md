# Milestone 3: Infrastructure Layer (Level 1)

**Goal:** Complete Error Handling, Logging, Threading modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/02_error_handling.md`
- `docs/specs/modules/03_logging.md`
- `docs/specs/modules/12_threading.md`

---

## Module: Error Handling (10 functions)

**Spec Directory:** `docs/specs/functions/error_logging/`, `docs/specs/functions/validation/`

### Step 3.1.1: Error Tests

**LOC:** ~150
**File:** `tests/unit/test_error.c`

### Step 3.1.2: Core Error Functions

**LOC:** ~100
**File:** `src/error/core.c`
**Specs:**
- `docs/specs/functions/error_logging/cxf_error.md`
- `docs/specs/functions/error_logging/cxf_errorlog.md`

### Step 3.1.3: NaN/Inf Detection

**LOC:** ~80
**File:** `src/error/nan_check.c`
**Specs:**
- `docs/specs/functions/validation/cxf_check_nan.md`
- `docs/specs/functions/validation/cxf_check_nan_or_inf.md`

### Step 3.1.4: Environment Validation

**LOC:** ~60
**File:** `src/error/env_check.c`
**Spec:** `docs/specs/functions/validation/cxf_checkenv.md`

### Step 3.1.5: Model Flag Checks

**LOC:** ~100
**File:** `src/error/model_flags.c`
**Specs:**
- `docs/specs/functions/validation/cxf_check_model_flags1.md`
- `docs/specs/functions/validation/cxf_check_model_flags2.md`

### Step 3.1.6: Termination Check

**LOC:** ~60
**File:** `src/error/terminate.c`
**Spec:** `docs/specs/functions/callbacks/cxf_check_terminate.md`

### Step 3.1.7: Pivot Validation

**LOC:** ~80
**File:** `src/error/pivot_check.c`
**Specs:**
- `docs/specs/functions/ratio_test/cxf_pivot_check.md`
- `docs/specs/functions/statistics/cxf_special_check.md`

---

## Module: Logging (5 functions)

**Spec Directory:** `docs/specs/functions/error_logging/`, `docs/specs/functions/utilities/`

### Step 3.2.1: Logging Tests

**LOC:** ~100
**File:** `tests/unit/test_logging.c`

### Step 3.2.2: Log Output

**LOC:** ~100
**File:** `src/logging/output.c`
**Specs:**
- `docs/specs/functions/error_logging/cxf_log_printf.md`
- `docs/specs/functions/error_logging/cxf_register_log_callback.md`

### Step 3.2.3: Format Helpers

**LOC:** ~80
**File:** `src/logging/format.c`
**Specs:**
- `docs/specs/functions/utilities/cxf_log10_wrapper.md`
- `docs/specs/functions/utilities/cxf_snprintf_wrapper.md`

### Step 3.2.4: System Info

**LOC:** ~60
**File:** `src/logging/system.c`
**Spec:** `docs/specs/functions/threading/cxf_get_logical_processors.md`

---

## Module: Threading (7 functions)

**Spec Directory:** `docs/specs/functions/threading/`

### Step 3.3.1: Threading Tests

**LOC:** ~150
**File:** `tests/unit/test_threading.c`

### Step 3.3.2: Lock Management

**LOC:** ~120
**File:** `src/threading/locks.c`
**Specs:**
- `docs/specs/functions/threading/cxf_acquire_solve_lock.md`
- `docs/specs/functions/threading/cxf_release_solve_lock.md`
- `docs/specs/functions/threading/cxf_env_acquire_lock.md`
- `docs/specs/functions/threading/cxf_leave_critical_section.md`

### Step 3.3.3: Thread Configuration

**LOC:** ~100
**File:** `src/threading/config.c`
**Specs:**
- `docs/specs/functions/threading/cxf_get_threads.md`
- `docs/specs/functions/threading/cxf_set_thread_count.md`

### Step 3.3.4: CPU Detection

**LOC:** ~80
**File:** `src/threading/cpu.c`
**Specs:**
- `docs/specs/functions/threading/cxf_get_logical_processors.md`
- `docs/specs/functions/threading/cxf_get_physical_cores.md`

### Step 3.3.5: Seed Generation

**LOC:** ~60
**File:** `src/threading/seed.c`
**Spec:** `docs/specs/functions/threading/cxf_generate_seed.md`
