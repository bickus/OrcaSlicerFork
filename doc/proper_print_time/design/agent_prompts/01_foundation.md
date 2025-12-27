# Deliverable 1: Foundation

## Task Overview

You are implementing the foundation infrastructure for Klipper-compatible print time estimation in OrcaSlicer. This is the first deliverable in a series of 8. Your work will establish the data structures and code organization needed by subsequent agents.

**Branch**: `claude/fix-print-time-estimation-tmz9u`

## Important Constraints

1. **NO COMPILATION/EXECUTION**: You cannot compile or run the application. All validation must be done through static code analysis and careful review.

2. **NO TEST WRITING**: Do not create test files. Use the validation criteria below as a self-check for your code.

3. **PRESERVE LEGACY BEHAVIOR**: The existing time estimation algorithm for non-Klipper printers (Marlin, RepRapFirmware, Smoothieware, etc.) MUST remain completely unchanged. Your changes are ADDITIVE only.

4. **DOCUMENTATION REQUIRED**: After completing your work, create a completion report (see Documentation section below).

## Background

Read these design documents before starting:
- `doc/proper_print_time/design/00_OVERVIEW.md` - High-level architecture
- `doc/proper_print_time/design/01_foundation.md` - Detailed foundation design
- `doc/proper_print_time/design/ASSUMPTIONS.md` - Design decisions and constraints

Key concepts:
- OrcaSlicer currently uses a Marlin-like jerk-based algorithm for ALL printers
- Klipper uses a different "junction deviation" algorithm
- For Klipper printers, the jerk parameters (machine_max_jerk_x/y) ARE the Square Corner Velocity (SCV)
- We add a new Klipper code path while keeping the Legacy code path unchanged

## Files to Modify

1. **`src/libslic3r/GCode/GCodeProcessor.hpp`**
   - Add `EstimatorMode` enum
   - Add `KlipperState` struct to `TimeMachine`
   - Extend `TimeBlock` with Klipper-specific fields

2. **`src/libslic3r/GCode/GCodeProcessor.cpp`**
   - Implement derived parameter calculations
   - Add feature-flag guards for Klipper vs Legacy paths
   - Wire up `EstimatorMode` selection in `apply_config()`

## Specific Tasks

### Task 1: Add EstimatorMode Enum

In `GCodeProcessor.hpp`, add near the top of the file (after includes, before classes):

```cpp
enum class EstimatorMode : uint8_t {
    Legacy = 0,   // Marlin-like jerk-based algorithm (unchanged)
    Klipper = 1   // Junction deviation algorithm
};
```

### Task 2: Add KlipperState Struct

In `GCodeProcessor.hpp`, add the `KlipperState` struct. This should be inside the `TimeMachine` struct or as a nested struct within it:

```cpp
struct KlipperState {
    float junction_deviation{0.0f};      // Derived from jerk²/max_accel
    float accel_to_decel{50.0f};         // Derived from cruise_ratio
    float instant_corner_velocity{1.0f}; // From machine_max_jerk_e

    // Indicates if Klipper state is properly initialized
    bool initialized{false};
};
```

Also add to `TimeMachine`:
```cpp
EstimatorMode estimator_mode{EstimatorMode::Legacy};
KlipperState klipper_state;
```

### Task 3: Extend TimeBlock with Klipper Fields

In `GCodeProcessor.hpp`, find the `TimeBlock` struct and add these fields:

```cpp
// Klipper-specific fields (only used when estimator_mode == Klipper)
struct KlipperFields {
    Vec3f rate_xyz{Vec3f::Zero()};  // Unit direction vector for XYZ
    float rate_e{0.0f};              // Extruder rate (signed)

    float max_cruise_v2{0.0f};       // Maximum cruise velocity squared
    float max_dv2{0.0f};             // Max delta-v squared (from acceleration)
    float smoothed_dv2{0.0f};        // Smoothed delta-v squared (accel_to_decel)

    float max_start_v2{0.0f};        // Max start velocity squared (from junction)
    float max_smoothed_v2{0.0f};     // Max smoothed start velocity squared

    float junction_deviation{0.0f};  // Junction deviation for this move

    bool is_kinematic{true};         // false for E-only moves
    bool has_xy_motion{true};        // false for Z-only or E-only moves

    // Resolved velocities (set by two-pass planner)
    float resolved_start_v{0.0f};
    float resolved_cruise_v{0.0f};
    float resolved_end_v{0.0f};
};
KlipperFields klipper;
```

