# Phase 2: Junction Velocity Calculation

## Overview

This phase implements the Klipper junction velocity algorithm, which determines the maximum speed at which the toolhead can transition between two consecutive moves. This is fundamentally different from Marlin's jerk-based approach.

---

### Terminology: SCV = Jerk Parameters

**OrcaSlicer does NOT have a separate "Square Corner Velocity" (SCV) parameter.**

Throughout this document, when "SCV" is mentioned, it refers to:
- **`min(machine_max_jerk_x, machine_max_jerk_y)`** from the printer profile

When "instant corner velocity" is mentioned, it refers to:
- **`machine_max_jerk_e`** from the printer profile

These values are already configured in Klipper printer profiles as jerk settings.

---

## Goals

1. Calculate junction angle between consecutive moves
2. Implement junction deviation velocity formula (uses jerk-as-SCV)
3. Implement centripetal velocity constraint
4. Implement extruder junction speed limiting (uses extruder jerk)
5. Combine all constraints into `max_start_v2`

---

## Algorithm Summary

The junction velocity is limited by several factors:

```
max_start_v2 = min(
    junction_deviation_v2,      // From angle + junction deviation
    centripetal_v2_current,     // Centripetal accel limit (current move)
    centripetal_v2_previous,    // Centripetal accel limit (previous move)
    extruder_v2,                // Extrusion rate change limit
    max_cruise_v2_current,      // Can't exceed cruise speed
    max_cruise_v2_previous,     // Previous move's cruise limit
    propagated_v2               // From previous move's start + max_dv2
)
```

---

## Work Items

### 2.1 Angle Calculation

Calculate the cosine of the angle between consecutive moves:

**File**: `GCodeProcessor.cpp`, new function

```cpp
// Calculate cos(θ) where θ is the angle between two direction vectors
// Note: We want the angle AT the junction, so we use negative dot product
float calculate_junction_cos_theta(const Vec3f& prev_rate, const Vec3f& curr_rate) {
    // cos(θ) = -(prev · curr) because we want the angle at the junction
    float cos_theta = -(prev_rate.dot(curr_rate));

    // Clamp to valid range to handle floating point errors
    return std::clamp(cos_theta, -0.999999f, 0.999999f);
}

// Check if moves are co-linear (same direction)
bool are_moves_colinear(float cos_theta) {
    return cos_theta > 0.999999f;
}
```

### 2.2 Junction Deviation Velocity

**Mathematical Background**:
```
# SCV = min(machine_max_jerk_x, machine_max_jerk_y) - read from jerk params!
junction_deviation = jerk² × (√2 - 1) / max_acceleration

sin(θ/2) = √(0.5 × (1 - cos_θ))
r = sin(θ/2) / (1 - sin(θ/2))

junction_v² = r × junction_deviation × acceleration
```

Note: The `scv` value passed to `compute_junction_deviation()` comes from `min(machine_max_jerk_x, machine_max_jerk_y)`. See Phase 1 for the parameter reading code.

**Implementation**:

```cpp
struct JunctionResult {
    float v2;                    // Maximum junction velocity squared
    bool is_colinear;            // True if moves are same direction
};

JunctionResult calculate_junction_deviation_v2(
    const TimeBlock& prev,
    const TimeBlock& curr,
    float cos_theta
) {
    JunctionResult result = {std::numeric_limits<float>::infinity(), false};

    // Skip if moves are co-linear
    if (cos_theta > 0.999999f) {
        result.is_colinear = true;
        return result;
    }

    // Calculate sin(θ/2) and r
    float sin_theta_d2 = std::sqrt(0.5f * (1.0f - cos_theta));
    float r = sin_theta_d2 / (1.0f - sin_theta_d2);

    // Junction velocity from current move's junction deviation
    float v2_curr = r * curr.junction_deviation * curr.acceleration;

    // Junction velocity from previous move's junction deviation
    float v2_prev = r * prev.junction_deviation * prev.acceleration;

    // Return the minimum
    result.v2 = std::min(v2_curr, v2_prev);
    return result;
}
```

### 2.3 Centripetal Velocity

The centripetal constraint prevents the toolhead from sliding outward at corners:

