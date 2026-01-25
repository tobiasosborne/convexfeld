# Convexfeld LP Solver: Product Requirements Document

**Version:** 1.0
**Date:** January 2026
**Status:** Specification Phase
**License:** MIT

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Product Vision & Goals](#2-product-vision--goals)
3. [Target Users & Use Cases](#3-target-users--use-cases)
4. [Functional Requirements](#4-functional-requirements)
5. [Technical Architecture](#5-technical-architecture)
6. [Data Structures](#6-data-structures)
7. [API Design](#7-api-design)
8. [Algorithm Specification](#8-algorithm-specification)
9. [Non-Functional Requirements](#9-non-functional-requirements)
10. [Dependencies & Technology Stack](#10-dependencies--technology-stack)
11. [Implementation Roadmap](#11-implementation-roadmap)
12. [Success Metrics](#12-success-metrics)
13. [References](#13-references)

---

## 1. Executive Summary

### 1.1 Project Overview

**Convexfeld** is a comprehensive specification for a production-quality Linear Programming (LP) solver implementing the revised simplex method. The project provides complete, implementation-ready documentation for building an LP solver from scratch, synthesizing decades of academic research in computational optimization.

### 1.2 Current Status

- **Phase:** Specification Complete (No Implementation Code)
- **Documentation:** 65,000+ lines across 180+ files
- **Functions Specified:** 142 (30 public API + 112 internal)
- **Modules:** 17 organized in 6 hierarchical layers
- **Data Structures:** 8 core structures fully documented

### 1.3 Key Deliverables

| Deliverable | Status | Description |
|-------------|--------|-------------|
| Architecture Specification | Complete | System design, data flow, state machines |
| Function Specifications | Complete | 142 functions with full behavioral contracts |
| Data Structure Definitions | Complete | 8 structures with field layouts and lifecycles |
| Module Organization | Complete | 17 modules with dependency ordering |
| API Design | Complete | Public interface with error handling patterns |
| Implementation Code | Not Started | Target: C language |
| Formal Verification | Planned | Target: Lean 4 with mathlib |

### 1.4 Business Value

- **Educational:** Reference implementation for teaching optimization algorithms
- **Research:** Foundation for computational optimization research
- **Commercial:** Blueprint for building competitive LP solvers
- **Open Source:** MIT license enables broad adoption

---

## 2. Product Vision & Goals

### 2.1 Vision Statement

To provide the most comprehensive, academically-grounded, and implementation-ready specification for building a production-quality linear programming solver, enabling developers, researchers, and educators to create correct and efficient LP solvers.

### 2.2 Primary Goals

1. **Correctness:** Mathematically sound algorithms with provable termination
2. **Completeness:** Sufficient detail for faithful implementation without additional research
3. **Performance:** Exploit sparsity for industrial-scale problems (millions of variables)
4. **Robustness:** Handle degenerate, infeasible, and unbounded problems gracefully
5. **Clarity:** Clear documentation suitable for implementation and education

### 2.3 Design Principles

| Principle | Implementation |
|-----------|----------------|
| **Sparsity Exploitation** | CSC matrix format, sparse FTRAN/BTRAN |
| **Numerical Stability** | Harris ratio test, periodic refactorization |
| **Modularity** | Clear separation between pricing, pivot, basis operations |
| **Determinism** | Fixed random seeds, deterministic tie-breaking |
| **Resource Awareness** | Iteration limits, time limits, memory limits |
| **Thread Safety** | Critical sections protect shared resources |

### 2.4 Non-Goals

- MIP (Mixed-Integer Programming) solver implementation
- Interior-point/barrier method (only simplex specified)
- GPU acceleration
- Distributed computing support
- Commercial support infrastructure

---

## 3. Target Users & Use Cases

### 3.1 Target Users

| User Type | Description | Primary Need |
|-----------|-------------|--------------|
| **Graduate Students** | Optimization/OR courses | Learning simplex implementation |
| **Researchers** | Computational optimization | Baseline for experiments |
| **Educators** | Teaching optimization | Reference material |
| **Open Source Developers** | Building solver libraries | Implementation blueprint |
| **Industry Engineers** | Custom optimization tools | Domain-specific solvers |

### 3.2 Use Cases

#### UC-1: Educational Implementation
**Actor:** Graduate student in operations research
**Goal:** Implement simplex method as course project
**Flow:** Student reads specifications, implements in chosen language, validates against test problems

#### UC-2: Research Baseline
**Actor:** Optimization researcher
**Goal:** Establish baseline solver for comparing new algorithms
**Flow:** Researcher implements from spec, runs benchmarks, compares with new approaches

#### UC-3: Formal Verification
**Actor:** Verification researcher
**Goal:** Create formally verified LP solver in Lean 4
**Flow:** Translate specifications to Lean, prove correctness properties, eliminate axioms

#### UC-4: Embedded Solver
**Actor:** Embedded systems engineer
**Goal:** Create lightweight LP solver for resource-constrained environment
**Flow:** Implement subset of specification, optimize for memory footprint

#### UC-5: Teaching Material
**Actor:** University professor
**Goal:** Create lecture materials on simplex algorithm
**Flow:** Extract algorithm descriptions, create slides, assign implementation homework

---

## 4. Functional Requirements

### 4.1 Core Capabilities

#### FR-1: Linear Program Solving
- **FR-1.1:** Solve standard form LPs: minimize c^T x subject to Ax = b, l ≤ x ≤ u
- **FR-1.2:** Handle inequality constraints via slack variables
- **FR-1.3:** Support both minimization and maximization objectives
- **FR-1.4:** Report optimal solution, objective value, and dual prices

#### FR-2: Problem Detection
- **FR-2.1:** Detect and report infeasible problems
- **FR-2.2:** Detect and report unbounded problems
- **FR-2.3:** Detect trivial problems (empty, all-fixed-variables)
- **FR-2.4:** Validate input data (NaN/Inf detection, bound consistency)

#### FR-3: Solution Quality
- **FR-3.1:** Guarantee optimality within user-specified tolerance
- **FR-3.2:** Provide iterative refinement for improved accuracy
- **FR-3.3:** Report solution feasibility violations if any
- **FR-3.4:** Support sensitivity analysis queries

#### FR-4: Resource Management
- **FR-4.1:** Respect iteration limits
- **FR-4.2:** Respect time limits
- **FR-4.3:** Report resource usage statistics
- **FR-4.4:** Support early termination via callbacks

### 4.2 API Requirements

#### FR-5: Model Construction
- **FR-5.1:** Create optimization environment and model instances
- **FR-5.2:** Add variables with bounds and objective coefficients
- **FR-5.3:** Add linear constraints
- **FR-5.4:** Modify existing coefficients and bounds
- **FR-5.5:** Delete variables and constraints

#### FR-6: Model Query
- **FR-6.1:** Query model dimensions (variables, constraints, nonzeros)
- **FR-6.2:** Query solution values (primal, dual, reduced costs, slacks)
- **FR-6.3:** Query solver statistics (iterations, time, status)
- **FR-6.4:** Query basis information

#### FR-7: Parameter Control
- **FR-7.1:** Set/get integer parameters (iteration limit, log level)
- **FR-7.2:** Set/get floating-point parameters (tolerances, time limit)
- **FR-7.3:** Support parameter files for batch configuration

#### FR-8: File I/O
- **FR-8.1:** Read models from MPS format
- **FR-8.2:** Read models from LP format
- **FR-8.3:** Write models to multiple formats
- **FR-8.4:** Write/read solution files

### 4.3 Function Inventory Summary

| Module | Function Count | Purpose |
|--------|----------------|---------|
| Public API | 30 | External interface |
| Memory Management | 9 | Allocation, deallocation |
| Error Handling | 10 | Error codes, logging |
| Validation | 2 | Input validation |
| Parameters | 4 | Parameter access |
| Logging | 5 | Output management |
| Threading | 7 | Concurrency |
| Matrix Operations | 7 | Sparse matrix routines |
| Timing | 5 | Performance measurement |
| Model Analysis | 6 | Statistics collection |
| Basis Operations | 8 | FTRAN, BTRAN, refactorization |
| Callbacks | 6 | User callbacks |
| Solver State | 4 | State management |
| Pricing | 6 | Variable selection |
| Ratio Test | 4 | Pivot selection |
| Pivot Operations | 2 | Basis updates |
| Simplex Core | 21 | Main algorithm |
| Crossover | 2 | IPM to simplex conversion |
| Utilities | 10 | Helper functions |
| **TOTAL** | **142** | |

---

## 5. Technical Architecture

### 5.1 Module Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                    Layer 6: Public API (30)                     │
│              cxf_newmodel, cxf_optimize, cxf_addvar...          │
└─────────────────────────────────────────────────────────────────┘
                               │
┌─────────────────────────────────────────────────────────────────┐
│               Layer 5: High-Level Algorithms (33)               │
│          Simplex Core (21) | Crossover (2) | Utilities (10)     │
└─────────────────────────────────────────────────────────────────┘
                               │
┌─────────────────────────────────────────────────────────────────┐
│               Layer 4: Algorithm Components (10)                │
│                   Pricing (6) | Solver State (4)                │
└─────────────────────────────────────────────────────────────────┘
                               │
┌─────────────────────────────────────────────────────────────────┐
│                Layer 3: Core Data Operations (25)               │
│       Basis (8) | Callbacks (6) | Analysis (6) | Timing (5)     │
└─────────────────────────────────────────────────────────────────┘
                               │
┌─────────────────────────────────────────────────────────────────┐
│                 Layer 2: Data Management (19)                   │
│          Matrix (7) | Threading (7) | Logging (5)               │
└─────────────────────────────────────────────────────────────────┘
                               │
┌─────────────────────────────────────────────────────────────────┐
│                  Layer 1: Error Handling (12)                   │
│                  Error (10) | Validation (2)                    │
└─────────────────────────────────────────────────────────────────┘
                               │
┌─────────────────────────────────────────────────────────────────┐
│                   Layer 0: Foundation (13)                      │
│                Memory (9) | Parameters (4)                      │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 State Machines

#### 5.2.1 Model State Machine

```
UNDEFINED ──▶ CONSTRUCTED ──▶ POPULATED ──▶ UPDATED ──▶ SOLVING ──▶ SOLVED
    │                                           │            │         │
    └───────────────────────────────────────────┴────────────┴─────────┘
                                                      │
                                                      ▼
                                                   FREED
```

| State | Description | Valid Operations |
|-------|-------------|------------------|
| UNDEFINED | Pre-allocation | None |
| CONSTRUCTED | Model created, no data | Add variables/constraints |
| POPULATED | Has data, pending changes | Modify, query dimensions |
| UPDATED | Changes applied | Optimize, query |
| SOLVING | Optimization in progress | Callbacks, terminate |
| SOLVED | Optimization complete | Query solution |
| FREED | Memory released | None |

#### 5.2.2 Solver State Machine

```
IDLE ──▶ INITIALIZING ──▶ CRASHING ──▶ PREPROCESSING ──▶ PHASE_I ──┐
                                                           │        │
                                              ┌────────────┘        │
                                              ▼                     │
                                         PHASE_II ◀────────────────┘
                                              │
                    ┌─────────────────────────┼─────────────────────┐
                    ▼                         ▼                     ▼
               OPTIMAL              INFEASIBLE              UNBOUNDED
                    │                         │                     │
                    └─────────────────────────┼─────────────────────┘
                                              ▼
                                         REFINING ──▶ CLEANUP ──▶ IDLE
```

### 5.3 Data Flow

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│    INPUT     │     │   PROCESS    │     │   OUTPUT     │
│              │     │              │     │              │
│ • Objective  │────▶│ • Presolve   │────▶│ • Primal X   │
│ • Bounds     │     │ • Crash      │     │ • Dual Y     │
│ • Matrix A   │     │ • Iterate    │     │ • Reduced C  │
│ • RHS b      │     │ • Refine     │     │ • Obj Value  │
│              │     │ • Cleanup    │     │ • Status     │
└──────────────┘     └──────────────┘     └──────────────┘
```

---

## 6. Data Structures

### 6.1 Core Structures Summary

| Structure | Size | Purpose | Lifecycle |
|-----------|------|---------|-----------|
| CxfEnv | ~10.5 KB | Solver environment | Application lifetime |
| CxfModel | ~1 KB + data | Problem instance | User controlled |
| SparseMatrix | Variable | Constraint matrix (CSC) | Per model |
| SolverContext | ~1,192 bytes | Iteration state | Per solve |
| BasisState | Variable | LU factorization | Per solve |
| EtaFactors | ~100 bytes each | Basis updates | Per pivot |
| PricingContext | Variable | Variable selection | Per solve |
| CallbackContext | Fixed | User callbacks | Per model |

### 6.2 CxfEnv (Environment)

```c
struct CxfEnv {
    // Memory Management
    void* (*allocator)(size_t);     // Custom allocator
    void (*deallocator)(void*);     // Custom deallocator

    // Parameters
    CxfParams* params;              // Solver parameters

    // Thread Safety
    CxfMutex* globalLock;           // Critical section
    int threadCount;                // Active threads

    // Logging
    int logLevel;                   // 0=silent, 3=verbose
    FILE* logFile;                  // Output destination

    // Error State
    int lastError;                  // Most recent error code
    char errorMsg[256];             // Error message buffer
};
```

**Memory:** ~10.5 KB fixed overhead + parameter storage

### 6.3 CxfModel (Model Instance)

```c
struct CxfModel {
    uint32_t validMagic;            // 0xC0FEFE1D for validation
    CxfEnv* env;                    // Parent environment
    char* name;                     // Model name string

    // Dimensions
    int numVars;                    // Number of variables
    int numConstrs;                 // Number of constraints
    int numNonzeros;                // Nonzero coefficients

    // Problem Data
    SparseMatrix* matrix;           // Constraint matrix
    double* objective;              // Objective coefficients c[n]
    double* lowerBounds;            // Variable lower bounds l[n]
    double* upperBounds;            // Variable upper bounds u[n]
    double* rhs;                    // Right-hand side b[m]
    char* sense;                    // Constraint senses {'<','=','>'}

    // Solution Data
    double* solution;               // Primal solution x[n]
    double* dualValues;             // Dual prices y[m]
    double* reducedCosts;           // Reduced costs c̄[n]
    double* slacks;                 // Slack values s[m]
    double objValue;                // Objective value
    int status;                     // Solution status

    // State Management
    int modificationBlocked;        // True during optimization
    PendingMods* pending;           // Queued modifications
};
```

**Memory:** ~1 KB structure + 44n + 16m + 12·nnz bytes

### 6.4 SparseMatrix (CSC Format)

```c
struct SparseMatrix {
    // Compressed Sparse Column (Primary)
    int* colStart;                  // Column pointers [n+1]
    int* rowIndices;                // Row indices [nnz]
    double* coeffValues;            // Coefficient values [nnz]

    // Dimensions
    int numRows;                    // Number of rows (constraints)
    int numCols;                    // Number of columns (variables)
    int numNonzeros;                // Total nonzeros

    // Lazy CSR (Row-major, built on demand)
    int* rowStart;                  // Row pointers [m+1] (lazy)
    int* colIndices;                // Column indices [nnz] (lazy)
    double* rowCoeffs;              // Coefficients by row [nnz] (lazy)
    int csrValid;                   // CSR cache validity flag
};
```

**Invariants:**
- `colStart[0] = 0`
- `colStart[numCols] = numNonzeros`
- `colStart[i] ≤ colStart[i+1]` for all i
- `rowIndices[j] ∈ [0, numRows)` for all j

### 6.5 SolverContext (Iteration State)

```c
struct SolverContext {
    // Reference
    CxfModel* model;                // Parent model

    // Working Copies (may be perturbed/scaled)
    double* workLower;              // Working lower bounds [n]
    double* workUpper;              // Working upper bounds [n]
    double* workObj;                // Working objective [n]

    // Basis Information
    int* basisHeader;               // Basic variable indices [m]
    int* variableStatus;            // Variable status codes [n]
    BasisState* basis;              // Factorization data

    // Iteration Tracking
    int iteration;                  // Current iteration count
    int phase;                      // 0=undef, 1=Phase I, 2=Phase II
    double currentObj;              // Current objective value

    // Eta Storage
    EtaList* etaList;               // Eta vector linked list
    int etaCount;                   // Eta vectors since refactorization

    // Timing
    TimingState* timing;            // Performance measurements
};
```

**Memory:** ~1,192 bytes fixed + 56n + 24m + eta storage

### 6.6 Variable Status Codes

| Code | Constant | Meaning |
|------|----------|---------|
| ≥ 0 | CXF_BASIC | Basic variable (index = basic row) |
| -1 | CXF_NONBASIC_LOWER | Nonbasic at lower bound |
| -2 | CXF_NONBASIC_UPPER | Nonbasic at upper bound |
| -3 | CXF_SUPERBASIC | Superbasic (between bounds) |
| -4 | CXF_FIXED | Fixed variable (lb = ub) |

---

## 7. API Design

### 7.1 Naming Conventions

- **Prefix:** All public functions use `cxf_` prefix (Convexfeld)
- **Style:** lowercase with underscores: `cxf_add_var`, `cxf_optimize`
- **Consistency:** CRUD pattern for model operations

### 7.2 Environment API

```c
// Create new environment
CxfEnv* cxf_emptyenv(void);
CxfEnv* cxf_loadenv(const char* licFile);

// Free environment
void cxf_freeenv(CxfEnv* env);

// Error handling
const char* cxf_geterrormsg(CxfEnv* env);

// Version info
const char* cxf_version(void);
```

### 7.3 Model API

```c
// Model lifecycle
CxfModel* cxf_newmodel(CxfEnv* env, const char* name);
CxfModel* cxf_copymodel(CxfModel* model);
void cxf_freemodel(CxfModel* model);
int cxf_updatemodel(CxfModel* model);

// Variable operations
int cxf_addvar(CxfModel* model, double obj, double lb, double ub,
               char type, const char* name);
int cxf_addvars(CxfModel* model, int count, /* arrays */);
int cxf_delvars(CxfModel* model, int count, int* indices);

// Constraint operations
int cxf_addconstr(CxfModel* model, int nz, int* vars, double* vals,
                  char sense, double rhs, const char* name);
int cxf_addconstrs(CxfModel* model, int count, /* arrays */);

// Coefficient modification
int cxf_chgcoeffs(CxfModel* model, int count, int* rows, int* cols,
                  double* vals);

// Optimization
int cxf_optimize(CxfModel* model);
int cxf_terminate(CxfModel* model);
```

### 7.4 Query API

```c
// Integer attributes
int cxf_getintattr(CxfModel* model, const char* name, int* value);
// Attributes: NumVars, NumConstrs, NumNZs, Status, IterCount

// Double attributes
int cxf_getdblattr(CxfModel* model, const char* name, double* value);
// Attributes: ObjVal, Runtime, ObjBound

// Array attributes
int cxf_getdblattrarray(CxfModel* model, const char* name,
                        int start, int len, double* values);
// Attributes: X, Pi, RC, Slack, VBasis, CBasis
```

### 7.5 Parameter API

```c
// Integer parameters
int cxf_setintparam(CxfEnv* env, const char* name, int value);
int cxf_getintparam(CxfEnv* env, const char* name, int* value);
// Parameters: IterLimit, LogLevel, Threads, Presolve

// Double parameters
int cxf_setdblparam(CxfEnv* env, const char* name, double value);
int cxf_getdblparam(CxfEnv* env, const char* name, double* value);
// Parameters: TimeLimit, FeasTol, OptTol, PivotTol
```

### 7.6 File I/O API

```c
int cxf_read(CxfModel* model, const char* filename);   // MPS, LP
int cxf_write(CxfModel* model, const char* filename);  // MPS, LP, SOL
int cxf_readparams(CxfEnv* env, const char* filename);
int cxf_writeparams(CxfEnv* env, const char* filename);
```

### 7.7 Return Codes

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | CXF_OK | Success |
| 1 | CXF_OPTIMAL | Optimal solution found |
| 2 | CXF_INFEASIBLE | Problem is infeasible |
| 3 | CXF_UNBOUNDED | Problem is unbounded |
| 9 | CXF_TIME_LIMIT | Time limit reached |
| 10 | CXF_ITERATION_LIMIT | Iteration limit reached |
| 11 | CXF_INTERRUPTED | User termination |
| 1001 | CXF_ERR_MEMORY | Out of memory |
| 1002 | CXF_ERR_NULL | NULL pointer argument |
| 1003 | CXF_ERR_INVALID | Invalid argument |

---

## 8. Algorithm Specification

### 8.1 Algorithm Selection

**Primary:** Revised Simplex Method with Product Form of Inverse (PFI)

**Rationale:**
- Industry standard for sparse LP problems
- Space efficient: O(m² + nnz) vs O(mn) for full tableau
- Exploits sparsity naturally
- Despite exponential worst-case, polynomial on real-world problems
- Well-understood numerical stability properties

### 8.2 Two-Phase Simplex Method

```
┌─────────────────────────────────────────────────────────────────┐
│                          PHASE I                                │
│                                                                 │
│  Objective: Minimize sum of artificial variables                │
│  Goal: Find a basic feasible solution (BFS)                     │
│                                                                 │
│  IF Phase I objective = 0 THEN                                  │
│      Remove artificials, restore original objective             │
│      GOTO Phase II                                              │
│  ELSE                                                           │
│      RETURN INFEASIBLE                                          │
│  END IF                                                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                          PHASE II                               │
│                                                                 │
│  Objective: Minimize original objective                         │
│  Goal: Achieve optimality                                       │
│                                                                 │
│  WHILE NOT optimal DO                                           │
│      Select entering variable (pricing)                         │
│      Compute pivot column (FTRAN)                               │
│      Select leaving variable (ratio test)                       │
│      IF no leaving variable THEN RETURN UNBOUNDED               │
│      Update basis (pivot)                                       │
│  END WHILE                                                      │
│                                                                 │
│  RETURN OPTIMAL                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 8.3 Core Operations

#### 8.3.1 FTRAN (Forward Transformation)

**Purpose:** Solve Bx = a (compute pivot column)

**Input:** Column vector a (from constraint matrix)
**Output:** x = B⁻¹a (transformed column)

**Algorithm:**
1. Apply L factor: solve Lz = a (forward substitution)
2. Apply U factor: solve Ux = z (backward substitution)
3. Apply eta vectors: x ← E_k · E_{k-1} · ... · E_1 · x

**Complexity:** O(m·fill_L + m·fill_U + k·avg_eta_nz)

#### 8.3.2 BTRAN (Backward Transformation)

**Purpose:** Solve B^T y = e_i (compute pricing row)

**Input:** Unit vector e_i
**Output:** y = (B^{-T})e_i (transformed row)

**Algorithm:**
1. Initialize y as unit vector
2. Apply eta vectors in **reverse** order
3. Solve (U^T)z = y (forward substitution)
4. Solve (L^T)y = z (backward substitution)

**Complexity:** Same as FTRAN

#### 8.3.3 Harris Two-Pass Ratio Test

**Purpose:** Select numerically stable leaving variable

**Algorithm:**
```
PASS 1: Compute minimum ratio with relaxed tolerance (10× feasTol)
        θ_relax = min{(bound - x_i) / d_i : d_i has correct sign}

        IF no finite ratio THEN RETURN UNBOUNDED

PASS 2: Among ratios within feasTol of θ_relax,
        Select variable with largest |d_i| (pivot element)
```

**Benefit:** Prevents tiny pivots that cause numerical instability

### 8.4 Pricing Strategies

#### 8.4.1 Steepest-Edge Pricing

**Criterion:** Select variable j maximizing |d_j| / √(γ_j)

where:
- d_j = reduced cost of variable j
- γ_j = ||B⁻¹ a_j||² (edge weight)

**Advantages:**
- 20-30% fewer iterations than Dantzig rule
- Better for difficult/degenerate problems

#### 8.4.2 Partial Pricing

- Divide variables into chunks
- Scan one chunk per iteration
- Rotate through all chunks
- Reduces O(n) pricing to O(n/k) per iteration

### 8.5 Anti-Cycling: Perturbation Method

**Purpose:** Guarantee finite termination on degenerate problems

**Algorithm:**
1. Add small random perturbations to variable bounds
2. Perturbation scale: feasTol × 10⁻⁶
3. Apply during first ~100 iterations
4. Remove perturbations before final solution

**Reference:** Wolfe (1963)

### 8.6 Basis Maintenance

**Product Form of Inverse (PFI):**
- Represent B⁻¹ = E_k · E_{k-1} · ... · E_1 · B₀⁻¹
- Each E_i is sparse elementary matrix (one pivot)
- Storage: ~100 bytes per eta vector

**Refactorization:**
- Trigger: Every 100-200 pivots
- Action: Fresh LU factorization of current basis
- Benefit: Controls condition number growth

### 8.7 Complexity Analysis

**Per Iteration:**
| Operation | Time Complexity |
|-----------|-----------------|
| BTRAN | O(m·fill) |
| Full Pricing | O(n) |
| Partial Pricing | O(n/k) |
| FTRAN | O(m·fill + k·eta_nz) |
| Ratio Test | O(m) sparse, O(col_nz) if sparse column |
| Pivot Update | O(m + eta_nz) |

**Overall:**
- Typical iterations: 0.5m to 10m (problem-dependent)
- Refactorizations: 5-100 per solve
- Memory: O(m² + nnz) for factors, O(m + n) for working arrays

---

## 9. Non-Functional Requirements

### 9.1 Performance Requirements

| Metric | Requirement |
|--------|-------------|
| Problem Size | Support up to 10⁶ variables, 10⁶ constraints |
| Sparsity | Exploit typical 0.1-1% sparsity |
| Memory | < 250 MB for 1M variable problems |
| Iterations | Competitive with commercial solvers |

### 9.2 Numerical Stability

| Parameter | Default Value |
|-----------|---------------|
| Feasibility Tolerance | 10⁻⁶ |
| Optimality Tolerance | 10⁻⁶ |
| Pivot Tolerance | 10⁻¹⁰ |
| Zero Threshold | 10⁻¹⁰ |
| Refactorization Frequency | 100-200 pivots |

### 9.3 Robustness Requirements

- **Degeneracy:** Handle via perturbation method
- **Numerical Issues:** Detect near-singular bases, trigger refactorization
- **Input Validation:** Reject NaN/Inf, inconsistent bounds
- **Resource Limits:** Graceful termination on limits

### 9.4 Thread Safety

- **Environment:** Fully thread-safe (mutex-protected)
- **Model:** Not thread-safe (single owner required)
- **Multiple Models:** Concurrent optimization of different models supported

### 9.5 Error Handling

- All functions return error codes
- Error messages stored in environment
- Cleanup guaranteed on all error paths
- No exceptions (C API)

---

## 10. Dependencies & Technology Stack

### 10.1 Implementation Language

**Target:** C (C99 standard)

**Rationale:**
- Performance-critical application
- Wide portability
- Standard in optimization community
- Easy FFI for other languages

### 10.2 External Dependencies

| Dependency | Purpose | Required |
|------------|---------|----------|
| Standard C Library | Memory, math, I/O | Yes |
| POSIX Threads | Threading (optional) | Optional |
| BLAS | Dense operations (if needed) | Optional |

### 10.3 Development Tools

| Tool | Purpose |
|------|---------|
| GCC/Clang | C compilation |
| CMake/Make | Build system |
| Valgrind | Memory debugging |
| GDB | Debugging |
| Lean 4 | Formal verification (future) |

### 10.4 Formal Verification Stack

For formal verification in Lean 4:

| Component | Purpose |
|-----------|---------|
| Lean 4 | Proof assistant |
| Mathlib | Mathematical foundations |
| Lake | Build system |
| LeanSearch | Theorem discovery |

**Standard Axioms (Acceptable):**
- `Classical.choice` (axiom of choice)
- `propext` (propositional extensionality)
- `Quot.sound` (quotient soundness)

---

## 11. Implementation Roadmap

### 11.1 Phase 1: Foundation (Layers 0-1)

**Modules:**
- Memory Management (9 functions)
- Parameters (4 functions)
- Validation (2 functions)
- Error Handling (10 functions)

**Deliverables:**
- Memory allocation wrappers
- Parameter storage and access
- Error code infrastructure
- Logging framework

### 11.2 Phase 2: Data Layer (Layer 2)

**Modules:**
- Matrix Operations (7 functions)
- Threading (7 functions)
- Logging (5 functions)

**Deliverables:**
- CSC sparse matrix implementation
- Thread pool (optional)
- Configurable logging

### 11.3 Phase 3: Core Operations (Layer 3)

**Modules:**
- Basis Operations (8 functions)
- Callbacks (6 functions)
- Model Analysis (6 functions)
- Timing (5 functions)

**Deliverables:**
- FTRAN/BTRAN implementation
- LU factorization
- Eta vector management
- Callback infrastructure

### 11.4 Phase 4: Algorithm Components (Layer 4)

**Modules:**
- Pricing (6 functions)
- Solver State (4 functions)
- Ratio Test (4 functions)
- Pivot Operations (2 functions)

**Deliverables:**
- Steepest-edge pricing
- Harris ratio test
- Pivot execution

### 11.5 Phase 5: Simplex Core (Layer 5)

**Modules:**
- Simplex Core (21 functions)
- Crossover (2 functions)
- Utilities (10 functions)

**Deliverables:**
- Complete simplex iteration loop
- Two-phase method
- Perturbation handling
- Solution refinement

### 11.6 Phase 6: Public API (Layer 6)

**Modules:**
- Public API (30 functions)

**Deliverables:**
- Complete user-facing API
- File I/O (MPS, LP formats)
- Documentation
- Examples

### 11.7 Phase 7: Verification (Optional)

**Activities:**
- Lean 4 formalization
- Correctness proofs
- Axiom elimination

---

## 12. Success Metrics

### 12.1 Correctness Metrics

| Metric | Target |
|--------|--------|
| Netlib Test Set | 100% correct solutions |
| Infeasibility Detection | 100% accuracy |
| Unboundedness Detection | 100% accuracy |
| Solution Accuracy | Within 10⁻⁶ of optimal |

### 12.2 Performance Metrics

| Metric | Target |
|--------|--------|
| Netlib Solve Time | Within 10× of commercial solvers |
| Memory Usage | < 2× problem size |
| Iteration Count | Within 2× of commercial solvers |

### 12.3 Quality Metrics

| Metric | Target |
|--------|--------|
| Test Coverage | > 90% line coverage |
| Documentation | 100% public API documented |
| Static Analysis | Zero critical warnings |

---

## 13. References

### 13.1 Foundational Literature

1. **Dantzig, G.B.** (1963). *Linear Programming and Extensions*. Princeton University Press.

2. **Chvátal, V.** (1983). *Linear Programming*. W.H. Freeman.

3. **Maros, I.** (2003). *Computational Techniques of the Simplex Method*. Springer.

### 13.2 Algorithmic References

4. **Bartels, R.H. & Golub, G.H.** (1969). The Simplex Method of Linear Programming Using LU Decomposition. *Communications of the ACM*, 12(5), 266-268.

5. **Harris, P.M.J.** (1973). Pivot Selection Methods of the Devex LP Code. *Mathematical Programming*, 5(1), 1-28.

6. **Forrest, J.J. & Goldfarb, D.** (1992). Steepest-Edge Simplex Algorithms for Linear Programming. *Mathematical Programming*, 57(1), 341-374.

7. **Wolfe, P.** (1963). A Technique for Resolving Degeneracy in Linear Programming. *Journal of the SIAM*, 11(2), 205-211.

### 13.3 Implementation References

8. **Forrest, J.J. & Tomlin, J.A.** (1972). Updated Triangular Factors of the Basis to Maintain Sparsity in the Product Form Simplex Method. *Mathematical Programming*, 2(1), 263-278.

9. **Suhl, U.H. & Suhl, L.M.** (1990). Computing Sparse LU Factorizations for Large-Scale Linear Programming Bases. *ORSA Journal on Computing*, 2(4), 325-335.

---

## Appendix A: Document Inventory

| Document Type | Count | Location |
|---------------|-------|----------|
| Architecture | 3 | `docs/specs/architecture/` |
| Module Specs | 17 | `docs/specs/modules/` |
| Function Specs | 142 | `docs/specs/functions/` |
| Structure Specs | 8 | `docs/specs/structures/` |
| Inventory | 2 | `docs/inventory/` |

---

## Appendix B: Glossary

| Term | Definition |
|------|------------|
| **Basic Variable** | Variable in the basis (part of BFS) |
| **BFS** | Basic Feasible Solution |
| **BTRAN** | Backward Transformation (B^T y = c) |
| **CSC** | Compressed Sparse Column matrix format |
| **Degeneracy** | Multiple basic variables at bounds |
| **Eta Vector** | Sparse representation of basis update |
| **FTRAN** | Forward Transformation (Bx = a) |
| **LP** | Linear Programming |
| **PFI** | Product Form of Inverse |
| **Pivot** | Basis exchange operation |
| **Reduced Cost** | c̄_j = c_j - y^T A_j |
| **Simplex** | Algorithm for solving LPs |

---

*Document generated by comprehensive analysis of the Convexfeld specification repository.*
