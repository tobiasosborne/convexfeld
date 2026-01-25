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
10. [Test Driven Design & Quality Assurance](#10-test-driven-design--quality-assurance)
11. [Dependencies & Technology Stack](#11-dependencies--technology-stack)
12. [Implementation Roadmap](#12-implementation-roadmap)
13. [Success Metrics](#13-success-metrics)
14. [References](#14-references)

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
| **Test Suite** | Not Started | ~1,000 tests, 1:3-1:4 test:code ratio (TDD) |
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
| **Test Driven Design** | Tests written FIRST; 1:3-1:4 test:code ratio |
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

## 10. Test Driven Design & Quality Assurance

### 10.1 TDD Methodology

**CRITICAL REQUIREMENT:** This project follows strict Test Driven Design (TDD) principles. Tests are written BEFORE implementation code.

#### 10.1.1 TDD Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│                      TDD CYCLE (Red-Green-Refactor)             │
│                                                                 │
│   1. RED:    Write failing test for new functionality           │
│   2. GREEN:  Write minimal code to pass the test                │
│   3. REFACTOR: Improve code while keeping tests green           │
│   4. REPEAT: Move to next test case                             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

#### 10.1.2 Implementation Order

For each function specification:

1. **Write unit tests** covering:
   - Normal operation (happy path)
   - Edge cases (boundary conditions)
   - Error conditions (invalid inputs)
   - Numerical edge cases (tolerances, near-zero values)

2. **Verify tests fail** (no implementation exists)

3. **Implement function** to pass tests

4. **Refactor** while maintaining green tests

5. **Add integration tests** connecting to other modules

### 10.2 Test Coverage Requirements

#### 10.2.1 Code Ratio Target

| Metric | Target Ratio | Description |
|--------|--------------|-------------|
| **Test:Implementation** | **1:3 to 1:4** | 1 line of test code per 3-4 lines of implementation |
| **Minimum Coverage** | **90%** | Line coverage across all modules |
| **Branch Coverage** | **85%** | Decision point coverage |
| **Function Coverage** | **100%** | Every function has at least one test |

#### 10.2.2 Coverage by Module

| Module | Functions | Min Tests | Est. Test LOC | Priority |
|--------|-----------|-----------|---------------|----------|
| Memory Management | 9 | 45 | 400 | P0 |
| Error Handling | 10 | 50 | 450 | P0 |
| Validation | 2 | 20 | 200 | P0 |
| Parameters | 4 | 24 | 200 | P0 |
| Matrix Operations | 7 | 70 | 800 | P1 |
| Basis Operations | 8 | 120 | 1,500 | P1 |
| Pricing | 6 | 60 | 700 | P1 |
| Ratio Test | 4 | 80 | 900 | P1 |
| Simplex Core | 21 | 200 | 2,500 | P2 |
| Public API | 30 | 180 | 2,000 | P2 |
| **TOTAL** | **142** | **~1,000** | **~12,000** | |

### 10.3 Test Categories

#### 10.3.1 Unit Tests

**Purpose:** Test individual functions in isolation

**Characteristics:**
- Mock all dependencies
- Fast execution (< 10ms per test)
- Deterministic (no randomness unless testing RNG)
- Independent (no test order dependency)

**Coverage per function:**

| Category | Tests Required | Example |
|----------|----------------|---------|
| Happy path | 2-3 | Normal inputs, expected outputs |
| Boundary values | 3-5 | Min/max values, empty inputs, single element |
| Error conditions | 2-4 | NULL pointers, invalid indices, out of memory |
| Numerical precision | 2-3 | Near-tolerance values, denormalized floats |

#### 10.3.2 Edge Case Tests

**Mandatory edge cases for LP solver:**

| Edge Case | Test Description | Expected Behavior |
|-----------|------------------|-------------------|
| **Empty problem** | n=0, m=0 | Return OPTIMAL immediately |
| **Single variable** | n=1, m=0 | Trivial optimization |
| **Single constraint** | n>0, m=1 | Single-row LP |
| **All fixed variables** | lb[i] = ub[i] ∀i | Verify feasibility only |
| **Unbounded problem** | No finite optimum | Return UNBOUNDED + ray |
| **Infeasible problem** | No feasible solution | Return INFEASIBLE |
| **Highly degenerate** | Many variables at bounds | No cycling, finite termination |
| **Dense matrix** | >10% nonzeros | Correct but slower |
| **Extremely sparse** | <0.01% nonzeros | Exploit sparsity |
| **Large coefficients** | |a_ij| > 10^9 | Scaling/numerical stability |
| **Tiny coefficients** | |a_ij| < 10^-9 | Zero detection |
| **Mixed scales** | Ratio > 10^12 | Scaling required |
| **Parallel constraints** | Redundant rows | Detect/remove |
| **Free variables** | lb = -∞, ub = +∞ | Handle unbounded vars |
| **Integer bounds** | lb, ub ∈ Z | No special handling (LP) |
| **Near-optimal start** | Warm start close to optimal | Fast convergence |
| **Far-from-optimal start** | Crash basis far from optimal | Eventually optimal |

#### 10.3.3 Integration Tests

**Purpose:** Test interactions between modules

**Test Suites:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    INTEGRATION TEST HIERARCHY                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Level 1: Module Pairs                                          │
│  ├── Memory + Error Handling                                    │
│  ├── Matrix + Basis Operations                                  │
│  ├── Pricing + Ratio Test                                       │
│  └── Basis + Pivot Operations                                   │
│                                                                 │
│  Level 2: Subsystems                                            │
│  ├── Data Layer (Matrix + Memory + Validation)                  │
│  ├── Algorithm Core (Pricing + Ratio + Pivot + Basis)           │
│  └── API Layer (Public API + Error + Logging)                   │
│                                                                 │
│  Level 3: End-to-End                                            │
│  ├── Complete solve cycle (model → optimize → solution)         │
│  ├── File I/O round-trip (read → solve → write → read)          │
│  ├── Warm start (solve → modify → re-solve)                     │
│  └── Multi-model (multiple models, shared environment)          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

#### 10.3.4 Regression Tests

**Purpose:** Prevent reintroduction of fixed bugs

**Requirements:**
- Every bug fix MUST include a regression test
- Test must fail before fix, pass after
- Document bug ID in test name/comment
- Never delete regression tests

#### 10.3.5 Benchmark Tests

**Purpose:** Detect performance regressions

**Standard Benchmarks:**

| Benchmark | Source | Problems | Purpose |
|-----------|--------|----------|---------|
| Netlib LP | netlib.org | 95 | Correctness + performance baseline |
| Kennington | netlib.org | 17 | Network flow problems |
| Miplib (LP relaxations) | miplib.zib.de | 50 | Difficult LP relaxations |
| Custom synthetic | Generated | 100 | Stress tests (size, sparsity, degeneracy) |

### 10.4 Test Specifications by Module

#### 10.4.1 Memory Management Tests

```c
// Unit tests for cxf_malloc
TEST(Memory, MallocZeroBytes)           // Edge: size = 0
TEST(Memory, MallocSmallAllocation)     // Normal: 64 bytes
TEST(Memory, MallocLargeAllocation)     // Normal: 1GB
TEST(Memory, MallocOverflow)            // Edge: size > SIZE_MAX/2
TEST(Memory, MallocAfterOOM)            // Error: simulated OOM
TEST(Memory, MallocAlignment)           // Verify 16-byte alignment
TEST(Memory, MallocTrackingAccuracy)    // Integration: memory accounting

// Unit tests for cxf_free
TEST(Memory, FreeNull)                  // Edge: NULL pointer
TEST(Memory, FreeValidPointer)          // Normal: allocated memory
TEST(Memory, DoubleFree)                // Error: detect double-free
TEST(Memory, FreeTrackingAccuracy)      // Integration: memory accounting

// Similar pattern for realloc, calloc...
```

#### 10.4.2 Sparse Matrix Tests

```c
// Construction tests
TEST(Matrix, CreateEmpty)               // Edge: 0x0 matrix
TEST(Matrix, CreateSingleElement)       // Edge: 1x1 matrix
TEST(Matrix, CreateFromCOO)             // Normal: COO → CSC conversion
TEST(Matrix, CreateFromDense)           // Normal: dense → CSC
TEST(Matrix, CreateWithDuplicates)      // Edge: duplicate entries (sum)

// Access tests
TEST(Matrix, GetColumnEmpty)            // Edge: empty column
TEST(Matrix, GetColumnSparse)           // Normal: sparse column
TEST(Matrix, GetColumnDense)            // Normal: dense column
TEST(Matrix, GetRowLazy)                // Integration: CSR construction
TEST(Matrix, GetCoeffExisting)          // Normal: existing entry
TEST(Matrix, GetCoeffNonexisting)       // Normal: zero entry

// Modification tests
TEST(Matrix, AddColumnToEmpty)          // Edge: first column
TEST(Matrix, AddColumnWithGrowth)       // Normal: reallocation
TEST(Matrix, ModifyCoeffExisting)       // Normal: change value
TEST(Matrix, ModifyCoeffNew)            // Normal: add new nonzero
TEST(Matrix, DeleteColumn)              // Normal: remove column
TEST(Matrix, TransposeSymmetric)        // Edge: symmetric matrix
TEST(Matrix, TransposeRectangular)      // Normal: m ≠ n

// Numerical edge cases
TEST(Matrix, CoeffNearZero)             // Edge: |a| < 10^-15
TEST(Matrix, CoeffVeryLarge)            // Edge: |a| > 10^15
TEST(Matrix, CoeffSubnormal)            // Edge: denormalized float
TEST(Matrix, CoeffInfNaN)               // Error: reject Inf/NaN
```

#### 10.4.3 Basis Operations Tests

```c
// FTRAN tests
TEST(Basis, FtranIdentity)              // Edge: B = I, verify x = a
TEST(Basis, FtranSinglePivot)           // Normal: one eta vector
TEST(Basis, FtranManyEtas)              // Normal: 100 eta vectors
TEST(Basis, FtranNearSingular)          // Edge: condition number > 10^12
TEST(Basis, FtranSparseColumn)          // Normal: sparse RHS
TEST(Basis, FtranDenseColumn)           // Normal: dense RHS
TEST(Basis, FtranZeroColumn)            // Edge: all-zero RHS
TEST(Basis, FtranAccuracy)              // Numerical: ||Bx - a|| < tol

// BTRAN tests (mirror FTRAN)
TEST(Basis, BtranIdentity)
TEST(Basis, BtranSinglePivot)
TEST(Basis, BtranManyEtas)
TEST(Basis, BtranAccuracy)              // Numerical: ||B^T y - c|| < tol

// Refactorization tests
TEST(Basis, RefactorFresh)              // Normal: initial factorization
TEST(Basis, RefactorAfterPivots)        // Normal: after 100 pivots
TEST(Basis, RefactorSingular)           // Error: singular basis
TEST(Basis, RefactorPreservesResult)    // Integration: FTRAN same before/after

// Eta vector tests
TEST(Basis, EtaAddSparse)               // Normal: sparse eta
TEST(Basis, EtaAddDense)                // Edge: dense eta (column)
TEST(Basis, EtaOverflow)                // Error: too many etas
```

#### 10.4.4 Simplex Algorithm Tests

```c
// Phase I tests
TEST(Simplex, PhaseIFeasible)           // Normal: finds BFS
TEST(Simplex, PhaseIInfeasible)         // Normal: detects infeasibility
TEST(Simplex, PhaseIDegenerate)         // Edge: degenerate at start
TEST(Simplex, PhaseIArtificialRemoval)  // Integration: clean up artificials

// Phase II tests
TEST(Simplex, PhaseIIOptimal)           // Normal: finds optimum
TEST(Simplex, PhaseIIUnbounded)         // Normal: detects unboundedness
TEST(Simplex, PhaseIICycling)           // Edge: cycling problem (Klee-Minty)
TEST(Simplex, PhaseIIDegenerate)        // Edge: many degenerate pivots

// Pricing tests
TEST(Simplex, PricingSteepestEdge)      // Normal: SE pricing
TEST(Simplex, PricingDantzig)           // Normal: largest reduced cost
TEST(Simplex, PricingPartial)           // Normal: partial pricing
TEST(Simplex, PricingAllOptimal)        // Edge: all reduced costs optimal
TEST(Simplex, PricingTieBreaking)       // Edge: equal reduced costs

// Ratio test tests
TEST(Simplex, RatioTestNormal)          // Normal: clear winner
TEST(Simplex, RatioTestDegenerate)      // Edge: ties in ratio
TEST(Simplex, RatioTestUnbounded)       // Normal: all ratios infinite
TEST(Simplex, RatioTestHarris)          // Normal: Harris two-pass
TEST(Simplex, RatioTestNumerical)       // Edge: near-zero pivot elements
```

#### 10.4.5 Public API Tests

```c
// Environment tests
TEST(API, CreateEnvDefault)             // Normal: default environment
TEST(API, CreateEnvCustomAllocator)     // Normal: custom memory
TEST(API, FreeEnvWithModels)            // Integration: cleanup cascade
TEST(API, EnvThreadSafety)              // Integration: concurrent access

// Model lifecycle tests
TEST(API, CreateModelEmpty)             // Normal: empty model
TEST(API, CreateModelNamed)             // Normal: with name
TEST(API, CopyModelDeep)                // Normal: independent copy
TEST(API, FreeModelCleanup)             // Normal: memory release

// Variable operations tests
TEST(API, AddVarSingle)                 // Normal: one variable
TEST(API, AddVarsBatch)                 // Normal: batch add
TEST(API, AddVarInvalidBounds)          // Error: lb > ub
TEST(API, AddVarInfBounds)              // Normal: infinite bounds
TEST(API, DeleteVarExisting)            // Normal: remove variable
TEST(API, DeleteVarInvalid)             // Error: invalid index

// Constraint operations tests
TEST(API, AddConstrSingle)              // Normal: one constraint
TEST(API, AddConstrsBatch)              // Normal: batch add
TEST(API, AddConstrEmpty)               // Edge: zero coefficients
TEST(API, AddConstrDuplicate)           // Edge: same variable twice

// Optimization tests
TEST(API, OptimizeTrivial)              // Edge: empty/trivial model
TEST(API, OptimizeSmall)                // Normal: small LP
TEST(API, OptimizeLarge)                // Normal: 10k vars
TEST(API, OptimizeWithCallback)         // Integration: progress callback
TEST(API, OptimizeTerminate)            // Integration: early termination
TEST(API, OptimizeTimeLimit)            // Normal: time limit
TEST(API, OptimizeIterLimit)            // Normal: iteration limit
```

### 10.5 Test Infrastructure

#### 10.5.1 Test Framework

**Recommended:** Google Test (gtest) or Unity (for pure C)

```c
// Example test structure
#include "cxf_test.h"

TEST_GROUP(Memory) {
    CxfEnv* env;

    void setup() {
        env = cxf_emptyenv();
    }

    void teardown() {
        cxf_freeenv(env);
    }
};

TEST(Memory, MallocSmallAllocation) {
    void* ptr = cxf_malloc(env, 64);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(cxf_get_allocated(env), 64);
    cxf_free(env, ptr);
    ASSERT_EQ(cxf_get_allocated(env), 0);
}
```

#### 10.5.2 Test Organization

```
tests/
├── unit/
│   ├── test_memory.c           # Memory management unit tests
│   ├── test_matrix.c           # Sparse matrix unit tests
│   ├── test_basis.c            # Basis operations unit tests
│   ├── test_pricing.c          # Pricing unit tests
│   ├── test_ratio.c            # Ratio test unit tests
│   ├── test_simplex.c          # Simplex core unit tests
│   └── test_api.c              # Public API unit tests
├── integration/
│   ├── test_data_layer.c       # Matrix + Memory integration
│   ├── test_algorithm.c        # Pricing + Ratio + Pivot integration
│   └── test_end_to_end.c       # Complete solve cycle
├── edge_cases/
│   ├── test_empty_problems.c   # Empty/trivial LPs
│   ├── test_degeneracy.c       # Degenerate problems
│   ├── test_numerical.c        # Numerical edge cases
│   └── test_large_scale.c      # Large problems
├── benchmarks/
│   ├── netlib/                 # Netlib test problems
│   ├── synthetic/              # Generated test problems
│   └── benchmark_runner.c      # Performance measurement
├── regression/
│   └── test_regressions.c      # Bug regression tests
├── fixtures/
│   ├── small_lps.h             # Small test problems
│   ├── netlib_subset.h         # Netlib problem data
│   └── generators.c            # Problem generators
└── mocks/
    ├── mock_memory.c           # Memory allocation mock
    └── mock_file.c             # File I/O mock
```

#### 10.5.3 Continuous Integration

```yaml
# .github/workflows/test.yml
name: Test Suite

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: cmake -B build && cmake --build build
      - name: Run Unit Tests
        run: ctest --test-dir build --output-on-failure -L unit
      - name: Upload Coverage
        run: gcov -b build/CMakeFiles/*.dir/*.c.gcno

  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: cmake -B build && cmake --build build
      - name: Run Integration Tests
        run: ctest --test-dir build --output-on-failure -L integration

  edge-case-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: cmake -B build && cmake --build build
      - name: Run Edge Case Tests
        run: ctest --test-dir build --output-on-failure -L edge

  benchmark-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build Release
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
      - name: Run Netlib Suite
        run: ./build/benchmark_netlib --json > netlib_results.json
      - name: Check Performance Regression
        run: python3 scripts/check_perf_regression.py netlib_results.json
```

### 10.6 Test Metrics & Reporting

#### 10.6.1 Required Metrics

| Metric | Threshold | Action if Below |
|--------|-----------|-----------------|
| Line Coverage | ≥ 90% | Block merge |
| Branch Coverage | ≥ 85% | Block merge |
| Function Coverage | 100% | Block merge |
| Test Pass Rate | 100% | Block merge |
| Test:Code Ratio | ≥ 1:4 | Warning |
| Mutation Score | ≥ 80% | Warning |

#### 10.6.2 Test Reports

```
================================================================================
                         CONVEXFELD TEST REPORT
================================================================================
Date: 2026-01-25
Commit: abc123def
Branch: feature/basis-operations

SUMMARY
-------
Total Tests:     1,247
Passed:          1,245
Failed:              2
Skipped:             0
Duration:        45.3s

COVERAGE
--------
Lines:           92.4% (8,234 / 8,912)
Branches:        87.1% (2,145 / 2,463)
Functions:      100.0% (142 / 142)

TEST:CODE RATIO
---------------
Test LOC:       11,847
Impl LOC:       38,234
Ratio:          1:3.23 ✓

FAILED TESTS
------------
[FAIL] Basis.FtranNearSingular
       Expected: result within tolerance
       Actual:   ||Bx - a|| = 1.2e-5 (tolerance: 1e-6)

[FAIL] API.OptimizeTimeLimit
       Expected: CXF_TIME_LIMIT
       Actual:   CXF_OPTIMAL (finished before limit)

NETLIB BENCHMARK
----------------
Problems solved: 95/95 (100%)
Geomean iterations: 1.8x commercial baseline
Geomean time: 2.3x commercial baseline
================================================================================
```

### 10.7 Test-First Development Checklist

For each function implementation:

- [ ] Read function specification
- [ ] Write happy-path tests (2-3 tests)
- [ ] Write boundary condition tests (3-5 tests)
- [ ] Write error condition tests (2-4 tests)
- [ ] Write numerical edge case tests (if applicable)
- [ ] Verify all tests FAIL (red)
- [ ] Implement function
- [ ] Verify all tests PASS (green)
- [ ] Refactor if needed (keep green)
- [ ] Check coverage ≥ 90%
- [ ] Add integration tests connecting to dependent modules
- [ ] Update test documentation

---

## 11. Dependencies & Technology Stack

### 11.1 Implementation Language

**Target:** C (C99 standard)

**Rationale:**
- Performance-critical application
- Wide portability
- Standard in optimization community
- Easy FFI for other languages

### 11.2 External Dependencies

| Dependency | Purpose | Required |
|------------|---------|----------|
| Standard C Library | Memory, math, I/O | Yes |
| POSIX Threads | Threading (optional) | Optional |
| BLAS | Dense operations (if needed) | Optional |

### 11.3 Development Tools

| Tool | Purpose |
|------|---------|
| GCC/Clang | C compilation |
| CMake/Make | Build system |
| Valgrind | Memory debugging |
| GDB | Debugging |
| Lean 4 | Formal verification (future) |

### 11.4 Formal Verification Stack

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

## 12. Implementation Roadmap

### 12.1 Phase 1: Foundation (Layers 0-1)

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

### 12.2 Phase 2: Data Layer (Layer 2)

**Modules:**
- Matrix Operations (7 functions)
- Threading (7 functions)
- Logging (5 functions)

**Deliverables:**
- CSC sparse matrix implementation
- Thread pool (optional)
- Configurable logging

### 12.3 Phase 3: Core Operations (Layer 3)

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

### 12.4 Phase 4: Algorithm Components (Layer 4)

**Modules:**
- Pricing (6 functions)
- Solver State (4 functions)
- Ratio Test (4 functions)
- Pivot Operations (2 functions)

**Deliverables:**
- Steepest-edge pricing
- Harris ratio test
- Pivot execution

### 12.5 Phase 5: Simplex Core (Layer 5)

**Modules:**
- Simplex Core (21 functions)
- Crossover (2 functions)
- Utilities (10 functions)

**Deliverables:**
- Complete simplex iteration loop
- Two-phase method
- Perturbation handling
- Solution refinement

### 12.6 Phase 6: Public API (Layer 6)

**Modules:**
- Public API (30 functions)

**Deliverables:**
- Complete user-facing API
- File I/O (MPS, LP formats)
- Documentation
- Examples

### 12.7 Phase 7: Verification (Optional)

**Activities:**
- Lean 4 formalization
- Correctness proofs
- Axiom elimination

---

## 13. Success Metrics

### 13.1 Correctness Metrics

| Metric | Target |
|--------|--------|
| Netlib Test Set | 100% correct solutions |
| Infeasibility Detection | 100% accuracy |
| Unboundedness Detection | 100% accuracy |
| Solution Accuracy | Within 10⁻⁶ of optimal |

### 13.2 Performance Metrics

| Metric | Target |
|--------|--------|
| Netlib Solve Time | Within 10× of commercial solvers |
| Memory Usage | < 2× problem size |
| Iteration Count | Within 2× of commercial solvers |

### 13.3 Quality Metrics (TDD Requirements)

| Metric | Target | Enforcement |
|--------|--------|-------------|
| **Test:Code Ratio** | **1:3 to 1:4** | Block merge if below 1:4 |
| Line Coverage | ≥ 90% | Block merge |
| Branch Coverage | ≥ 85% | Block merge |
| Function Coverage | 100% | Block merge |
| Test Pass Rate | 100% | Block merge |
| Mutation Score | ≥ 80% | Warning |
| Documentation | 100% public API | Block merge |
| Static Analysis | Zero critical warnings | Block merge |

### 13.4 TDD Compliance Metrics

| Phase | Required Tests Before Code | Verification |
|-------|---------------------------|--------------|
| Foundation (Memory, Error) | ~200 tests | CI gate |
| Data Layer (Matrix, Threading) | ~300 tests | CI gate |
| Core Operations (Basis, Pricing) | ~350 tests | CI gate |
| Simplex Core | ~200 tests | CI gate |
| Public API | ~180 tests | CI gate |
| **Total** | **~1,000+ tests** | Pre-release audit |

---

## 14. References

### 14.1 Foundational Literature

1. **Dantzig, G.B.** (1963). *Linear Programming and Extensions*. Princeton University Press.

2. **Chvátal, V.** (1983). *Linear Programming*. W.H. Freeman.

3. **Maros, I.** (2003). *Computational Techniques of the Simplex Method*. Springer.

### 14.2 Algorithmic References

4. **Bartels, R.H. & Golub, G.H.** (1969). The Simplex Method of Linear Programming Using LU Decomposition. *Communications of the ACM*, 12(5), 266-268.

5. **Harris, P.M.J.** (1973). Pivot Selection Methods of the Devex LP Code. *Mathematical Programming*, 5(1), 1-28.

6. **Forrest, J.J. & Goldfarb, D.** (1992). Steepest-Edge Simplex Algorithms for Linear Programming. *Mathematical Programming*, 57(1), 341-374.

7. **Wolfe, P.** (1963). A Technique for Resolving Degeneracy in Linear Programming. *Journal of the SIAM*, 11(2), 205-211.

### 14.3 Implementation References

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
