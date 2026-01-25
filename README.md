# Convexfeld

An open specification for implementing a linear programming solver using the revised simplex method.

## About

Convexfeld provides complete, implementation-ready specifications for building an LP solver from scratch. Developed as part of graduate research in computational optimization, these specifications document:

- Memory management patterns for sparse matrix operations
- Basis factorization using the Product Form of Inverse (PFI)
- Steepest-edge and Dantzig pricing strategies
- Harris two-pass ratio test for numerical stability
- Anti-cycling techniques (perturbation, lexicographic)

## Project Structure

```
convexfeld/
├── docs/
│   ├── specs/
│   │   ├── functions/      # 142 function specifications
│   │   ├── modules/        # 17 module specifications
│   │   ├── structures/     # 8 data structure specifications
│   │   └── architecture/   # System architecture documentation
│   └── inventory/          # Function catalog and dependencies
└── templates/              # Specification templates
```

## Specification Coverage

| Category | Count | Description |
|----------|-------|-------------|
| Functions | 142 | Complete behavioral specifications |
| Modules | 17 | Subsystem documentation |
| Structures | 8 | Core data structure layouts |
| Architecture | 3 | System design documents |

## Academic Foundation

These specifications synthesize algorithms from foundational optimization literature:

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Chvátal, V. (1983). *Linear Programming*. W.H. Freeman.
- Wolfe, P. (1963). "A technique for resolving degeneracy in linear programming." *SIAM Journal*.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*.
- Forrest, J.J. & Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*.
- Bartels, R.H. & Golub, G.H. (1969). "The simplex method of linear programming using LU decomposition." *Communications of the ACM*.

## API Naming Convention

All functions use the `cxf_` prefix (Convexfeld):

```c
// Environment management
CxfEnv* cxf_create_env(void);
void cxf_free_env(CxfEnv* env);

// Model operations
CxfModel* cxf_create_model(CxfEnv* env, const char* name);
int cxf_optimize(CxfModel* model);
int cxf_add_var(CxfModel* model, ...);
int cxf_add_constr(CxfModel* model, ...);
```

## Constants

```c
#define CXF_INFINITY     1e100
#define CXF_OPTIMAL      1
#define CXF_INFEASIBLE   2
#define CXF_UNBOUNDED    3
#define CXF_ERR_MEMORY   1001
#define CXF_ERR_NULL     1002
#define CXF_ERR_INVALID  1003
```

## License

MIT License - Free for educational and commercial use.

## Contributing

Contributions welcome. Please ensure all additions follow the established specification format and maintain consistency with the academic literature foundations.

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
