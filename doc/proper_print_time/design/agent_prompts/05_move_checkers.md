# Deliverable 5: Move Checkers

## Task Overview

You are implementing axis-specific and extruder-specific velocity/acceleration limiters for Klipper time estimation. This is deliverable 5 of 8. These "move checkers" ensure that the velocity planning respects per-axis limits and feature-specific acceleration settings.


## Prerequisites

Before starting, read:
1. **Previous agents' work**:
   - `doc/proper_print_time/design/agent_prompts/completed/01_foundation_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/02_rate_vector_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/03_junction_velocity_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/04_two_pass_planning_complete.md`
2. **Design documents**:
   - `doc/proper_print_time/design/04_move_checkers.md` - **Primary reference**
   - `src/libslic3r/PrintConfig.cpp` - For config key names

## Important Constraints

1. **NO COMPILATION/EXECUTION**: All validation through static code analysis only.
2. **NO TEST WRITING**: Use validation criteria for self-checking.
3. **PRESERVE LEGACY BEHAVIOR**: Only modify Klipper code paths.
4. **DOCUMENTATION REQUIRED**: Create completion report when done.

## Background

Move checkers apply velocity and acceleration limits based on:
1. **Per-axis limits**: X, Y, Z each have independent max velocity and acceleration
2. **Extruder limits**: E axis has separate limits
3. **Feature acceleration**: Different print features (infill, perimeter, etc.) may have different acceleration limits

In Klipper, when a move is diagonal, its velocity is limited by the slowest axis it uses. For example, a 45° XY move at 100mm/s requires each axis to move at 70.7mm/s, so the total velocity is limited by both X and Y axis limits.

## Files to Modify

1. **`src/libslic3r/GCode/GCodeProcessor.hpp`**
   - Add limiter structures if needed

2. **`src/libslic3r/GCode/GCodeProcessor.cpp`**
   - Implement axis velocity limiting
   - Implement axis acceleration limiting
   - Implement extruder limiting
   - Implement feature acceleration lookup
   - Integrate limiters into block initialization

## Specific Tasks

### Task 1: Implement Axis Velocity Limiter

Apply per-axis velocity limits based on the move's direction:

```cpp
namespace {
    // Limit velocity based on per-axis maximum velocities
    // rate_xyz: unit direction vector
    // Returns: maximum allowed velocity for this move direction
    float limit_velocity_by_axis(
        const Vec3f& rate_xyz,
        float requested_velocity,
        float max_vel_x,
        float max_vel_y,
        float max_vel_z)
    {
        float max_velocity = requested_velocity;

        // For each axis, if the move has motion on that axis,
        // limit velocity so the axis component doesn't exceed max

        // X axis: if rate_xyz.x != 0, then v * |rate_xyz.x| <= max_vel_x
        // So v <= max_vel_x / |rate_xyz.x|
        if (std::abs(rate_xyz.x()) > 0.0001f) {
            float axis_limit = max_vel_x / std::abs(rate_xyz.x());
            max_velocity = std::min(max_velocity, axis_limit);
        }

        // Y axis
        if (std::abs(rate_xyz.y()) > 0.0001f) {
            float axis_limit = max_vel_y / std::abs(rate_xyz.y());
            max_velocity = std::min(max_velocity, axis_limit);
        }

        // Z axis
        if (std::abs(rate_xyz.z()) > 0.0001f) {
            float axis_limit = max_vel_z / std::abs(rate_xyz.z());
            max_velocity = std::min(max_velocity, axis_limit);
        }

        return max_velocity;
    }
}
```

### Task 2: Implement Axis Acceleration Limiter

Apply per-axis acceleration limits similarly:

```cpp
namespace {
    // Limit acceleration based on per-axis maximum accelerations
    // rate_xyz: unit direction vector
    // Returns: maximum allowed acceleration for this move direction
    float limit_acceleration_by_axis(
        const Vec3f& rate_xyz,
        float requested_accel,
        float max_accel_x,
        float max_accel_y,
        float max_accel_z)
    {
        float max_accel = requested_accel;

        // Same logic as velocity: axis component can't exceed axis limit

        if (std::abs(rate_xyz.x()) > 0.0001f) {
            float axis_limit = max_accel_x / std::abs(rate_xyz.x());
            max_accel = std::min(max_accel, axis_limit);
        }

        if (std::abs(rate_xyz.y()) > 0.0001f) {
            float axis_limit = max_accel_y / std::abs(rate_xyz.y());
            max_accel = std::min(max_accel, axis_limit);
        }

        if (std::abs(rate_xyz.z()) > 0.0001f) {
            float axis_limit = max_accel_z / std::abs(rate_xyz.z());
            max_accel = std::min(max_accel, axis_limit);
        }

        return max_accel;
    }
}
```

