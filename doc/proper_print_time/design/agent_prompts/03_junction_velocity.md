# Deliverable 3: Junction Velocity

## Task Overview

You are implementing the Klipper junction velocity calculation algorithm. This is deliverable 3 of 8. Your work uses the rate vectors from Deliverable 2 to calculate how fast the printer can traverse junctions between consecutive moves.

## Prerequisites

Before starting, read:
1. **Previous agents' work**:
   - `doc/proper_print_time/design/agent_prompts/completed/01_foundation_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/02_rate_vector_complete.md`
2. **Design documents**:
   - `doc/proper_print_time/design/02_junction_velocity.md` - **Primary reference**
   - `doc/proper_print_time/KLIPPER_PRINT_TIME_ESTIMATION_LOGIC.md` - Algorithm reference

## Important Constraints

1. **NO COMPILATION/EXECUTION**: All validation through static code analysis only.
2. **NO TEST WRITING**: Use validation criteria for self-checking.
3. **PRESERVE LEGACY BEHAVIOR**: Only modify Klipper code paths.
4. **DOCUMENTATION REQUIRED**: Create completion report when done.

## Background

Junction velocity determines the maximum speed at which the printer can transition from one move to the next. Klipper uses three limiting factors:

1. **Junction Deviation**: Geometric limit based on the angle between moves and configured deviation tolerance
2. **Centripetal Acceleration**: Acceleration limit for curved paths
3. **Extruder Limit**: Rate-of-change limit for extrusion

The final junction velocity is the **minimum** of all three limits.

### Key Formulas (from Klipper)

**Cos(theta) between moves**:
```
cos_theta = -dot(prev_rate_xyz, curr_rate_xyz)
```
Note: Negative because we want the angle between the *directions*, not the velocity vectors.

**Junction deviation velocity squared**:
```
if cos_theta >= 0.999847695:  # < 1 degree
    v² = max_cruise_v²  # Full speed
else:
    sin_half = sqrt(0.5 * (1 - cos_theta))
    cos_half = sqrt(0.5 * (1 + cos_theta))
    v² = junction_deviation * acceleration * sin_half / (1 - sin_half)
```

**Centripetal velocity squared**:
```
if cos_theta < -0.999:  # ~180 degree reversal
    v² = 0
else:
    v² = 0.5 * distance * acceleration * sqrt(1 - cos_theta * cos_theta) / (1 - cos_theta)
```

**Extruder junction velocity squared**:
```
delta_e_rate = |curr_rate_e - prev_rate_e|
if delta_e_rate > 0:
    v² = (instant_corner_velocity / delta_e_rate)²
else:
    v² = infinity  # No limit
```

## Files to Modify

1. **`src/libslic3r/GCode/GCodeProcessor.hpp`** (if needed for structs)
2. **`src/libslic3r/GCode/GCodeProcessor.cpp`** - Main implementation

## Specific Tasks

### Task 1: Implement Angle Calculation

```cpp
namespace {
    // Calculate cos(theta) between two consecutive moves
    // Returns value in range [-1, 1]
    // cos_theta = 1 means same direction (0 degrees)
    // cos_theta = -1 means opposite direction (180 degrees)
    // cos_theta = 0 means perpendicular (90 degrees)
    float calculate_junction_cos_theta(const Vec3f& prev_rate, const Vec3f& curr_rate) {
        // Dot product of unit vectors gives cos of angle between them
        // We negate because we want angle between directions of travel,
        // and prev_rate points INTO the junction while curr_rate points OUT
        float dot = prev_rate.x() * curr_rate.x() +
                    prev_rate.y() * curr_rate.y() +
                    prev_rate.z() * curr_rate.z();

        // Clamp to handle floating point errors
        return std::clamp(-dot, -1.0f, 1.0f);
    }
}
```

### Task 2: Implement Junction Deviation Velocity

```cpp
namespace {
    // Calculate max junction velocity² from junction deviation
    // This is the primary junction velocity limit in Klipper
    float calculate_junction_deviation_v2(
        float cos_theta,
        float junction_deviation,
        float acceleration,
        float max_cruise_v2)
    {
        // Nearly co-linear moves (< 1 degree angle)
        constexpr float COS_NEARLY_COLINEAR = 0.999847695f;  // cos(1°)
        if (cos_theta >= COS_NEARLY_COLINEAR) {
            return max_cruise_v2;
        }

        // Calculate sin and cos of half-angle
        // Using half-angle formulas:
        // sin(θ/2) = sqrt((1 - cos(θ)) / 2)
        // cos(θ/2) = sqrt((1 + cos(θ)) / 2)
        float sin_theta_d2 = std::sqrt(0.5f * (1.0f - cos_theta));
        float cos_theta_d2 = std::sqrt(0.5f * (1.0f + cos_theta));

        // Avoid division by zero for sharp corners
        if (sin_theta_d2 >= 0.999f) {
            // Very sharp corner (> ~160 degrees) - use minimum velocity
            return acceleration * junction_deviation;
        }

        // Junction deviation formula from Klipper
        // v² = junction_deviation * acceleration * sin(θ/2) / (1 - sin(θ/2))
        float v2 = junction_deviation * acceleration * sin_theta_d2 / (1.0f - sin_theta_d2);

        return std::min(v2, max_cruise_v2);
    }
}
```

