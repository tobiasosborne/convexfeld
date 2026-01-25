# ConvexFeld Agent Instructions

This project uses **bd** (beads) for issue tracking. Run `bd onboard` to get started.

---

## Agent Startup Protocol

**Upon invocation, EVERY agent MUST:**

1. **Read HANDOFF.md** - Contains previous agent's work summary and next steps
2. **Read docs/learnings.md** - Contains project learnings and gotchas
3. **Read docs/IMPLEMENTATION_PLAN.md** - Contains the master plan and checklist
4. **Run `bd ready`** - Check for assigned/available work

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
- Record ALL learnings in `docs/learnings.md`
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
Add ALL learnings from this session to `docs/learnings.md`:
- What worked
- What didn't work
- Gotchas discovered
- Useful patterns found

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
- docs/learnings.md updated

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

- **Spec Location:** `docs/specs/` (142 functions, 8 structures, 17 modules)
- **Implementation Plan:** `docs/IMPLEMENTATION_PLAN.md`
- **Inventory:** `docs/inventory/all_functions.md`, `docs/inventory/module_assignment.md`

**Key Constraints:**
- TDD: Tests written BEFORE implementation
- Test:Code ratio: 1:3 to 1:4
- File size: 100-200 LOC max
- Language: Rust
