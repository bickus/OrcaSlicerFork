# Final Deliverables: Klipper Time Estimation Fix

**Document Version:** 1.0
**Date:** 2026-01-05
**Author:** Development Lead (Persona A)
**Status:** APPROVED FOR IMPLEMENTATION

---

## Executive Summary

### Problem Statement

OrcaSlicer's print time estimation consistently **overestimates** print duration by **15-25%** for high-speed Klipper printers. Users with tuned Voron and similar high-speed configurations report:
- Complex geometry prints (gyroid infill, lattice structures): 20-28% overestimation
- Standard prints: 15-18% overestimation
- Vase mode (minimal junctions): 12-15% overestimation

This inaccuracy prevents users from reliably scheduling prints and undermines confidence in the slicer.

### Root Cause Summary

**PRIMARY CAUSE (90% of error):** Junction velocity calculation is overly conservative due to incorrect parameter conversion. OrcaSlicer treats Klipper's `SQUARE_CORNER_VELOCITY` as a jerk value instead of converting it to junction deviation using the correct formula:

```
junction_deviation = (square_corner_velocity)^2 / (2 * max_acceleration)
```

OrcaSlicer's existing geometric calculations are sound, but the parameter mapping causes the printer model to slow down excessively at direction changes, accumulating significant time overestimation.

**SECONDARY CAUSE (5-10% of error):** Missing smoothed velocity tracking. The `ACCEL_TO_DECEL` parameter is parsed from G-code but not used in time estimation. This causes **underestimation** (opposite to junction errors), but junction errors dominate the net effect.

**TERTIARY CAUSE (~2% of error):** No extruder junction velocity constraint (`instant_corner_velocity`). This affects retraction boundaries only.

### Solution Overview

Implement a Klipper-compatible motion planner model in three phases:

1. **Phase 1 (3-4 days):** Fix junction velocity calculation with proper SCV-to-junction-deviation conversion and 5-constraint minimum
2. **Phase 2 (4-5 days):** Implement smoothed velocity tracking with `accel_to_decel` constraint
3. **Phase 3 (2-3 days):** Add extruder junction constraint and polish

Key design decisions:
- Use existing `gcode_flavor == gcfKlipper` for detection (no G-code command parsing)
- Add new settings (`klipper_minimum_cruise_ratio`, `klipper_instant_corner_velocity`) to Motion Ability tab
- Maintain **ZERO impact** on non-Klipper firmware estimation
- Validate against klipper_estimator CLI reference implementation

### Expected Outcome

| Phase | Max Error | Average Error | Target |
|-------|-----------|---------------|--------|
| Current | 28% | 20% | - |
| Phase 1 | 15% | 10% | Junction fix |
| Phase 2 | 8% | 5% | Smoothed velocity |
| Phase 3 | 5% | 3% | Polish |

---

## Approval Status

### Final Approval Table

| Document | Reviewer | Status | Notes |
|----------|----------|--------|-------|
| Issue Analysis v3.0 | Expert (B) | **APPROVED** | All technical corrections applied |
| Issue Analysis v3.0 | Enthusiast (C) | **APPROVED** | Matches real-world experience |
| Design Document v2.1 | Expert (B) | **APPROVED** | Ready for implementation |
| Design Document v2.1 | Enthusiast (C) | **APPROVED** | Ultra-fast test cases included |
| Development Plan v1.2 | Developer (D) | **APPROVED** | All Q&A answered |

### Workflow Status

| Flag | Value |
|------|-------|
| Phase 3 Verdict | APPROVED |
| Phase 5 Verdict | COMPLETE |
| Developer Ready | Yes |
| Design Changes Require Review | No |

### Sign-Off Summary

**Expert (B) - Klipper Technical Expert:**
> "All technical descriptions now match klipper_estimator reference implementation. I am highly confident this implementation will succeed."

**Enthusiast (C) - Ultra-Fast Klipper User:**
> "Design is ready for development. All four critical requirements from Round 2 were FULLY ADDRESSED. I'm ready to beta test Phase 1!"

**Developer (D):**
> "All critical questions answered with line numbers and code examples. Ready to begin implementation."