### Task 3: Implement Centripetal Velocity

```cpp
namespace {
    // Calculate max junction velocity² from centripetal acceleration limit
    // This prevents excessive lateral acceleration on curved paths
    float calculate_centripetal_v2(
        float move_distance,
        float acceleration,
        float cos_theta)
    {
        // Near-reversal moves have zero centripetal limit
        constexpr float COS_NEAR_REVERSAL = -0.999f;
        if (cos_theta <= COS_NEAR_REVERSAL) {
            return 0.0f;
        }

        // sin(theta) from cos(theta)
        float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

        // Avoid division by zero for co-linear moves
        float denom = 1.0f - cos_theta;
        if (denom < 0.0001f) {
            return std::numeric_limits<float>::max();  // No centripetal limit
        }

        // Centripetal formula: v² = 0.5 * d * a * sin(θ) / (1 - cos(θ))
        return 0.5f * move_distance * acceleration * sin_theta / denom;
    }
}
```

### Task 4: Implement Extruder Junction Velocity

```cpp
namespace {
    // Calculate max junction velocity² from extruder rate change limit
    // This prevents excessive instantaneous extrusion rate changes
    float calculate_extruder_junction_v2(
        float prev_rate_e,
        float curr_rate_e,
        float instant_corner_velocity)
    {
        float delta_e_rate = std::abs(curr_rate_e - prev_rate_e);

        // No rate change = no limit
        if (delta_e_rate < 0.0001f) {
            return std::numeric_limits<float>::max();
        }

        // v² = (ICV / delta_e_rate)²
        float v = instant_corner_velocity / delta_e_rate;
        return v * v;
    }
}
```

### Task 5: Implement Combined Junction Calculation

```cpp
namespace {
    // Calculate all junction limits and set max_start_v2 for a block
    // prev_block: the preceding block (nullptr for first block)
    // curr_block: the current block being processed
    // klipper_state: machine's Klipper configuration
    void calculate_klipper_junction(
        const GCodeProcessor::TimeBlock* prev_block,
        GCodeProcessor::TimeBlock& curr_block,
        const GCodeProcessor::KlipperState& state)
    {
        // First block starts from rest
        if (prev_block == nullptr) {
            curr_block.klipper.max_start_v2 = 0.0f;
            curr_block.klipper.max_smoothed_v2 = 0.0f;
            return;
        }

        // Non-kinematic moves (E-only) use only extruder junction logic
        if (!curr_block.klipper.is_kinematic || !prev_block->klipper.is_kinematic) {
            // For E-only moves, junction velocity is limited by instant_corner_velocity
            float icv = state.instant_corner_velocity;
            curr_block.klipper.max_start_v2 = icv * icv;
            curr_block.klipper.max_smoothed_v2 = icv * icv;
            return;
        }

        // Get acceleration for junction calculations
        // Use the minimum of prev and curr block accelerations
        float accel = std::min(prev_block->klipper.max_dv2, curr_block.klipper.max_dv2);
        if (accel <= 0.0f) {
            // Use cruise velocity as proxy for acceleration (will be set later)
            accel = curr_block.acceleration;  // Fallback to block acceleration
        }
        // Convert from v² units if needed
        // Note: max_dv2 stores acceleration * 2 * distance, so extract accel
        // This may need adjustment based on how max_dv2 is computed

        // Calculate angle between moves
        float cos_theta = calculate_junction_cos_theta(
            prev_block->klipper.rate_xyz,
            curr_block.klipper.rate_xyz);

        // Calculate junction deviation limit
        float jd_v2 = calculate_junction_deviation_v2(
            cos_theta,
            state.junction_deviation,
            curr_block.acceleration,  // Use block's acceleration
            std::min(prev_block->klipper.max_cruise_v2, curr_block.klipper.max_cruise_v2));

        // Calculate centripetal limit
        float cent_v2 = calculate_centripetal_v2(
            curr_block.distance,  // Use current block's distance
            curr_block.acceleration,
            cos_theta);

        // Calculate extruder limit
        float ext_v2 = calculate_extruder_junction_v2(
            prev_block->klipper.rate_e,
            curr_block.klipper.rate_e,
            state.instant_corner_velocity);

        // Combined junction velocity is minimum of all limits
        float max_start_v2 = std::min({jd_v2, cent_v2, ext_v2});

        // Also limit by cruise velocities
        max_start_v2 = std::min(max_start_v2, prev_block->klipper.max_cruise_v2);
        max_start_v2 = std::min(max_start_v2, curr_block.klipper.max_cruise_v2);

        curr_block.klipper.max_start_v2 = max_start_v2;

        // Smoothed velocity uses accel_to_decel
        // For now, set equal to max_start_v2 (proper smoothing in two-pass planner)
        curr_block.klipper.max_smoothed_v2 = max_start_v2;
    }
}
```