### Task 4: Implement Derived Parameter Calculations

In `GCodeProcessor.cpp`, add these helper functions (can be in anonymous namespace or as static functions):

```cpp
namespace {
    // Compute junction_deviation from jerk (which IS SCV for Klipper) and max acceleration
    // Formula: junction_deviation = jerk² * 0.41421356 / max_acceleration
    // The 0.41421356 = sqrt(2) - 1, derived from Klipper's junction geometry
    float compute_junction_deviation(float jerk_as_scv, float max_accel) {
        if (max_accel <= 0.0f) return 0.0f;
        constexpr float SQRT2_MINUS_1 = 0.41421356f;
        return (jerk_as_scv * jerk_as_scv) * SQRT2_MINUS_1 / max_accel;
    }

    // Compute accel_to_decel from max_acceleration and cruise_ratio
    // In Klipper: accel_to_decel = max_accel * (1 - cruise_ratio)
    // OrcaSlicer doesn't have cruise_ratio directly, so we use a default
    float compute_accel_to_decel(float max_accel, float cruise_ratio = 0.5f) {
        return max_accel * (1.0f - cruise_ratio);
    }
}
```

### Task 5: Add Feature-Flag Guards

In `GCodeProcessor.cpp`, find the existing `calculate_time()` function (or equivalent time calculation entry point). Rename it to `calculate_time_legacy()` and create a new dispatcher:

```cpp
// Rename existing implementation (DO NOT MODIFY THE LOGIC)
void GCodeProcessor::TimeMachine::calculate_time_legacy(/* existing params */) {
    // ... existing implementation unchanged ...
}

// New Klipper implementation stub (to be implemented in later deliverables)
void GCodeProcessor::TimeMachine::calculate_time_klipper(/* params */) {
    // TODO: Implement in Deliverable 4
    // For now, fall back to legacy to maintain current behavior
    calculate_time_legacy(/* same params */);
}

// Dispatcher based on estimator mode
void GCodeProcessor::TimeMachine::calculate_time(/* params */) {
    switch (estimator_mode) {
        case EstimatorMode::Klipper:
            calculate_time_klipper(/* params */);
            break;
        case EstimatorMode::Legacy:
        default:
            calculate_time_legacy(/* params */);
            break;
    }
}
```

### Task 6: Wire Up EstimatorMode Selection

In `GCodeProcessor.cpp`, find `apply_config()` or the configuration initialization section. Add automatic mode selection based on gcode_flavor:

```cpp
void GCodeProcessor::apply_config(const PrintConfig& config) {
    // ... existing config application ...

    // Determine estimator mode from gcode flavor
    // This is AUTOMATIC - not a user setting
    EstimatorMode mode = (m_flavor == gcfKlipper)
        ? EstimatorMode::Klipper
        : EstimatorMode::Legacy;

    for (auto& machine : m_time_processor.machines) {
        machine.estimator_mode = mode;

        // Initialize Klipper state if needed
        if (mode == EstimatorMode::Klipper) {
            // Get jerk values - these ARE SCV for Klipper
            float scv = std::min(
                config.machine_max_jerk_x.values[0],
                config.machine_max_jerk_y.values[0]
            );
            float max_accel = config.machine_max_acceleration.values[0];
            float extruder_icv = config.machine_max_jerk_e.values[0];

            machine.klipper_state.junction_deviation =
                compute_junction_deviation(scv, max_accel);
            machine.klipper_state.accel_to_decel =
                compute_accel_to_decel(max_accel);
            machine.klipper_state.instant_corner_velocity = extruder_icv;
            machine.klipper_state.initialized = true;
        }
    }
}
```