**Mathematical Background**:
```
tan(θ/2) = sin(θ/2) / √(0.5 × (1 + cos_θ))

centripetal_v² = 0.5 × distance × tan(θ/2) × acceleration
```

**Implementation**:

```cpp
float calculate_centripetal_v2(
    float distance,
    float acceleration,
    float cos_theta
) {
    // Calculate tan(θ/2)
    float sin_theta_d2 = std::sqrt(0.5f * (1.0f - cos_theta));
    float cos_theta_d2 = std::sqrt(0.5f * (1.0f + cos_theta));

    // Avoid division by zero
    if (cos_theta_d2 < 0.0001f) {
        return 0.0f;  // Nearly 180° turn, must stop
    }

    float tan_theta_d2 = sin_theta_d2 / cos_theta_d2;

    return 0.5f * distance * tan_theta_d2 * acceleration;
}
```

### 2.4 Extruder Junction Speed

When extrusion rate changes between moves, the extruder limits junction velocity:

**Mathematical Background**:
```
diff_r = |current_e_rate - previous_e_rate|

if diff_r > 0:
    extruder_v² = (instant_corner_velocity / diff_r)²
else:
    extruder_v² = infinity  // No limit
```

**Implementation**:

```cpp
float calculate_extruder_junction_v2(
    float prev_e_rate,
    float curr_e_rate,
    float instant_corner_velocity
) {
    float diff_r = std::abs(curr_e_rate - prev_e_rate);

    if (diff_r > 0.0001f) {
        float v = instant_corner_velocity / diff_r;
        return v * v;
    }

    return std::numeric_limits<float>::infinity();
}
```

### 2.5 Combined Junction Velocity Calculation

Combine all constraints to get the final `max_start_v2`:

**File**: `GCodeProcessor.cpp`, integrate into `process_G1()` / `process_G2_G3()`

```cpp
void calculate_klipper_junction(
    TimeBlock& curr,
    const TimeBlock& prev,
    const TimeMachine::KlipperState& klipper_state
) {
    // Only calculate junction for kinematic moves following kinematic moves
    if (!curr.is_kinematic || !prev.is_kinematic) {
        // Non-kinematic moves (E-only) start and end at zero velocity
        curr.max_start_v2 = 0.0f;
        curr.max_smoothed_v2 = 0.0f;
        return;
    }

    // Calculate angle between moves
    float cos_theta = calculate_junction_cos_theta(prev.rate_xyz, curr.rate_xyz);

    // Check for co-linear moves (no corner)
    if (are_moves_colinear(cos_theta)) {
        // Can transition at full speed
        curr.max_start_v2 = std::min(curr.max_cruise_v2, prev.max_cruise_v2);
        curr.max_smoothed_v2 = curr.max_start_v2;
        return;
    }

    // Calculate all velocity constraints

    // 1. Junction deviation constraint
    JunctionResult junc = calculate_junction_deviation_v2(prev, curr, cos_theta);

    // 2. Centripetal constraints (both moves)
    float centripetal_curr_v2 = calculate_centripetal_v2(
        curr.distance, curr.acceleration, cos_theta
    );
    float centripetal_prev_v2 = calculate_centripetal_v2(
        prev.distance, prev.acceleration, cos_theta
    );

    // 3. Extruder junction constraint
    float extruder_v2 = calculate_extruder_junction_v2(
        prev.rate_e, curr.rate_e,
        klipper_state.instant_corner_velocity
    );

    // 4. Can't exceed either move's cruise speed
    float cruise_limit_v2 = std::min(curr.max_cruise_v2, prev.max_cruise_v2);

    // 5. Propagated constraint from previous move
    float propagated_v2 = prev.max_start_v2 + prev.max_dv2;

    // Final junction velocity is minimum of all constraints
    curr.max_start_v2 = std::min({
        junc.v2,
        centripetal_curr_v2,
        centripetal_prev_v2,
        extruder_v2,
        cruise_limit_v2,
        propagated_v2
    });

    // Calculate smoothed velocity (for deceleration smoothing)
    float smoothed_propagated = prev.max_smoothed_v2 + prev.smoothed_dv2;
    curr.max_smoothed_v2 = std::min(curr.max_start_v2, smoothed_propagated);
}
```

### 2.6 Integration into process_G1()