### Task 6: Integrate into Block Processing

In `process_G1()` and `process_G2_G3()`, after rate vector calculation, call junction calculation:

```cpp
// In process_G1(), after rate vector calculation:
if (machine.estimator_mode == EstimatorMode::Klipper) {
    // ... rate vector calculation from Deliverable 2 ...

    // Set max cruise velocity squared
    // feedrate is in mm/min, convert to mm/s
    float feedrate_mms = block.feedrate / 60.0f;
    block.klipper.max_cruise_v2 = feedrate_mms * feedrate_mms;

    // Set max_dv2 based on acceleration
    // max_dv2 = 2 * acceleration * distance (for kinematic equation v² = 2*a*d)
    block.klipper.max_dv2 = 2.0f * block.acceleration * block.distance;

    // Get previous block for junction calculation
    const TimeBlock* prev_block = nullptr;
    if (!machine.blocks.empty()) {
        prev_block = &machine.blocks.back();
    }

    // Calculate junction velocity
    calculate_klipper_junction(prev_block, block, machine.klipper_state);
}
```

## Self-Validation Checklist

After completing your implementation, verify:

- [ ] **calculate_junction_cos_theta()** returns 1.0 for same direction, -1.0 for opposite
- [ ] **Dot product is negated** to get angle between travel directions
- [ ] **cos_theta clamped** to [-1, 1] range
- [ ] **calculate_junction_deviation_v2()** returns max_cruise_v2 for nearly co-linear moves
- [ ] **Half-angle formulas correct**: sin(θ/2), cos(θ/2)
- [ ] **calculate_centripetal_v2()** returns 0 for 180° reversals
- [ ] **calculate_centripetal_v2()** returns max for co-linear moves
- [ ] **calculate_extruder_junction_v2()** returns max for no rate change
- [ ] **Combined function takes minimum** of all three limits
- [ ] **First block starts from rest** (max_start_v2 = 0)
- [ ] **E-only moves use ICV limit** only
- [ ] **Integration calls junction calc** after rate vector calc
- [ ] **max_cruise_v2 set correctly** from feedrate
- [ ] **max_dv2 set correctly** from acceleration and distance
- [ ] **No division by zero** in any edge cases
- [ ] **Legacy code unaffected**

## Numerical Validation Scenarios

Verify these scenarios produce sensible results:

1. **Co-linear moves** (same direction):
   - cos_theta ≈ 1.0
   - Junction velocity = cruise velocity
   - Example: Two X+ moves in a row

2. **90° turn**:
   - cos_theta = 0
   - Junction velocity significantly reduced
   - Example: X+ move followed by Y+ move

3. **180° reversal**:
   - cos_theta = -1.0
   - Junction velocity ≈ 0
   - Example: X+ move followed by X- move

4. **Small angle** (< 5°):
   - cos_theta ≈ 0.996
   - Junction velocity near cruise velocity
   - Example: Diagonal infill lines

5. **Retraction/prime sequence**:
   - E-only moves
   - Junction velocity = instant_corner_velocity

## Documentation Requirements

Create completion report at:
`doc/proper_print_time/design/agent_prompts/completed/03_junction_velocity_complete.md`

Include:
1. **Summary**: What was implemented
2. **Files Modified**: With line numbers
3. **Algorithm Implementation**: Key formulas and their code equivalents
4. **Edge Cases Handled**: List of special cases and how they're handled
5. **Design Decisions**: Choices made during implementation
6. **Deviations**: Differences from design documents
7. **Known Issues**: Concerns for Deliverable 4
8. **Self-Validation Results**: Completed checklist

## Commit Requirements

Commit with message: "Deliverable 3: Implement Klipper junction velocity calculation"

Push to branch `claude/fix-print-time-estimation-tmz9u`

## What NOT to Do

- DO NOT implement the two-pass velocity planner (that's Deliverable 4)
- DO NOT modify time calculation yet (that's Deliverable 4)
- DO NOT modify Legacy code paths
- DO NOT create test files
- DO NOT try to compile or run the code
