# Phase 4: Move Checkers (Axis/Extruder Limiters)

## Overview

This phase implements move checkers that apply additional velocity constraints based on axis-specific limits and extruder-specific limits. These checkers modify the move's maximum velocity and acceleration before junction calculations.

## Goals

1. Implement axis limiter for X, Y, Z specific constraints
2. Implement extruder limiter for extrude-only moves
3. Apply limiters during move creation
4. Ensure limits are applied before junction calculations

---

## Algorithm Summary

Move checkers modify velocity constraints based on how much a move travels along specific axes:

```
For a move with direction (dx, dy, dz):
  - Calculate ratio = move_distance / |axis_component|
  - Limited velocity = axis_max_velocity × ratio
  - Limited acceleration = axis_max_acceleration × ratio
```

The ratio scaling ensures that the actual velocity along each axis doesn't exceed its limit.

---

## Work Items

### 4.1 Axis Limiter Implementation

Each axis can have independent velocity and acceleration limits:

```cpp
struct AxisLimiter {
    Vec3f axis;            // Unit vector for this axis (1,0,0), (0,1,0), or (0,0,1)
    float max_velocity;    // mm/s
    float max_accel;       // mm/s²

    void apply(TimeBlock& block) const {
        if (block.distance < 0.0001f) return;

        // Calculate displacement along this axis
        float axis_displacement = std::abs(
            block.rate_xyz.x() * axis.x() +
            block.rate_xyz.y() * axis.y() +
            block.rate_xyz.z() * axis.z()
        ) * block.distance;

        if (axis_displacement < 0.0001f) return;  // Move doesn't use this axis

        // Calculate ratio: how much faster can we go in total vs along this axis
        float ratio = block.distance / axis_displacement;

        // Calculate limited velocity and acceleration
        float limited_velocity = max_velocity * ratio;
        float limited_accel = max_accel * ratio;

        // Apply limits (take minimum with existing)
        float limited_v2 = limited_velocity * limited_velocity;
        if (limited_v2 < block.max_cruise_v2) {
            block.max_cruise_v2 = limited_v2;
            block.feedrate_profile.cruise = limited_velocity;
        }

        if (limited_accel < block.acceleration) {
            block.acceleration = limited_accel;
            // Recalculate dependent values
            block.max_dv2 = 2.0f * block.distance * limited_accel;
        }
    }
};
```

### 4.2 Per-Axis Limiter Setup

Create axis limiters from printer configuration:

```cpp
struct AxisLimiters {
    std::optional<AxisLimiter> x_limiter;
    std::optional<AxisLimiter> y_limiter;
    std::optional<AxisLimiter> z_limiter;

    void setup_from_config(const MachineLimitsConfig& config, size_t mode_index) {
        // X axis limiter
        if (config.machine_max_speed_x.values.size() > mode_index &&
            config.machine_max_acceleration_x.values.size() > mode_index) {
            float max_v = config.machine_max_speed_x.values[mode_index];
            float max_a = config.machine_max_acceleration_x.values[mode_index];
            if (max_v > 0 && max_a > 0) {
                x_limiter = AxisLimiter{{1, 0, 0}, max_v, max_a};
            }
        }

        // Y axis limiter
        if (config.machine_max_speed_y.values.size() > mode_index &&
            config.machine_max_acceleration_y.values.size() > mode_index) {
            float max_v = config.machine_max_speed_y.values[mode_index];
            float max_a = config.machine_max_acceleration_y.values[mode_index];
            if (max_v > 0 && max_a > 0) {
                y_limiter = AxisLimiter{{0, 1, 0}, max_v, max_a};
            }
        }

        // Z axis limiter (typically much lower than X/Y)
        if (config.machine_max_speed_z.values.size() > mode_index &&
            config.machine_max_acceleration_z.values.size() > mode_index) {
            float max_v = config.machine_max_speed_z.values[mode_index];
            float max_a = config.machine_max_acceleration_z.values[mode_index];
            if (max_v > 0 && max_a > 0) {
                z_limiter = AxisLimiter{{0, 0, 1}, max_v, max_a};
            }
        }
    }

    void apply_all(TimeBlock& block) const {
        if (x_limiter) x_limiter->apply(block);
        if (y_limiter) y_limiter->apply(block);
        if (z_limiter) z_limiter->apply(block);
    }
};
```

