#!/usr/bin/env python3
"""Assemble SPECIFICATION.md from clean source specs."""

import os
from datetime import date

SPECS_DIR = os.path.join(os.path.dirname(__file__), "specs")
OUTPUT = os.path.join(os.path.dirname(__file__), "output", "SPECIFICATION.md")

# Ordered list: (section_id, toc_title, subdir, filename)
SECTIONS = [
    # Part I: Data Model
    ("P1.01", "Model", "data-model", "model.md"),
    ("P1.02", "MatrixData", "data-model", "matrix_data.md"),
    ("P1.03", "Environment", "data-model", "environment.md"),
    ("P1.04", "SolverState", "data-model", "solver_state.md"),
    ("P1.05", "BasisState", "data-model", "basis_state.md"),
    ("P1.06", "CallbackState", "data-model", "callback_state.md"),
    ("P1.07", "PricingState", "data-model", "pricing_state.md"),
    ("P1.08", "EtaVector", "data-model", "eta_vector.md"),
    ("P1.09", "WorkArrays (Solution Data Container)", "data-model", "work_arrays.md"),
    ("P1.10", "Supporting Structures", "data-model", "supporting_structures.md"),
    # Part II: Algorithms
    ("P2.1", "Revised Simplex Method", "algorithms", "revised_simplex.md"),
    ("P2.2", "Product Form of the Inverse (PFI) Basis Update", "algorithms", "product_form_inverse.md"),
    ("P2.3", "Multi-Level Partial Pricing", "algorithms", "partial_pricing.md"),
    ("P2.4", "Harris Ratio Test and Bound-Flipping Ratio Test (BFRT)", "algorithms", "harris_ratio_test.md"),
    ("P2.5", "Crash Basis Construction", "algorithms", "crash_basis.md"),
    ("P2.6", "Perturbation and Anti-Cycling", "algorithms", "perturbation.md"),
    ("P2.7", "Barrier-to-Simplex Crossover", "algorithms", "crossover.md"),
    ("P2.8", "Simplex-Integrated Bound Propagation", "algorithms", "bound_propagation.md"),
    # Part III: Module Contracts (P3.27 Solve MIP and P3.28 Multi-Objective excluded)
    ("P3.01", "Module: Memory Primitives", "modules", "memory_primitives.md"),
    ("P3.02", "Module: Allocation Helpers", "modules", "allocation_helpers.md"),
    ("P3.03", "Module: State Initialization", "modules", "state_initialization.md"),
    ("P3.04", "Module: State Cleanup - Buffers", "modules", "state_cleanup_buffers.md"),
    ("P3.05", "Module: State Cleanup - Solver", "modules", "state_cleanup_solver.md"),
    ("P3.06", "Module: Model Type Checking", "modules", "model_type_checking.md"),
    ("P3.07", "Module: Input Validation", "modules", "input_validation.md"),
    ("P3.08", "Module: Data Validation", "modules", "data_validation.md"),
    ("P3.09", "Module: Error Handling", "modules", "error_handling.md"),
    ("P3.10", "Module: Logging", "modules", "logging.md"),
    ("P3.11", "Module: Threading & Synchronization", "modules", "threading_sync.md"),
    ("P3.12", "Module: Thread Init & Thunks", "modules", "thread_init_thunks.md"),
    ("P3.13", "Module: Callbacks", "modules", "callbacks.md"),
    ("P3.14", "Module: Matrix Core", "modules", "matrix_core.md"),
    ("P3.15", "Module: Matrix Finalization", "modules", "matrix_finalization.md"),
    ("P3.16", "Module: Basis Operations", "modules", "basis_operations.md"),
    ("P3.17", "Module: Pricing Core", "modules", "pricing_core.md"),
    ("P3.18", "Module: Pricing Support", "modules", "pricing_support.md"),
    ("P3.19", "Module: Pivot Operations", "modules", "pivot_operations.md"),
    ("P3.20", "Module: Simplex Iteration", "modules", "simplex_iteration.md"),
    ("P3.21", "Module: Simplex Phases", "modules", "simplex_phases.md"),
    ("P3.22", "Module: Simplex Lifecycle", "modules", "simplex_lifecycle.md"),
    ("P3.23", "Module: Crossover", "modules", "crossover.md"),
    ("P3.24", "Module: Solve Entry & Dispatch", "modules", "solve_entry.md"),
    ("P3.25", "Module: Solve LP Core", "modules", "solve_lp_core.md"),
    ("P3.26", "Module: Solve Barrier & Concurrent", "modules", "solve_barrier_concurrent.md"),
    # P3.27 (Solve MIP) — EXCLUDED
    # P3.28 (Multi-Objective & Scenario) — EXCLUDED
    ("P3.29", "Module: Solution Processing", "modules", "solution_processing.md"),
    ("P3.30", "Module: Environment Lifecycle", "modules", "environment_lifecycle.md"),
    ("P3.31", "Module: Model Lifecycle", "modules", "model_lifecycle.md"),
    ("P3.32", "Module: Optimization Preparation", "modules", "optimization_preparation.md"),
    ("P3.33", "Module: Statistics & Diagnostics", "modules", "statistics_diagnostics.md"),
    ("P3.34", "Module: Cleanup Utilities", "modules", "cleanup_utilities.md"),
    ("P3.35", "Module: Query Utilities", "modules", "query_utilities.md"),
    # Part IV: Integration
    ("P4.1", "Optimization Pipeline", "integration", "optimization_pipeline.md"),
    ("P4.2", "Parameter System", "integration", "parameter_system.md"),
    ("P4.3", "Integration: Error Propagation", "integration", "error_propagation.md"),
    ("P4.4", "Callback Protocol", "integration", "callback_protocol.md"),
    ("P4.5", "Threading Model", "integration", "threading_model.md"),
    # Part V: Reference
    ("P5.1", "Error & Status Codes", "reference", "error_status_codes.md"),
    ("P5.2", "Parameters & Defaults", "reference", "parameters_defaults.md"),
    ("P5.3", "Tolerances & Constants", "reference", "tolerances_constants.md"),
    # Part VI: Implementation Guidance
    ("P7.R15", "Implementation Order Guide", "reference", "implementation_order.md"),
    ("P7.R16", "Test Oracle Guidance", "reference", "test_oracle_guidance.md"),
    ("P7.R17", "Numerical Stability Guide for LP Solver Implementation", "reference", "numerical_stability.md"),
]

