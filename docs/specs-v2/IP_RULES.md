# IP Cleanliness Rules (THE LAW)

**Every spec writer MUST read this document before writing any specification.**

These rules are NON-NEGOTIABLE. Violation of any rule invalidates the specification.

---

## Rule 1: NO ORIGINAL CODE

- No C code copied from decompiled sources
- No Ghidra variable names (param_1, local_20, iVar3, DAT_*, FUN_*)
- No assembly instructions or register references
- No hex constants from the binary (0x231d8a78, 0x28bf7dc5, etc.)

## Rule 2: NO BINARY LAYOUT INFORMATION

- No byte offsets (model+0xD8, env+0x1f78, etc.)
- No structure sizes in bytes (unless derived from standard algorithm requirements)
- No memory addresses from the DLL
- No padding or alignment details specific to the binary

## Rule 3: ALGORITHMS MUST REFERENCE PUBLISHED WORK

- Every algorithm description must cite the published technique it implements
- Cite by author/year: "Harris two-pass ratio test (Harris, 1973)"
- Cite by textbook: "Product Form of Inverse (Dantzig, 1963)"
- If the implementation makes a CHOICE between standard approaches, document the choice
  as an architectural decision, not as "what the code does"

## Rule 4: DESCRIBE BEHAVIOR, NOT IMPLEMENTATION

- "This function validates that the environment is properly initialized" (GOOD)
- "This function checks env+0x04 != 0 and env+0x08 >= 0" (BAD)
- "Returns an error code if the model has pending modifications" (GOOD)
- "Returns 10017 if *(model+0x280) != 0" (BAD)

## Rule 5: CONSTANTS ARE ALGORITHMIC PARAMETERS

- Error codes: Describe by name and purpose, not by magic number
- Status codes: Describe by semantic meaning
- Tolerances: Describe by algorithmic role ("feasibility tolerance, typically 1e-6")
- Magic numbers: NEVER include literal values from the binary

## Rule 6: DATA STRUCTURES ARE SEMANTIC

- Describe fields by PURPOSE and TYPE, not by offset
- "The model contains a pointer to its sparse matrix representation" (GOOD)
- "The model stores a MatrixData* at offset 0xD8" (BAD)
- Field ordering in specs is LOGICAL, not physical

## Rule 7: FUNCTION NAMES ARE GENERIC

- Use descriptive names that could apply to any LP solver implementation
- "validate_environment" or "check_environment" instead of implementation-specific names
- Group by logical module, not by binary organization

## Rule 8: NO BINARY-IDENTIFIABLE PATTERNS

- No sequences of operations that uniquely fingerprint the original binary
- No specific error message strings from the original
- No specific logging format strings
- No call sequences that could be pattern-matched to the original

## Rule 9: WHAT IS ALLOWED

- Standard algorithm descriptions from published literature
- Architectural choices documented as design decisions
- Behavioral contracts (preconditions, postconditions, side effects)
- Pseudocode using standard mathematical notation
- Performance characteristics derived from algorithm analysis
- Tolerance values that are standard in the field (1e-6, 1e-9, etc.)
- Error handling patterns common to commercial LP solvers
- Standard data structure patterns (CSC matrices, eta vectors, etc.)

## Rule 10: THE CLEAN ROOM TEST

For every sentence in a specification, ask:
> "Could this sentence have been written by someone who has NEVER seen the original
> software, but who is an expert in LP solver design and has read the published
> documentation/API reference?"

If NO: the sentence must be rewritten or removed.
If YES: the sentence is clean.

---

## VERIFICATION CHECKLIST (run on every spec)

```
[ ] No hex addresses or offsets
[ ] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[ ] No binary-specific constants
[ ] No copied code fragments
[ ] All algorithms cite published sources
[ ] All data structures described semantically
[ ] Passes the Clean Room Test (Rule 10)
```