### 4.3 Extruder Limiter Implementation

For extrude-only moves (no XYZ motion), the extruder limits apply:

```cpp
struct ExtruderLimiter {
    float max_velocity;    // mm/s (max_extrude_only_velocity)
    float max_accel;       // mm/s² (max_extrude_only_accel)

    void apply(TimeBlock& block) const {
        // Only applies to extrude-only moves
        if (!block.is_extrude_only()) return;

        float e_rate = std::abs(block.rate_e);
        if (e_rate < 0.0001f) return;

        // For extrude-only moves, e_rate == 1 (or -1 for retract)
        // So we can apply limits directly
        float inv_e_rate = 1.0f / e_rate;

        float limited_velocity = max_velocity * inv_e_rate;
        float limited_accel = max_accel * inv_e_rate;

        // Apply limits
        float limited_v2 = limited_velocity * limited_velocity;
        if (limited_v2 < block.max_cruise_v2) {
            block.max_cruise_v2 = limited_v2;
            block.feedrate_profile.cruise = limited_velocity;
        }

        if (limited_accel < block.acceleration) {
            block.acceleration = limited_accel;
            block.max_dv2 = 2.0f * block.distance * limited_accel;
        }
    }
};
```

### 4.4 Combined Move Checker

Create a unified interface for all move checkers:

```cpp
class MoveCheckers {
public:
    AxisLimiters axis_limiters;
    ExtruderLimiter extruder_limiter;
    bool enabled{false};

    void setup(const MachineLimitsConfig& config, size_t mode_index) {
        axis_limiters.setup_from_config(config, mode_index);

        // Extruder limits - use jerk settings as velocity limits for extrude-only
        // Note: OrcaSlicer doesn't have explicit max_extrude_only_velocity
        // We derive it from existing parameters
        extruder_limiter.max_velocity = config.machine_max_jerk_e.values[mode_index];
        extruder_limiter.max_accel = config.machine_max_acceleration_retracting.values[mode_index];

        enabled = true;
    }

    void apply(TimeBlock& block) const {
        if (!enabled) return;

        axis_limiters.apply_all(block);

        // Note: Extruder limiter only applies to extrude-only moves
        extruder_limiter.apply(block);
    }
};
```

### 4.5 Integration into TimeMachine

Add move checkers to the TimeMachine structure:

```cpp
struct TimeMachine {
    // Existing fields...

    MoveCheckers move_checkers;

    // Initialize during setup
    void init_move_checkers(const MachineLimitsConfig& config, size_t mode_index) {
        if (estimator_mode != EstimatorMode::Legacy) {
            move_checkers.setup(config, mode_index);
        }
    }
};
```

### 4.6 Integration into process_G1()

Apply move checkers after creating the TimeBlock:

```cpp
void GCodeProcessor::process_G1(const GCodeReader::GCodeLine& line) {
    // ... existing code to create TimeBlock ...

    TimeBlock& block = curr_machine.blocks.back();

    // Initialize Klipper fields
    if (curr_machine.estimator_mode != EstimatorMode::Legacy) {
        // ... existing Klipper field initialization ...

        // Apply move checkers BEFORE junction calculation
        curr_machine.move_checkers.apply(block);

        // NOW calculate junction velocity with limited values
        if (!curr_machine.blocks.empty() && curr_machine.blocks.size() > 1) {
            const TimeBlock& prev = curr_machine.blocks[curr_machine.blocks.size() - 2];
            calculate_klipper_junction(block, prev, curr_machine.klipper_state);
        }
    }

    // ... rest of existing code ...
}
```

### 4.7 Z-Axis Special Handling

Z-axis moves are typically much slower than XY. Ensure proper handling:

