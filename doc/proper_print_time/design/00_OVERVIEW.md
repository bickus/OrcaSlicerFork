# Proper Print Time Estimation - Implementation Overview

## Executive Summary

This document series describes the implementation plan for adding a **Klipper-specific** time estimation algorithm to OrcaSlicer. The existing Marlin-compatible algorithm remains **completely unchanged** and continues to be used for all non-Klipper printers.

**Critical Design Principle**: This is an ADDITIVE change. We are adding a new code path for Klipper printers while preserving the existing code path for Marlin, RepRapFirmware, and all other firmware types.

## Problem Statement

### Current State
- OrcaSlicer uses a Marlin-like jerk-based algorithm for time estimation
- This algorithm works well for Marlin, RepRapFirmware, and similar firmware
- Print time estimates for **Klipper printers specifically** are often 15-30% too pessimistic
- The existing `klipper_actual_speed` feature adds visualization but explicitly avoids changing time estimation
- Key Klipper concepts (junction deviation, two-pass planning, proper lookahead) are not implemented

### Desired State
- Accurate print time estimation (±5%) for Klipper printers using a Klipper-specific algorithm
- **Marlin and other firmware types continue using the existing algorithm unchanged**
- Per-layer and per-feature time breakdowns match actual print behavior

## Firmware-Specific Approach

```
┌─────────────────────────────────────────────────────────────────┐
│                    G-code Flavor Detection                       │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              │                               │
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────────────┐
│   gcfKlipper            │     │   All Other Flavors              │
│   (Klipper firmware)    │     │   (Marlin, RRF, Smoothie, etc.) │
└─────────────────────────┘     └─────────────────────────────────┘
              │                               │
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────────────┐
│   EstimatorMode::Klipper│     │   EstimatorMode::Legacy          │
│   NEW algorithm         │     │   EXISTING algorithm (unchanged) │
│   - Junction deviation  │     │   - Jerk-based junctions         │
│   - Two-pass planning   │     │   - Current forward/reverse pass │
│   - SCV corner limiting │     │   - Current trapezoid calc       │
└─────────────────────────┘     └─────────────────────────────────┘
```

**The Legacy path is the EXISTING code. It is not modified in any way.**

## Architecture Overview

