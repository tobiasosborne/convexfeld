# Specification Templates

**Reference document for all spec writers. Each Layer has a specific template.**

---

## Layer 1: Data Structure Specification Template

```markdown
# [Structure Name]

## Purpose
One paragraph: what this structure represents and its role in the solver.

## Fields

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| ... | ... | ... | ... | ... |

Notes:
- Type uses SEMANTIC types: int, double, bool, pointer-to-X, array-of-X
- Purpose is a sentence describing the field's role
- Valid Values constrains the domain
- Invariants describe relationships with other fields

## Relationships
- What other structures this contains/references
- Ownership semantics (owns, borrows, shares)

## Lifecycle
- Creation: how/when this structure is initialized
- Mutation: what operations modify it and when
- Destruction: cleanup requirements, deallocation order

## Invariants
- Properties that must ALWAYS hold for a valid instance
- Relationships between fields
- Consistency conditions

## Thread Safety
- Which fields are thread-safe
- What synchronization is required
- Which operations need locking

## Design Rationale
- Why this structure exists as a separate entity
- Key design choices and their justification
- References to published patterns
```

---

## Layer 2: Algorithm Specification Template

```markdown
# [Algorithm Name]

## Published Reference
- Primary citation (author, year, paper/book title)
- The specific variant or combination of techniques used

## Purpose
What problem this algorithm solves and where it fits in the LP solver.

## Inputs
- What data the algorithm requires
- Preconditions on the input

## Outputs
- What the algorithm produces
- Postconditions guaranteed on output

## Algorithm Description

### Overview
High-level description (1-2 paragraphs)

### Detailed Steps
1. Step description (in mathematical/pseudocode notation)
2. ...

### Key Design Choices
- Choice A: [what was chosen] because [rationale]
- Choice B: ...

## Numerical Considerations
- Tolerances used and their roles
- Stability concerns
- Degeneracy handling

## Termination
- When the algorithm terminates
- Convergence guarantees
- Iteration limits

## Complexity
- Time complexity (best, average, worst case)
- Space complexity

## Edge Cases
- Empty inputs, trivial cases
- Degenerate cases
- Numerical boundary conditions
```

---

## Layer 3: Module Behavioral Contract Template

```markdown
# Module: [Module Name]

## Purpose
What this module does in the system.

## Functions

### [function_name]

**Purpose:** One sentence.

**Signature:**
- Input: [parameter] : [semantic type] - [description]
- Output: [return type] - [description]

**Preconditions:**
- What must be true before calling

**Postconditions:**
- What is guaranteed after successful return

**Side Effects:**
- What state changes occur

**Error Conditions:**
- [condition] -> [error response]

**Behavioral Description:**
1-3 sentences describing WHAT the function does (not HOW).
For complex functions, use numbered steps at a behavioral level.

**Thread Safety:** [safe | unsafe | conditional]

**Dependencies:** What other module functions this calls.

---
(repeat for each function in module)
```

---

## Layer 4: Integration Specification Template

```markdown
# [Integration Aspect]

## Overview
What this document describes and why it matters for reimplementation.

## Components Involved
Which modules participate.

## Flow Description
Step-by-step description of the integration flow.
Use sequence diagrams or state diagrams where helpful.

## State Transitions
What state changes occur at each step.

## Error Handling
How errors propagate between components.

## Configuration
What parameters affect this flow and how.

## Design Decisions
Key architectural choices and their rationale.
```

---

## Layer 5: Reference Document Template

```markdown
# [Reference Topic]

## Overview
What this reference covers.

## Table

| Name | Category | Description | Typical Value |
|------|----------|-------------|---------------|
| ... | ... | ... | ... |

Notes:
- "Typical Value" uses standard field values, NOT binary-specific constants
- Categories group related items
```

---

## Naming Conventions for Spec Files

- Layer 1: `cleanroom/v2/specs/data-model/{structure_name}.md`
- Layer 2: `cleanroom/v2/specs/algorithms/{algorithm_name}.md`
- Layer 3: `cleanroom/v2/specs/modules/{module_name}.md`
- Layer 4: `cleanroom/v2/specs/integration/{aspect_name}.md`
- Layer 5: `cleanroom/v2/specs/reference/{topic_name}.md`

All filenames: lowercase, underscores, .md extension.
