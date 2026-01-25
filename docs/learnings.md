# Project Learnings

This file captures learnings, gotchas, and useful patterns discovered during development.

**Rule: ERRORS = SUCCESS if learnings are recorded here.**

---

## 2025-01-25: Initial Setup

### CRITICAL ERROR: Wrong Language

**FAILURE: Implementation plan written for Rust instead of C99.**

- The PRD explicitly states C99
- Agent failed to read the PRD carefully
- Entire implementation plan has wrong file structure, syntax, tooling
- This is a major error requiring full rewrite of implementation plan

**Lesson: ALWAYS verify the target language from PRD before writing ANY implementation details.**

---

### Planning Phase Learnings

1. **Previous planning attempts failed** because they didn't:
   - Map every function to its spec file
   - Include explicit checklists for all 142 functions
   - Define a tracer bullet milestone to prove architecture
   - Specify parallelization strategy

2. **Tracer bullet pattern is critical**
   - Prove end-to-end works before investing in full implementation
   - A 1-variable LP test exercises: API → Simplex → Solution extraction

3. **File size discipline**
   - 200 LOC limit prevents complexity accumulation
   - Forces proper decomposition from the start

4. **Spec structure**
   - 142 functions across 17 modules in 6 layers
   - 8 core data structures
   - Implementation language: C99

---

## Gotchas

*(Add gotchas discovered during implementation here)*

---

## Useful Patterns

*(Add useful patterns discovered during implementation here)*

---

## Things That Didn't Work

*(Add failed approaches here so future agents don't repeat them)*