### Task 3: Implement Extruder Limiter

Apply extruder velocity and acceleration limits:

```cpp
namespace {
    // Limit velocity based on extruder maximum velocity
    // rate_e: extruder rate relative to XYZ distance
    // Returns: maximum allowed velocity
    float limit_velocity_by_extruder(
        float rate_e,
        float requested_velocity,
        float max_extruder_velocity)
    {
        if (std::abs(rate_e) < 0.0001f) {
            return requested_velocity;  // No extrusion, no limit
        }

        // Extrusion rate = velocity * rate_e
        // Limit: velocity * |rate_e| <= max_extruder_velocity
        float max_velocity = max_extruder_velocity / std::abs(rate_e);
        return std::min(requested_velocity, max_velocity);
    }

    // Limit acceleration based on extruder maximum acceleration
    float limit_acceleration_by_extruder(
        float rate_e,
        float requested_accel,
        float max_extruder_accel)
    {
        if (std::abs(rate_e) < 0.0001f) {
            return requested_accel;  // No extrusion, no limit
        }

        float max_accel = max_extruder_accel / std::abs(rate_e);
        return std::min(requested_accel, max_accel);
    }
}
```

### Task 4: Implement Feature Acceleration Lookup

Get acceleration based on print feature (extrusion role):

```cpp
namespace {
    // Get acceleration for a specific print feature
    // This maps ExtrusionRole to the appropriate acceleration setting
    float get_feature_acceleration(
        const PrintConfig& config,
        ExtrusionRole role,
        float default_accel)
    {
        // Map extrusion roles to their acceleration settings
        // Note: Actual config key names may vary - verify from PrintConfig.cpp

        switch (role) {
            case erPerimeter:
            case erExternalPerimeter:
                // Use outer wall acceleration if available
                if (config.outer_wall_acceleration.value > 0)
                    return config.outer_wall_acceleration.value;
                break;

            case erInternalInfill:
            case erSolidInfill:
            case erTopSolidInfill:
            case erBottomSurface:
                // Use infill acceleration if available
                if (config.infill_acceleration.value > 0)
                    return config.infill_acceleration.value;
                break;

            case erBridgeInfill:
                // Use bridge acceleration if available
                if (config.bridge_acceleration.value > 0)
                    return config.bridge_acceleration.value;
                break;

            case erSupportMaterial:
            case erSupportMaterialInterface:
                // Use support acceleration if available
                // (may not have separate config - use default)
                break;

            case erTravel:
                // Use travel acceleration if available
                if (config.travel_acceleration.value > 0)
                    return config.travel_acceleration.value;
                break;

            default:
                break;
        }

        // Fall back to default acceleration
        // This might be machine_max_acceleration or a print-level default
        return default_accel;
    }
}
```

### Task 5: Integrate Limiters into Block Initialization

In `process_G1()` and `process_G2_G3()`, apply limiters when initializing Klipper blocks:

```cpp
// In process_G1() after creating block and calculating rate vector:
if (machine.estimator_mode == EstimatorMode::Klipper) {
    // ... existing rate vector calculation ...

    // Get machine limits from config
    // Note: Verify actual config member names from previous agents' work
    float max_vel_x = m_time_processor.machine_limits.machine_max_speed_x.values[0];
    float max_vel_y = m_time_processor.machine_limits.machine_max_speed_y.values[0];
    float max_vel_z = m_time_processor.machine_limits.machine_max_speed_z.values[0];
    float max_vel_e = m_time_processor.machine_limits.machine_max_speed_e.values[0];

    float max_accel_x = m_time_processor.machine_limits.machine_max_acceleration_x.values[0];
    float max_accel_y = m_time_processor.machine_limits.machine_max_acceleration_y.values[0];
    float max_accel_z = m_time_processor.machine_limits.machine_max_acceleration_z.values[0];
    float max_accel_e = m_time_processor.machine_limits.machine_max_acceleration_e.values[0];

    // Get feedrate (requested velocity)
    float feedrate_mms = block.feedrate / 60.0f;  // mm/min to mm/s

    // Apply axis velocity limits
    float limited_velocity = limit_velocity_by_axis(
        block.klipper.rate_xyz,
        feedrate_mms,
        max_vel_x, max_vel_y, max_vel_z);

    // Apply extruder velocity limit
    limited_velocity = limit_velocity_by_extruder(
        block.klipper.rate_e,
        limited_velocity,
        max_vel_e);

    // Set max cruise velocity squared
    block.klipper.max_cruise_v2 = limited_velocity * limited_velocity;

    // Get base acceleration
    float base_accel = block.acceleration;

    // Apply feature-specific acceleration
    float feature_accel = get_feature_acceleration(
        m_config,  // or however config is accessed
        block.role,
        base_accel);

    // Apply axis acceleration limits
    float limited_accel = limit_acceleration_by_axis(
        block.klipper.rate_xyz,
        feature_accel,
        max_accel_x, max_accel_y, max_accel_z);

    // Apply extruder acceleration limit
    limited_accel = limit_acceleration_by_extruder(
        block.klipper.rate_e,
        limited_accel,
        max_accel_e);

    // Store limited acceleration and compute max_dv2
    block.acceleration = limited_accel;  // Update block acceleration
    block.klipper.max_dv2 = 2.0f * limited_accel * block.distance;

    // Compute smoothed_dv2 using accel_to_decel
    block.klipper.smoothed_dv2 = 2.0f *
        machine.klipper_state.accel_to_decel * block.distance;

    // ... junction calculation ...
}
```