# Part boundaries for TOC grouping
PARTS = [
    ("Part I: Data Model", "P1."),
    ("Part II: Algorithms", "P2."),
    ("Part III: Module Contracts", "P3."),
    ("Part IV: Integration Specifications", "P4."),
    ("Part V: Reference", "P5."),
    ("Part VI: Implementation Guidance", "P7."),
]


def make_anchor(sid, title):
    """Generate GitHub-style anchor from section ID and title."""
    raw = f"{sid} -- {title}".lower()
    anchor = raw.replace(" ", "-").replace(".", "").replace("(", "").replace(")", "")
    anchor = anchor.replace("&", "").replace(",", "").replace(":", "").replace("/", "")
    anchor = anchor.replace("--", "-").replace("--", "-")
    return anchor


def build_toc():
    lines = []
    for part_title, prefix in PARTS:
        lines.append(f"\n### {part_title}\n")
        for sid, title, _, _ in SECTIONS:
            if sid.startswith(prefix):
                anchor = make_anchor(sid, title)
                lines.append(f"- [{sid} -- {title}](#{anchor})")
    return "\n".join(lines)


def build_header(total_files, total_lines):
    return f"""# ConvexFeld LP Solver: Cleanroom Behavioral Specification

**Version:** 2.1 (Consolidated, LP-only)
**Date:** {date.today().isoformat()}
**Files assembled:** {total_files}
**Total lines:** {total_lines:,}

---

## About This Document

This document is the consolidated output of the ConvexFeld LP Solver cleanroom reverse-engineering project. It contains complete behavioral specifications organized into 5 specification layers plus implementation guidance.

**IP Compliance:** All content has been validated for intellectual property cleanliness. This document contains:
- No hex addresses, byte offsets, or binary layout details
- No Ghidra decompilation artifacts
- No copied code fragments
- Only behavioral descriptions derived from published algorithms and standard LP solver techniques
- All references cite published academic literature or public documentation

**Scope:** LP (Linear Programming) solver internals only. MIP (branch-and-bound), barrier/IPM (interior-point), and presolve are documented as out-of-scope with references to published literature.

**Specification Layers:**
1. **Data Model (Part I):** Core data structures and their semantic fields
2. **Algorithms (Part II):** Step-by-step algorithm descriptions
3. **Module Contracts (Part III):** Per-function behavioral contracts
4. **Integration (Part IV):** Cross-module interaction patterns
5. **Reference (Part V):** Error codes, parameters, tolerances
6. **Implementation Guidance (Part VI):** Build order, testing, numerical stability

---

## Table of Contents
"""


def main():
    # Read all source files
    file_contents = {}
    for sid, title, subdir, fname in SECTIONS:
        path = os.path.join(SPECS_DIR, subdir, fname)
        if not os.path.exists(path):
            print(f"WARNING: Missing file: {path}")
            continue
        with open(path, "r") as f:
            content = f.read().rstrip()
        # Strip the first H1 heading from the source file (we replace it with section ID)
        lines = content.split("\n")
        if lines and lines[0].startswith("# "):
            lines = lines[1:]  # remove first heading
            # Also remove blank line after heading if present
            while lines and lines[0].strip() == "":
                lines = lines[1:]
        file_contents[(sid, title)] = "\n".join(lines)

    # Build body
    body_parts = []
    for sid, title, subdir, fname in SECTIONS:
        key = (sid, title)
        if key not in file_contents:
            continue
        section_header = f"# {sid} -- {title}"
        body_parts.append(f"\n---\n\n{section_header}\n\n{file_contents[key]}")

    body = "\n".join(body_parts)
    total_files = len(file_contents)

    # Assemble
    toc = build_toc()
    # Count lines for header
    preliminary = build_header(total_files, 0) + toc + "\n" + body
    total_lines = len(preliminary.split("\n"))
    # Rebuild with correct line count
    full = build_header(total_files, total_lines) + toc + "\n" + body + "\n"

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    with open(OUTPUT, "w") as f:
        f.write(full)

    print(f"Assembled {total_files} files -> {OUTPUT}")
    print(f"Total lines: {total_lines:,}")


if __name__ == "__main__":
    main()