---

## Design Document (Final)

### 1. New Settings Specification

#### Setting: `klipper_minimum_cruise_ratio`

| Property | Value |
|----------|-------|
| **Name** | `klipper_minimum_cruise_ratio` |
| **Type** | `ConfigOptionFloat` |
| **Default** | 0.5 |
| **Range** | 0.0 - 1.0 |
| **Category** | Machine Limits (Motion Ability tab) |
| **Visibility** | Expert |
| **Tooltip** | "Klipper minimum cruise ratio. Set to 0.0 for legacy behavior. Modern Klipper defaults to 0.5. This affects how time estimation models acceleration-to-deceleration transitions." |

#### Setting: `klipper_instant_corner_velocity`

| Property | Value |
|----------|-------|
| **Name** | `klipper_instant_corner_velocity` |
| **Type** | `ConfigOptionFloat` |
| **Default** | 1.0 |
| **Range** | 0.1 - 10.0 |
| **Unit** | mm/s |
| **Category** | Machine Limits (Motion Ability tab) |
| **Visibility** | Expert |
| **Tooltip** | "Klipper instant corner velocity for extruder direction changes. Limits junction speed at retractions. Typical value: 1.0 mm/s." |

#### Relationship to Existing Settings

Existing settings `accel_to_decel_enable` and `accel_to_decel_factor` are for **G-code GENERATION**, not time estimation. New settings have different semantic models and purposes:

```
klipper_minimum_cruise_ratio = 1.0 - (accel_to_decel_factor / 100.0)

Examples:
- accel_to_decel_factor = 50%  -> cruise_ratio = 0.5
- accel_to_decel_factor = 75%  -> cruise_ratio = 0.25
- accel_to_decel_factor = 100% -> cruise_ratio = 0.0
```

### 2. Klipper Detection

**Simplified Approach:** Use existing `gcode_flavor` printer profile setting.

```cpp
bool GCodeProcessor::is_klipper_flavor() const {
    return m_flavor == gcfKlipper;
}
```

**Advantages:**
- Deterministic - known at slice time
- User-controlled via explicit profile setting
- No parsing edge cases
- Already configured in printer profiles

### 3. Backward Compatibility

**Guarantee:** All changes have ZERO impact on Marlin, RepRapFirmware, and other non-Klipper firmware.

```cpp
if (m_flavor != gcfKlipper) {
    // Use existing jerk-based junction calculation
    return calculate_junction_legacy(prev, curr);
}
// Use new multi-constraint junction calculation
return calculate_junction_klipper(prev, curr);
```

### 4. Junction Velocity Algorithm

Klipper calculates junction velocity as the **minimum** of **5 primary constraints**:

```
max_junction_v^2 = min(
    extruder_junction_v^2,                                 // Constraint 1: Extruder reversal
    r * current_junction_deviation * current_accel,        // Constraint 2: Junction dev (current)
    r * previous_junction_deviation * previous_accel,      // Constraint 3: Junction dev (previous)
    0.5 * current_distance * tan_theta_d2 * current_accel, // Constraint 4: Centripetal (current)
    0.5 * previous_distance * tan_theta_d2 * previous_accel // Constraint 5: Centripetal (previous)
)
```

**Geometric factors:**
```
junction_cos_theta = -current_direction . previous_direction
sin_theta_d2 = sqrt(0.5 * (1.0 - cos_theta))
r = sin_theta_d2 / (1.0 - sin_theta_d2)
tan_theta_d2 = sin_theta_d2 / sqrt(0.5 * (1.0 + cos_theta))
```

**Edge Case Handling:**

| Angle | Condition | Handling |
|-------|-----------|----------|
| 180-degree (reversal) | `junction_cos_theta >= 1.0 - epsilon` | `r = 0`, forces full stop |
| 0-degree (straight) | `sin_theta_d2 >= 1.0 - epsilon` | Return `max_cruise_v2` (no constraint) |

### 5. Smoothed Velocity Algorithm

**Core Constraint:**
```
smoothed_v2 = min(max_cruise_v2, previous.smoothed_v2 + smoothed_dv2)

where:
  smoothed_dv2 = 2 * distance * accel_to_decel
  accel_to_decel = max_accel * (1.0 - minimum_cruise_ratio)
```