### Task 6: Handle E-Only Moves Specially

E-only moves (retraction/prime) should use extruder limits directly:

```cpp
// In the Klipper block initialization:
if (!block.klipper.is_kinematic) {
    // E-only move - use extruder limits directly
    float e_velocity = std::abs(block.klipper.rate_e) * (block.feedrate / 60.0f);
    block.klipper.max_cruise_v2 = std::min(e_velocity, max_vel_e);
    block.klipper.max_cruise_v2 *= block.klipper.max_cruise_v2;  // Square it

    block.acceleration = max_accel_e;
    block.klipper.max_dv2 = 2.0f * max_accel_e * block.distance;
}
```

## Self-Validation Checklist

After completing your implementation, verify:

**Axis Velocity Limiter**:
- [ ] **Pure X move limited by max_vel_x**
- [ ] **Pure Y move limited by max_vel_y**
- [ ] **Pure Z move limited by max_vel_z**
- [ ] **45° XY move limited** so X and Y components each ≤ their limits
- [ ] **No division by zero** when rate component is zero
- [ ] **Returns minimum** of all applicable limits

**Axis Acceleration Limiter**:
- [ ] **Same logic as velocity** applied to acceleration
- [ ] **Per-axis limits respected**
- [ ] **Diagonal moves properly limited**

**Extruder Limiter**:
- [ ] **High extrusion rate limits velocity** correctly
- [ ] **Zero extrusion means no limit**
- [ ] **Works for both velocity and acceleration**

**Feature Acceleration**:
- [ ] **Perimeter uses outer_wall_acceleration** if set
- [ ] **Infill uses infill_acceleration** if set
- [ ] **Travel uses travel_acceleration** if set
- [ ] **Falls back to default** if feature config is 0 or unset

**Integration**:
- [ ] **Limiters called after rate vector calculation**
- [ ] **max_cruise_v2 uses limited velocity**
- [ ] **max_dv2 uses limited acceleration**
- [ ] **smoothed_dv2 uses accel_to_decel**
- [ ] **E-only moves handled specially**
- [ ] **Config values accessed correctly** (verify member names)
- [ ] **Legacy code unchanged**

## Numerical Validation Scenarios

1. **Pure X move at 200mm/s, max_vel_x = 150mm/s**:
   - Limited velocity = 150mm/s
   - max_cruise_v2 = 22500

2. **45° XY move at 200mm/s, max_vel_x = max_vel_y = 150mm/s**:
   - X component = 141.4mm/s (already under limit)
   - Limited velocity = 200mm/s (not limited in this case)
   - But if max_vel_x = 100mm/s: limited to 141.4mm/s

3. **Retraction at 60mm/s, max_vel_e = 50mm/s**:
   - E-only move, limited to 50mm/s

4. **External perimeter with outer_wall_acceleration = 1500**:
   - Feature acceleration = 1500
   - Then apply axis limits

5. **Infill with infill_acceleration = 5000, max_accel_x = 3000**:
   - Feature acceleration = 5000
   - After axis limiting (if diagonal): reduced appropriately

## Documentation Requirements

Create completion report at:
`doc/proper_print_time/design/agent_prompts/completed/05_move_checkers_complete.md`

Include:
1. **Summary**: Limiters implemented
2. **Files Modified**: With line numbers
3. **Config Key Names**: Actual names used for machine limits (critical for next agent)
4. **Feature Role Mapping**: Which roles map to which accelerations
5. **Design Decisions**: Choices made during implementation
6. **Deviations**: Differences from design
7. **Known Issues**: Concerns for Deliverable 6
8. **Self-Validation Results**: Completed checklist

## Commit Requirements

Commit with message: "Deliverable 5: Implement move checkers for axis and extruder limits"

Push to branch `claude/fix-print-time-estimation-tmz9u`

## What NOT to Do

- DO NOT implement layer/feature time accumulation (that's Deliverable 6)
- DO NOT modify the two-pass planner logic
- DO NOT modify Legacy code paths
- DO NOT create test files
- DO NOT try to compile or run the code
