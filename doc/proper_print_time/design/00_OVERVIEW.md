# Proper Print Time Estimation - Implementation Overview

## Executive Summary

This document series describes the implementation plan for replacing OrcaSlicer's current print time estimation logic with a Klipper-compatible algorithm that accurately predicts print duration for Klipper-based printers. The new implementation will also improve accuracy for other firmware types.

## Problem Statement

### Current State
- OrcaSlicer uses a Marlin-like jerk-based algorithm for time estimation
- Print time estimates for Klipper printers are often 15-30% too pessimistic
- The existing `klipper_actual_speed` feature adds visualization but explicitly avoids changing time estimation
- Key Klipper concepts (junction deviation, two-pass planning, proper lookahead) are not implemented

### Desired State
- Accurate print time estimation (±5%) for Klipper printers
- Maintained or improved accuracy for other firmware types
- Per-layer and per-feature time breakdowns match actual print behavior

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

## Key Differences from Current Implementation

| Aspect | Current OrcaSlicer | Klipper Algorithm |
|--------|-------------------|-------------------|
| Junction velocity | Jerk-based (Marlin style) | Junction deviation + centripetal |
| Corner speed | Uses `machine_max_jerk_*` | Uses `square_corner_velocity` |
| Velocity planning | Basic forward/reverse pass | Full two-pass with smoothing |
| Lookahead | Not implemented (flag only) | Velocity constraint propagation |
| Cruise ratio | Stored but not used in time calc | Enforces minimum cruise distance |
| Axis limits | Partially implemented | Full per-axis velocity/accel limits |
| Extruder limits | Basic | Proper instantaneous corner velocity |

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

| OrcaSlicer Parameter | Maps to Klipper | Location |
|---------------------|-----------------|----------|
| `machine_max_jerk_x/y/z/e` | `square_corner_velocity` (for XY) | Printer profile |
| `machine_max_speed_x/y/z/e` | `max_velocity` per axis | Printer profile |
| `machine_max_acceleration_x/y/z/e` | `max_acceleration` per axis | Printer profile |
| `machine_max_acceleration_extruding` | Feature-specific accel | Printer profile |
| `klipper_cruise_ratio` | `minimum_cruise_ratio` | Printer profile (added by klipper_actual_speed) |

### Values from G-code (Runtime)

| G-code Command | Parameter | Current Handling |
|----------------|-----------|------------------|
| `SET_VELOCITY_LIMIT VELOCITY=` | Max velocity | Parsed, used |
| `SET_VELOCITY_LIMIT ACCEL=` | Max acceleration | Parsed, used |
| `SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=` | SCV | Parsed, stored (viz only) |
| `SET_VELOCITY_LIMIT MINIMUM_CRUISE_RATIO=` | Cruise ratio | Parsed, stored (viz only) |
| `M204 S`/`P`/`T` | Acceleration | Parsed, used |

### Parameters NOT Needed (Derivable)

| Parameter | How to Derive |
|-----------|--------------|
| `junction_deviation` | Calculated: `scv² × 0.414 / max_acceleration` |
| `instant_corner_velocity` | Use extruder jerk value from profile |
| `accel_to_decel` | Use: `max_acceleration × (1 - cruise_ratio)` |
| Per-feature SCV | Use per-feature jerk values already in profile |

## Success Criteria

1. **Accuracy**: Print time estimates within ±5% of actual for Klipper printers
2. **Regression**: Non-Klipper printers maintain current accuracy (±10%)
3. **Performance**: Time estimation adds <100ms to G-code processing
4. **Compatibility**: All existing features (preview, layer times, etc.) continue to work

## Document Index

- [01_foundation.md](./01_foundation.md) - Phase 1: Foundation and data structures
- [02_junction_velocity.md](./02_junction_velocity.md) - Phase 2: Junction velocity calculation
- [03_two_pass_planner.md](./03_two_pass_planner.md) - Phase 3: Two-pass velocity planning
- [04_move_checkers.md](./04_move_checkers.md) - Phase 4: Axis and extruder limiters
- [05_integration.md](./05_integration.md) - Phase 5: Integration and testing
- [ASSUMPTIONS.md](./ASSUMPTIONS.md) - Documented assumptions and design decisions
