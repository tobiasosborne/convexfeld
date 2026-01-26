# ConvexFeld Agent Instructions

This project uses **bd** (beads) for issue tracking. Run `bd onboard` to get started.

---

## Agent Startup Protocol

**Upon invocation, EVERY agent MUST:**

1. **Read HANDOFF.md** - Contains previous agent's work summary and next steps
2. **Read docs/learnings/README.md** - Index of learnings (see gotchas.md, patterns.md)
3. **Read docs/plan/README.md** - Implementation plan overview (milestones in separate files)
4. **Run `bd ready`** - Check for assigned/available work

### Documentation Structure

**Learnings** (docs/learnings/):
- `README.md` - Index and quick reference
- `m0-m1_setup.md` - Setup and tracer bullet learnings
- `m2-m4_foundation.md` - Foundation/Infrastructure/Data layers
- `m5-m6_core.md` - Core Operations and Algorithm layers
- `patterns.md` - Reusable patterns and code examples
- `gotchas.md` - Failures and things to avoid

**Implementation Plan** (docs/plan/):
- `README.md` - Overview, architecture, constraints
- `m0_setup.md` through `m8_api.md` - Milestone-specific steps
- `structures.md` - Structure definitions
- `functions.md` - Function checklist
- `parallelization.md` - Parallelization guide

---

## Quick Reference

```bash
bd ready              # Find available work
bd show <id>          # View issue details
bd update <id> --status in_progress  # Claim work
bd close <id>         # Complete work
bd add "title"        # Create new issue
bd sync               # Sync with git
```

---

## Project Rules

### Rule 1: 200 LOC Limit

**Any file exceeding 200 lines of code triggers a refactor requirement.**

When you notice a file > 200 LOC:
```bash
bd add "Refactor <filename> to < 200 LOC" --label refactor
```

### Rule 2: Errors Are Not Failures

**ERRORS = SUCCESS if learnings are recorded.**

- Errors are expected during development
- An error becomes a success when you document what you learned
- Record learnings in appropriate file under `docs/learnings/`:
  - Milestone-specific: `m0-m1_setup.md`, `m2-m4_foundation.md`, `m5-m6_core.md`
  - Reusable patterns: `patterns.md`
  - Failures/gotchas: `gotchas.md`
- Never hide or ignore errors - they are valuable data

### Rule 3: Proactive Issue Creation

**Every agent is responsible for adding beads issues when something isn't right.**

Create issues for:
- Bugs discovered
- Technical debt noticed
- Documentation gaps
- Test coverage gaps
- Performance concerns
- Unclear specifications

### Rule 4: Decompose Complexity

**If a step gets too complicated, break it down.**

When stuck on a complex step:
```bash
bd add "Research: decompose <step> into smaller steps" --label research
bd add "Implement: <substep 1>" --label implementation
bd add "Implement: <substep 2>" --label implementation
```

---

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

### MANDATORY WORKFLOW:

#### 1. Record Learnings
Add learnings to the appropriate file in `docs/learnings/`:
- Milestone-specific (m0-m1, m2-m4, m5-m6): What worked, what didn't
- `patterns.md`: Reusable code patterns discovered
- `gotchas.md`: Failures and things to avoid

#### 2. Update HANDOFF.md
Write a handoff for the next agent containing:
- **Work completed** - What you accomplished
- **Current state** - Where things stand
- **Next steps** - What the next agent should do
- **Blockers/concerns** - Any issues to be aware of

#### 3. File Issues for Remaining Work
Create beads issues for anything that needs follow-up:
```bash
bd add "description of remaining work"
```

#### 4. Run Quality Gates (if code changed)
- Tests pass
- Linters pass
- Build succeeds

#### 5. Update Issue Status
- Close finished work: `bd close <id>`
- Update in-progress items: `bd update <id> --status blocked`

#### 6. PUSH TO REMOTE (MANDATORY)
```bash
git add -A
git commit -m "descriptive message"
git pull --rebase
bd sync
git push
git status  # MUST show "up to date with origin"
```

#### 7. Verify
- All changes committed AND pushed
- HANDOFF.md updated
- Learnings recorded in docs/learnings/

---

## CRITICAL RULES

- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
- ALWAYS update HANDOFF.md before ending session
- ALWAYS record learnings before ending session

---

## Project Context

**ConvexFeld** is an LP solver implementing the revised simplex method.

- **Specs:** `docs/specs/` (142 functions, 8 structures, 17 modules)
- **Plan:** `docs/plan/` (milestone files m0-m8, structures, functions, parallelization)
- **Learnings:** `docs/learnings/` (patterns, gotchas, milestone learnings)
- **Inventory:** `docs/inventory/all_functions.md`, `docs/inventory/module_assignment.md`

**Key Constraints:**
- TDD: Tests written BEFORE implementation
- Test:Code ratio: 1:3 to 1:4
- File size: 100-200 LOC max
- Language: C99
