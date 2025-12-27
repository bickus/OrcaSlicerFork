# Agent Prompts for Klipper Print Time Estimation Implementation

This folder contains prompts for developer agents implementing each deliverable of the Klipper print time estimation feature.

## Usage

Each prompt file (`01_foundation.md`, `02_rate_vector.md`, etc.) contains a complete prompt to give to a developer agent. The prompts are designed to be self-contained but reference design documents and previous agent work.

## Important Constraints

1. **No Compilation/Execution**: Developer agents cannot compile or run the application. All validation is through static analysis and code review.

2. **No Test Writing**: Agents do not write test files. Validation criteria are used for self-checking code correctness.

3. **Documentation Required**: Each agent MUST document their work in `doc/proper_print_time/design/agent_prompts/completed/` for subsequent agents.

4. **Single Branch**: All work is done on branch `claude/fix-print-time-estimation-tmz9u`.

5. **Self-Validation**: Testing/validation points from the design documents should be used as checklists for code review.

## Execution Order

Execute prompts in order:
1. `01_foundation.md` - Foundation and infrastructure
2. `02_rate_vector.md` - Rate vector calculation
3. `03_junction_velocity.md` - Junction velocity algorithm
4. `04_two_pass_planning.md` - Backward/forward velocity planning
5. `05_move_checkers.md` - Axis and extruder limiters
6. `06_integration.md` - Integration and consistency
7. `07_validation.md` - Code review and static validation
8. `08_documentation.md` - Final documentation and polish

## Completed Work Documentation

After each deliverable, the agent creates a completion report in:
```
doc/proper_print_time/design/agent_prompts/completed/
```

Each report should include:
- Summary of changes made
- Files modified with line number references
- Design decisions made during implementation
- Any deviations from the design documents
- Known issues or concerns for subsequent agents
- Self-validation checklist results

## File Structure

```
agent_prompts/
├── README.md                    # This file
├── 01_foundation.md             # Deliverable 1 prompt
├── 02_rate_vector.md            # Deliverable 2 prompt
├── 03_junction_velocity.md      # Deliverable 3 prompt
├── 04_two_pass_planning.md      # Deliverable 4 prompt
├── 05_move_checkers.md          # Deliverable 5 prompt
├── 06_integration.md            # Deliverable 6 prompt
├── 07_validation.md             # Deliverable 7 prompt
├── 08_documentation.md          # Deliverable 8 prompt
└── completed/                   # Agent completion reports
    ├── 01_foundation_complete.md
    ├── 02_rate_vector_complete.md
    └── ...
```
