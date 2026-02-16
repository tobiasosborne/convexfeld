#!/usr/bin/env python3
"""Rename misnomer functions/structures across all v2 spec files."""

import os
import re

SPECS_DIR = os.path.join(os.path.dirname(__file__), "specs")

# (old_name, new_name) — order matters for safety (no substring collisions)
RENAMES = [
    # Functions
    ("cxf_simplex_iterate", "cxf_log_iteration_progress"),
    ("cxf_simplex_cleanup", "cxf_simplex_postsolve"),
    ("cxf_basis_refactor", "cxf_fix_variables_at_bounds"),
    ("cxf_basis_snapshot", "cxf_progress_snapshot"),
    ("cxf_sort_indices", "cxf_sort_by_values"),
    ("cxf_check_nan_or_inf", "cxf_is_finite"),
    ("cxf_cleanup_helper", "cxf_propagate_bounds"),
    ("cxf_setup_basis", "cxf_free_warmstart_basis"),
    ("cxf_setup_work_arrays", "cxf_free_work_arrays"),
    ("cxf_free_solver_state", "cxf_free_attribute_table"),
    ("cxf_errorlog", "cxf_set_error_string"),
    ("cxf_pre_optimize_callback", "cxf_pre_optimize_hook"),
    ("cxf_post_optimize_callback", "cxf_post_optimize_hook"),
    ("cxf_acquire_solve_lock", "cxf_save_locale_state"),
    ("cxf_set_thread_count", "cxf_validate_thread_count"),
    # Structures
    ("WorkArrays", "SolutionData"),
]

def rename_in_file(filepath, renames):
    """Apply all renames to a single file. Returns (changed, count) tuple."""
    with open(filepath, "r") as f:
        content = f.read()

    original = content
    total = 0
    for old, new in renames:
        # Use word-boundary-aware replacement to avoid partial matches
        # But be careful: these are code identifiers, so we match them as-is
        count = content.count(old)
        if count > 0:
            content = content.replace(old, new)
            total += count

    if content != original:
        with open(filepath, "w") as f:
            f.write(content)
        return True, total
    return False, 0

def main():
    # Verify no old name is a substring of another old name
    old_names = [r[0] for r in RENAMES]
    for i, a in enumerate(old_names):
        for j, b in enumerate(old_names):
            if i != j and a in b:
                print(f"WARNING: '{a}' is substring of '{b}' — order matters!")

    # Find all .md files under specs/
    md_files = []
    for root, dirs, files in os.walk(SPECS_DIR):
        for f in files:
            if f.endswith(".md"):
                md_files.append(os.path.join(root, f))

    # Also process top-level files
    for f in ["FUNCTION_MAP.md", "PLAN.md"]:
        path = os.path.join(os.path.dirname(SPECS_DIR), f)
        if os.path.exists(path):
            md_files.append(path)

    md_files.sort()

    print(f"Processing {len(md_files)} files with {len(RENAMES)} renames...\n")

    changed_files = []
    total_replacements = 0

    for filepath in md_files:
        changed, count = rename_in_file(filepath, RENAMES)
        if changed:
            relpath = os.path.relpath(filepath, os.path.dirname(SPECS_DIR))
            changed_files.append((relpath, count))
            total_replacements += count

    print(f"Changed {len(changed_files)} files, {total_replacements} total replacements:\n")
    for path, count in changed_files:
        print(f"  {count:3d}  {path}")

if __name__ == "__main__":
    main()