**Implementation Strategy:**
1. Start with basic `smoothed_v2` constraint
2. Measure accuracy improvement
3. Only implement full delayed moves algorithm if needed

### 6. UI/UX Design

**Location:** Printer Settings > Motion Ability tab (existing)

**Visibility:** Expert mode only, visible when `gcode_flavor == gcfKlipper`

**Warnings:**
- When cruise_ratio is assumed (not detected): Show tooltip warning
- When values are clamped: Show GUI warning near time estimate

### 7. Expert Validation Notes

Per Expert (B) review:
- Multi-constraint junction formula verified against klipper_estimator planner.rs:399-430
- Smoothed velocity algorithm verified against planner.rs:638-711
- Edge case handling for 0/180 degrees properly documented
- Phase estimates revised to realistic 3-4 days (Phase 1) and 4-5 days (Phase 2)

---

## Development Plan (Final)

### Phase 1: Junction Velocity Fix (3-4 days)

#### Task 1.1: Add Klipper-Specific Member Variables

**File:** `GCodeProcessor.hpp`

```cpp
// Klipper-specific time estimation parameters
float m_junction_deviation = 0.04f;    // Default Klipper value in mm
float m_accel_to_decel = 0.0f;         // Calculated from cruise_ratio or G-code
float m_instant_corner_velocity = 1.0f; // Klipper default mm/s
float m_cruise_ratio = 0.5f;           // Modern Klipper default
bool m_accel_to_decel_from_gcode = false;
float m_prev_e_feedrate = 0.0f;        // For extruder reversal detection
```

#### Task 1.2: Add Helper Function for Klipper Check

**File:** `GCodeProcessor.cpp`

```cpp
bool GCodeProcessor::is_klipper_flavor() const {
    return m_flavor == gcfKlipper;
}
```

#### Task 1.3: Fix SQUARE_CORNER_VELOCITY Parsing

**Location:** `process_SET_VELOCITY_LIMIT()` (lines 4016-4062)

**Key change:** Convert SCV to junction_deviation:
```cpp
// Correct formula: junction_deviation = scv^2 / (2 * accel)
m_junction_deviation = (scv * scv) / (2.0f * max_accel);
```

#### Task 1.4: Add ACCEL_TO_DECEL Parsing

**Location:** After VELOCITY parsing in `process_SET_VELOCITY_LIMIT()`

```cpp
std::regex a2d_pattern("\\sACCEL_TO_DECEL\\s*=\\s*([0-9]*\\.?[0-9]+)");
if (std::regex_search(line.raw(), matches, a2d_pattern) && matches.size() == 2) {
    m_accel_to_decel = std::stof(matches[1]);
    m_accel_to_decel_from_gcode = true;
}
```

#### Task 1.5: Implement 5-Constraint Junction Velocity

**Location:** Lines 3049-3127 in `process_G1()`

**Key implementation points:**
- Direction from `TimeMachine::State` (`prev.exit_direction`, `curr.enter_direction`)
- Extruder reversal via `m_prev_e_feedrate` sign comparison
- Edge case handling for 0/180 degrees
- Wrap existing jerk code in `else` block for non-Klipper

#### Task 1.6: Add New Config Settings

**Files:** `PrintConfig.cpp`, `PrintConfig.hpp`, `GCodeProcessor.cpp`

- Define settings with proper bounds, defaults, visibility
- Read values in `apply_config()`

#### Task 1.7: Unit Tests

**File:** `tests/libslic3r/test_klipper_estimation.cpp`

**Test cases:**
- Junction R calculation (normal angles, straight-through)
- Junction deviation from SCV formula
- Zero acceleration guard
- Klipper flavor detection

### Phase 2: Smoothed Velocity Implementation (4-5 days)

#### Task 2.1: Add Smoothed Velocity Fields

**File:** `GCodeProcessor.hpp`, `TimeBlock` struct

```cpp
float smoothed_v2 = 0.0f;      // Smoothed velocity squared
float smoothed_dv2 = 0.0f;     // Max velocity change squared
```