**File**: `GCodeProcessor.cpp`, in `process_G1()` around line 3200

Replace/augment existing junction calculation for Klipper mode:

```cpp
// After creating the TimeBlock and calculating distance/rate vectors...

// Initialize Klipper fields
if (machine.estimator_mode != EstimatorMode::Legacy) {
    float max_accel = machine.max_acceleration();  // Get current acceleration
    float accel_to_decel = machine.klipper_state.accel_to_decel;
    block.junction_deviation = machine.klipper_state.junction_deviation;
    block.init_klipper_fields(block.feedrate_profile.cruise, max_accel, accel_to_decel);
}

// Junction velocity calculation
if (machine.estimator_mode != EstimatorMode::Legacy && !blocks.empty()) {
    const TimeBlock& prev = blocks.back();
    calculate_klipper_junction(block, prev, machine.klipper_state);
}

// Legacy junction calculation (existing code)
else {
    // ... existing jerk-based calculation ...
}
```

### 2.7 Handle First Move

The first move in a sequence starts at zero velocity:

```cpp
// When adding the first block to the queue
if (blocks.empty()) {
    block.max_start_v2 = 0.0f;
    block.max_smoothed_v2 = 0.0f;
}
```

### 2.8 Special Cases

**Extrude-only moves**:
```cpp
if (block.is_extrude_only()) {
    // Extrude-only moves always start and end at zero
    block.max_start_v2 = 0.0f;
    block.max_smoothed_v2 = 0.0f;
}
```

**Kinematic move following E-only**:
```cpp
if (curr.is_kinematic && !prev.is_kinematic) {
    // Coming from a stop, start at zero
    curr.max_start_v2 = 0.0f;
    curr.max_smoothed_v2 = 0.0f;
}
```

**Z-only moves**:
```cpp
// Z-only moves without XY motion don't participate in SCV/junction limiting
if (!curr.has_xy_motion && !prev.has_xy_motion) {
    // Use simpler junction calculation (just acceleration limit)
    curr.max_start_v2 = std::min(curr.max_cruise_v2, prev.max_cruise_v2);
}
```

---

## Testing & Validation

### Unit Tests

1. **Angle calculation**:
   - Same direction (0°): cos_θ = -1 (moves continue straight)
   - 90° turn: cos_θ = 0
   - 180° turn: cos_θ = 1 (reversal)

2. **Junction deviation velocity**:
   - 90° turn, jd=0.01, accel=1000: v² ≈ 4.14 → v ≈ 2.03 mm/s
   - 45° turn, jd=0.01, accel=1000: v² ≈ 1.47 → v ≈ 1.21 mm/s

3. **Extruder junction**:
   - Same e_rate: no limit
   - e_rate change of 0.1 with ICV=1: v² = 100 → v = 10 mm/s

### Integration Tests

1. Parse G-code with sharp corners → Verify reduced junction velocities
2. Parse G-code with straight lines → Verify full-speed junctions
3. Parse G-code with varying extrusion rates → Verify extruder limiting

### Validation Against klipper_estimator

Create test G-code files and compare:
1. Square outline (90° corners)
2. Circle approximation (small angle changes)
3. Complex infill pattern
4. Travel moves with sharp turns

---

## Deliverables Checklist

- [ ] Angle calculation function implemented
- [ ] Junction deviation velocity calculation
- [ ] Centripetal velocity calculation
- [ ] Extruder junction calculation
- [ ] Combined junction velocity function
- [ ] Integration into process_G1()
- [ ] Integration into process_G2_G3()
- [ ] First move handling
- [ ] Special case handling (E-only, Z-only)
- [ ] Unit tests
- [ ] Validation against klipper_estimator

---

## Notes for Implementation

### Numerical Stability

- Always use v² instead of v to avoid unnecessary sqrt operations
- Clamp cos_θ to avoid NaN from acos of values slightly outside [-1, 1]
- Use epsilon comparisons (0.0001f) instead of exact zero checks

### Performance

- The junction calculation adds ~10 float operations per move
- No heap allocations needed
- Can be done inline as moves are processed

### Debugging

- Log junction velocity components to identify which constraint is limiting
- Add optional verbose mode that outputs per-move junction analysis