```
                    ┌────────────────────────────────────────────────────────────┐
                    │                    G-code Processing                        │
                    └────────────────────────────────────────────────────────────┘
                                              │
                                              ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                          PlanningMove Creation                                   │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────────────┐ │
│  │  Parse G0/G1    │───▶│ Create Move     │───▶│ Apply Move Checkers         │ │
│  │  commands       │    │ (distance,rate) │    │ (axis limits, extruder)     │ │
│  └─────────────────┘    └─────────────────┘    └─────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────────┘
                                              │
                                              ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                       Junction Velocity Calculation                              │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────────────┐ │
│  │ Angle between   │───▶│ Junction dev.   │───▶│ Extruder junction           │ │
│  │ moves           │    │ velocity        │    │ (instant_corner_velocity)   │ │
│  └─────────────────┘    └─────────────────┘    └─────────────────────────────┘ │
│                              │                              │                    │
│                              └──────────────┬───────────────┘                    │
│                                             ▼                                    │
│                              ┌─────────────────────────────┐                     │
│                              │ max_start_v2 = min(all)     │                     │
│                              └─────────────────────────────┘                     │
└─────────────────────────────────────────────────────────────────────────────────┘
                                              │
                                              ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                       Two-Pass Velocity Planning                                 │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────────────────────┐ │
│  │ Backward pass   │───▶│ Forward pass    │───▶│ Resolve delayed moves       │ │
│  │ (constraints)   │    │ (propagate)     │    │ (set final velocities)      │ │
│  └─────────────────┘    └─────────────────┘    └─────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────────┘
                                              │
                                              ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                       Time Calculation                                           │
│  ┌────────────────────────────────────────────────────────────────────────────┐ │
│  │  For each move: time = accel_time + cruise_time + decel_time               │ │
│  │  Accumulate by: layer, feature type, extruder                              │ │
│  └────────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## Algorithm Comparison (Klipper Mode vs Legacy Mode)

The following table compares the NEW Klipper algorithm with the EXISTING Legacy algorithm. **The Legacy algorithm remains unchanged and is used for all non-Klipper printers.**

| Aspect | Legacy Mode (Marlin/RRF/etc.) | Klipper Mode (Klipper only) |
|--------|-------------------------------|----------------------------|
| Junction velocity | Jerk-based (current code) | Junction deviation + centripetal |
| Corner speed | Uses `machine_max_jerk_*` | Uses `machine_max_jerk_*` as SCV (see note below) |
| Velocity planning | Basic forward/reverse pass | Full two-pass with smoothing |
| Lookahead | Current implementation | Velocity constraint propagation |
| Cruise ratio | Current behavior | Enforces minimum cruise distance |
| Axis limits | Current implementation | Full per-axis velocity/accel limits |
| Extruder limits | Current implementation | Proper instantaneous corner velocity |
| **Code path** | **EXISTING (unchanged)** | **NEW (additive)** |

---

## IMPORTANT: Jerk Parameters = Square Corner Velocity for Klipper

**OrcaSlicer does NOT have a separate "Square Corner Velocity" (SCV) parameter.**

For Klipper printers, the existing **jerk parameters** (`machine_max_jerk_x`, `machine_max_jerk_y`) serve as the Square Corner Velocity:

| OrcaSlicer Parameter | Klipper Meaning | Notes |
|---------------------|-----------------|-------|
| `machine_max_jerk_x` | Square Corner Velocity (X component) | Used as SCV for Klipper |
| `machine_max_jerk_y` | Square Corner Velocity (Y component) | Used as SCV for Klipper |
| `machine_max_jerk_e` | Instantaneous Corner Velocity | For extruder junction limiting |

**Throughout this design document, when "SCV" is mentioned, it refers to the value read from `machine_max_jerk_x/y` - NOT a separate parameter.**

The `SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=` G-code command can override this at runtime, but the default comes from the jerk settings.

---

## Implementation Phases

### Phase 1: Foundation (Deliverable 1)
- Introduce `EstimatorMode` to toggle between legacy and Klipper algorithms
- Add/expose configuration parameters needed for Klipper estimation
- Create `KlipperPlanner` class as a separate calculation path
- No changes to existing time estimation (feature-flagged)

### Phase 2: Junction Velocity (Deliverable 2)
- Implement junction deviation formula
- Implement centripetal velocity constraint
- Implement extruder junction speed limiting
- Calculate proper `max_start_v2` for each move

### Phase 3: Two-Pass Planning (Deliverable 3)
- Implement backward pass with constraint propagation
- Implement forward pass with delayed move resolution
- Implement velocity smoothing (`accel_to_decel`)
- Calculate final `start_v`, `cruise_v`, `end_v` for each move

### Phase 4: Move Checkers (Deliverable 4)
- Implement axis-specific velocity/acceleration limits
- Implement extruder-only move limits
- Apply checkers during move creation

### Phase 5: Integration (Deliverable 5)
- Wire new planner into GCodeProcessor
- Update per-layer and per-feature time accumulation
- Ensure preview kinematics use same values
- Validation testing against klipper_estimator

## Parameter Mapping

### Existing Parameters (Already in OrcaSlicer)

| OrcaSlicer Parameter | Klipper Equivalent | Notes |
|---------------------|-------------------|-------|
| `machine_max_jerk_x` | `square_corner_velocity` | **Jerk IS SCV for Klipper** |
| `machine_max_jerk_y` | `square_corner_velocity` | Use min(jerk_x, jerk_y) as SCV |
| `machine_max_jerk_e` | `instantaneous_corner_velocity` | For extruder junction limiting |
| `machine_max_speed_x/y/z/e` | `max_velocity` per axis | Printer profile |
| `machine_max_acceleration_x/y/z/e` | `max_acceleration` per axis | Printer profile |
| `machine_max_acceleration_extruding` | Feature-specific accel | Printer profile |
| `klipper_cruise_ratio` | `minimum_cruise_ratio` | Already exists (from klipper_actual_speed) |

### Values from G-code (Runtime Override)

| G-code Command | Parameter | Handling |
|----------------|-----------|----------|
| `SET_VELOCITY_LIMIT VELOCITY=` | Max velocity | Parsed, used |
| `SET_VELOCITY_LIMIT ACCEL=` | Max acceleration | Parsed, used |
| `SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=` | Overrides jerk-as-SCV | Parsed, will be used for time est |
| `SET_VELOCITY_LIMIT MINIMUM_CRUISE_RATIO=` | Cruise ratio | Parsed, will be used for time est |
| `M204 S`/`P`/`T` | Acceleration | Parsed, used |

### Derived Parameters (No New Config Needed)

| Derived Value | Source | Formula |
|--------------|--------|---------|
| SCV (Square Corner Velocity) | `min(machine_max_jerk_x, machine_max_jerk_y)` | Direct read from jerk |
| `junction_deviation` | SCV + max_accel | `jerk² × 0.414 / max_acceleration` |
| `instant_corner_velocity` | `machine_max_jerk_e` | Direct read from extruder jerk |
| `accel_to_decel` | cruise_ratio + max_accel | `max_acceleration × (1 - cruise_ratio)` |
| Per-feature SCV | Per-feature jerk values | Already in profile for each feature |

## Success Criteria

1. **Klipper Accuracy**: Print time estimates within ±5% of actual for Klipper printers
2. **Non-Klipper Unchanged**: Marlin, RepRapFirmware, and other non-Klipper printers produce **byte-identical** time estimates (same code path, zero changes)
3. **Performance**: Time estimation adds <100ms to G-code processing
4. **Compatibility**: All existing features (preview, layer times, etc.) continue to work

### Non-Regression Guarantee

The following firmware types will use the **existing, unmodified** Legacy algorithm:
- Marlin / Marlin 2
- RepRapFirmware (RRF)
- Smoothieware
- Repetier
- Mach3/Mach4
- Any firmware where `gcode_flavor != gcfKlipper`

**Only printers explicitly configured with Klipper G-code flavor will use the new algorithm.**

## Document Index

- [01_foundation.md](./01_foundation.md) - Phase 1: Foundation and data structures
- [02_junction_velocity.md](./02_junction_velocity.md) - Phase 2: Junction velocity calculation
- [03_two_pass_planner.md](./03_two_pass_planner.md) - Phase 3: Two-pass velocity planning
- [04_move_checkers.md](./04_move_checkers.md) - Phase 4: Axis and extruder limiters
- [05_integration.md](./05_integration.md) - Phase 5: Integration and testing
- [ASSUMPTIONS.md](./ASSUMPTIONS.md) - Documented assumptions and design decisions