#### Task 2.2: Implement Smoothed Velocity Calculation

**Location:** After reverse pass, before `recalculate_trapezoids()` (line 444)

**Algorithm:**
1. Calculate `smoothed_dv2` for each block
2. Backward pass propagating smoothed velocity constraint
3. Apply constraints to exit velocities
4. Re-run trapezoid calculation

#### Task 2.3: Calculate accel_to_decel from cruise_ratio

**Priority:**
1. G-code `SET_VELOCITY_LIMIT ACCEL_TO_DECEL=X`
2. Config `klipper_minimum_cruise_ratio`
3. Default cruise_ratio = 0.5

### Phase 3: Polish and Edge Cases (2-3 days)

- Verify extruder junction constraint (Task 1.5 Constraint 1)
- Add info logging for assumed values
- Integration tests across all test cases
- Performance profiling and optimization if needed

---

## Validation Plan

### Test Infrastructure

**Required Tools:**
- `klipper_estimator` CLI for reference estimates
- Test G-code files with known actual print times
- Comparison scripts for per-move velocity validation

**Reference Data Generation:**
```bash
klipper_estimator --config_moonraker_url http://printer:7125 \
                  --dump-moves moves.json \
                  test.gcode
```

### Test Cases

| Test Case | Parameters | Phase 1 Target | Phase 2 Target | Phase 3 Target |
|-----------|------------|----------------|----------------|----------------|
| Grid Infill | 300mm/s, 10k accel, SCV 5 | < 15% max | < 8% max | < 5% max |
| Gyroid Infill | 300mm/s, 10k accel, SCV 5 | < 15% max | < 8% max | < 5% max |
| Vase Mode | 300mm/s, 10k accel | < 12% max | < 6% max | < 3% max |
| **Ultra-Fast Voron** | **1200mm/s, 100k accel, SCV 20, cruise_ratio 0.25** | < 15% max | < 8% max | < 5% max |
| Cruise Ratio Sweep | 0.0, 0.25, 0.5, 0.9 | Correct trend | Correct trend | < 5% each |
| Junction Torture | 1000mm/s, 100k accel, 1mm grid | < 15% max | < 10% max | < 8% max |
| Marlin (Regression) | Standard Marlin | 0% change | 0% change | 0% change |
| RepRap (Regression) | Standard RepRap | 0% change | 0% change | 0% change |

### Acceptance Criteria

**Phase 1 (Junction Fix):**
- Maximum error: No test case exceeds 15%
- Average error: < 10% across all test cases
- Per-move junction velocities within 15% of klipper_estimator
- Non-Klipper estimates unchanged

**Phase 2 (Smoothed Velocity):**
- Maximum error: No test case exceeds 8%
- Average error: < 5% across all test cases
- Per-move junction velocities within 10% of klipper_estimator
- Correct trend when cruise_ratio changes

**Phase 3 (Polish):**
- Maximum error: No test case exceeds 5%
- Average error: < 3% across all test cases
- Per-move junction velocities within 5% of klipper_estimator
- Performance within targets

### Per-Move Validation

**Methodology:**
1. Extract per-move velocities from klipper_estimator (`--dump-moves`)
2. Add debug output mode to OrcaSlicer for same data
3. Compare per-move junction velocities, not just total time
4. Flag moves with > 10% junction velocity error

**Why Required:** Two estimators can produce same total time with completely different velocity profiles. Per-move validation ensures algorithm correctness.

### Performance Targets

| Print Size | Moves | Target Time | Hard Limit |
|------------|-------|-------------|------------|
| Small (Benchy) | 50k | < 250ms | 500ms |
| Medium (typical) | 200k | < 1000ms | 2000ms |
| Large (complex) | 1M | < 5s | 8s |
| Very Large | 2.5M | < 12.5s | 20s |

**Absolute Limits:**
- Typical prints (< 500k moves): < 3 seconds
- Large prints (< 2M moves): < 15 seconds
- Any print: < 30 seconds maximum

---

## Appendix

### A. Research Notes

**Key Codebase Discoveries:**