```cpp
void AxisLimiter::apply(TimeBlock& block) const {
    // ... existing calculation ...

    // Special case: pure Z moves (layer changes, Z-hop)
    if (axis == Vec3f(0, 0, 1)) {
        // For pure Z moves, also update smoothed_dv2
        if (limited_accel < block.acceleration) {
            // Z moves typically have gentler acceleration requirements
            float z_accel_to_decel = limited_accel;  // Use full accel for Z
            block.smoothed_dv2 = 2.0f * block.distance * z_accel_to_decel;
        }
    }
}
```

### 4.8 Feature-Specific Acceleration

OrcaSlicer has per-feature acceleration settings. Ensure these are respected:

```cpp
// In process_G1, after determining the extrusion role:
float feature_accel = get_feature_acceleration(block.role, machine_limits, mode_index);
if (feature_accel > 0 && feature_accel < block.acceleration) {
    block.acceleration = feature_accel;
    block.max_dv2 = 2.0f * block.distance * feature_accel;
    block.smoothed_dv2 = 2.0f * block.distance *
        std::min(feature_accel, curr_machine.klipper_state.accel_to_decel);
}

float get_feature_acceleration(ExtrusionRole role,
                               const MachineLimitsConfig& limits,
                               size_t mode) {
    switch (role) {
        case erExternalPerimeter:
            return limits.machine_max_acceleration_outer_wall.values[mode];
        case erPerimeter:
            return limits.machine_max_acceleration_inner_wall.values[mode];
        case erInternalInfill:
            return limits.machine_max_acceleration_infill.values[mode];
        case erBridgeInfill:
            return limits.machine_max_acceleration_bridge.values[mode];
        case erTopSolidInfill:
            return limits.machine_max_acceleration_top_surface.values[mode];
        case erTravel:
            return limits.machine_max_acceleration_travel.values[mode];
        default:
            return limits.machine_max_acceleration_extruding.values[mode];
    }
}
```

---

## Testing & Validation

### Unit Tests

1. **X-axis limiter**:
   - Pure X move at 500mm/s, X limit 300mm/s → limited to 300mm/s
   - 45° XY move at 500mm/s, X limit 300mm/s → limited to ~424mm/s (300 × √2)

2. **Z-axis limiter**:
   - Z-hop at 100mm/s, Z limit 10mm/s → limited to 10mm/s
   - Diagonal XYZ move respects all limits

3. **Extruder limiter**:
   - Extrude-only at 100mm/s, limit 50mm/s → limited to 50mm/s
   - Retract respects retraction acceleration

### Integration Tests

1. **Layer change moves**:
   - Verify Z-limited velocity is respected
   - Total time increases appropriately

2. **High-speed travel**:
   - Verify axis limits are applied
   - Compare with actual printer behavior

3. **Complex print with mixed moves**:
   - Each move type should respect its limits
   - Time breakdown by feature should be accurate

---

## Deliverables Checklist

- [ ] AxisLimiter struct implementation
- [ ] Per-axis limiter setup from config
- [ ] ExtruderLimiter implementation
- [ ] MoveCheckers unified interface
- [ ] Integration into TimeMachine
- [ ] Integration into process_G1/G2_G3
- [ ] Z-axis special handling
- [ ] Feature-specific acceleration support
- [ ] Unit tests for each limiter
- [ ] Integration tests

---

## Notes for Implementation

### Parameter Sources

| Klipper Parameter | OrcaSlicer Source | Notes |
|-------------------|-------------------|-------|
| max_x_velocity | machine_max_speed_x | From printer profile |
| max_x_accel | machine_max_acceleration_x | From printer profile |
| max_z_velocity | machine_max_speed_z | Typically 5-20 mm/s |
| max_z_accel | machine_max_acceleration_z | Typically 100-500 mm/s² |
| max_extrude_only_velocity | machine_max_jerk_e | Approximation |
| max_extrude_only_accel | machine_max_acceleration_retracting | For retracts |

### Application Order

The order of applying limiters matters:

1. **Axis limiters first**: These set hard physical limits
2. **Feature-specific acceleration**: May further reduce acceleration
3. **Junction calculation last**: Uses the limited values

### Edge Cases

- Moves with zero component in an axis are not limited by that axis
- Very small moves may have numerical precision issues with ratios
- Ensure inf/nan checks when dividing by small axis components