## Self-Validation Checklist

After completing your implementation, verify each item by reviewing your code:

- [ ] **EstimatorMode enum exists** with Legacy=0 and Klipper=1 values
- [ ] **KlipperState struct exists** with junction_deviation, accel_to_decel, instant_corner_velocity fields
- [ ] **TimeBlock extended** with KlipperFields nested struct
- [ ] **compute_junction_deviation()** correctly implements: jerk² × 0.41421356 / max_accel
- [ ] **compute_accel_to_decel()** correctly implements: max_accel × (1 - cruise_ratio)
- [ ] **Legacy code path renamed** but logic NOT modified
- [ ] **Klipper stub falls back to legacy** (so current behavior unchanged)
- [ ] **Mode selection automatic** based on gcfKlipper flavor check
- [ ] **Non-Klipper printers always use Legacy mode** - verify the else/default branch
- [ ] **No hardcoded SCV values** - jerk parameters are used as SCV
- [ ] **All new fields have sensible defaults** (0.0f or appropriate values)
- [ ] **Code compiles conceptually** - syntax is correct, types match, includes present
- [ ] **No changes to actual time calculation logic** in Legacy path

## Documentation Requirements

After completing your work, create a completion report at:
`doc/proper_print_time/design/agent_prompts/completed/01_foundation_complete.md`

The report MUST include:

1. **Summary**: Brief description of what was implemented
2. **Files Modified**: List with specific line numbers/ranges where changes were made
3. **Design Decisions**: Any choices you made that weren't explicitly specified
4. **Deviations**: Any differences from the design documents (with justification)
5. **Known Issues**: Problems or concerns for subsequent agents
6. **Self-Validation Results**: Completed checklist from above with notes
7. **Code Snippets**: Key code sections that subsequent agents should be aware of

Example structure:
```markdown
# Deliverable 1: Foundation - Completion Report

## Summary
Implemented foundation infrastructure for Klipper time estimation...

## Files Modified
- `src/libslic3r/GCode/GCodeProcessor.hpp`
  - Lines 45-48: Added EstimatorMode enum
  - Lines 123-135: Added KlipperState struct
  - Lines 201-225: Extended TimeBlock with KlipperFields

- `src/libslic3r/GCode/GCodeProcessor.cpp`
  - Lines 89-102: Added compute_junction_deviation()
  - Lines 104-108: Added compute_accel_to_decel()
  - Lines 1450-1475: Renamed calculate_time to calculate_time_legacy
  - Lines 1477-1495: Added calculate_time_klipper stub
  - Lines 1497-1510: Added calculate_time dispatcher
  - Lines 2100-2130: Added mode selection in apply_config

## Design Decisions
1. Placed KlipperState as nested struct in TimeMachine because...
2. Used float instead of double for velocity² values because...

## Deviations
None - followed design documents exactly.

## Known Issues
1. The cruise_ratio is hardcoded to 0.5 - may need to derive from config
2. Need to verify machine_max_acceleration config key name

## Self-Validation Results
- [x] EstimatorMode enum exists - Line 46
- [x] KlipperState struct exists - Line 125
...

## Key Code for Next Agent
The mode selection happens in apply_config() at line 2100:
```cpp
EstimatorMode mode = (m_flavor == gcfKlipper) ? ...
```
```

## Commit Requirements

After completing your work:
1. Stage all modified files
2. Commit with message: "Deliverable 1: Add foundation infrastructure for Klipper time estimation"
3. Push to branch `claude/fix-print-time-estimation-tmz9u`

## What NOT to Do

- DO NOT modify the actual time calculation logic in the Legacy path
- DO NOT add new user-facing configuration parameters
- DO NOT create test files
- DO NOT try to compile or run the code
- DO NOT implement the actual Klipper algorithm yet (that's for later deliverables)
- DO NOT change anything related to non-Klipper printers except adding the mode check