1. **Direction Fields Location:**
   - Direction stored in `TimeMachine::State`, NOT `TimeBlock`
   - Access via `prev.exit_direction` and `curr.enter_direction`
   - Location: `GCodeProcessor.hpp` lines 414-416

2. **Extruder Position Access:**
   - `delta_pos[E]` where E=3, but `delta_pos` is local to `process_G1()`
   - Track via `m_prev_e_feedrate` member variable
   - Compare `axis_feedrate[E]` signs for reversal detection

3. **Junction Calculation Integration:**
   - No separate function - inline code at lines 3049-3127
   - Must wrap existing code in `else` block for non-Klipper

4. **Config Access Pattern:**
   - No `m_config` member - use `apply_config()` method
   - Store values in member variables

5. **Test Infrastructure:**
   - Catch2 v2 framework
   - Create tests in `/tests/libslic3r/test_klipper_estimation.cpp`

### B. Reference Links

**Klipper Estimator Repository:**
- https://github.com/Annex-Engineering/klipper_estimator

**Key Source Files:**
- `lib/src/planner.rs` - Main motion planning algorithms
  - Lines 399-430: Junction calculation (5 constraints)
  - Lines 638-711: Smoothed velocity backward pass
  - Lines 656-700: Delayed moves algorithm

**OrcaSlicer Source Files:**
- `src/libslic3r/GCode/GCodeProcessor.cpp` - Time estimation engine
- `src/libslic3r/GCode/GCodeProcessor.hpp` - Data structures
- `src/libslic3r/PrintConfig.cpp` - Configuration definitions
- `src/libslic3r/PrintConfig.hpp` - Configuration declarations

### C. Klipper Estimator Code References

**Junction Velocity (planner.rs:399-430):**
```rust
pub fn junction_speed_v2(&self, current: &PlanningMove, previous: &PlanningMove) -> f64 {
    // 5 constraints: extruder, junction_dev (curr/prev), centripetal (curr/prev)
    let min_v2 = [
        extruder_junction_v2,
        r * current.junction_deviation * current.accel,
        r * previous.junction_deviation * previous.accel,
        0.5 * current.distance * tan_theta_d2 * current.accel,
        0.5 * previous.distance * tan_theta_d2 * previous.accel,
    ].iter().copied().fold(f64::INFINITY, f64::min);
    min_v2
}
```

**Smoothed Velocity (planner.rs:638-711):**
```rust
// Backward pass with smoothed velocity
for i in (0..moves.len()).rev() {
    let smoothed_dv2 = 2.0 * distance * accel_to_decel;
    let max_smoothed_v2 = next.smoothed_v2 + smoothed_dv2;
    curr.smoothed_v2 = f64::min(max_cruise_v2, max_smoothed_v2);
}
```

### D. Revision History

| Version | Date | Changes |
|---------|------|---------|
| Issue Analysis v1.0 | 2026-01-05 | Initial analysis |
| Issue Analysis v2.0 | 2026-01-05 | Fixed junction formula, added validation plan |
| Issue Analysis v3.0 | 2026-01-05 | Fixed constraint count, edge cases, smoothed velocity characterization |
| Design Document v1.0 | 2026-01-05 | Initial design |
| Design Document v2.0 | 2026-01-05 | Added all Expert and Enthusiast corrections |
| Design Document v2.1 | 2026-01-05 | Simplified to use gcode_flavor, Motion Ability tab |
| Development Plan v1.0 | 2026-01-05 | Initial plan |
| Development Plan v1.1 | 2026-01-05 | Simplified Klipper detection |
| Development Plan v1.2 | 2026-01-05 | Added Q&A answers with code examples |

---

## Implementation Readiness Checklist

- [x] Issue analysis complete and approved
- [x] Design document complete and approved
- [x] Development plan with Q&A complete and approved
- [x] Test cases defined including ultra-fast configurations
- [x] Acceptance criteria clearly defined (max and average)
- [x] Performance targets specified (absolute time limits)
- [x] Backward compatibility strategy documented
- [x] All reviewer sign-offs obtained

**STATUS: READY FOR IMPLEMENTATION**

---

**End of Final Deliverables**
