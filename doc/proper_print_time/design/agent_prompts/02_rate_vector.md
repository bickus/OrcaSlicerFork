# Deliverable 2: Rate Vector Calculation

## Task Overview

You are implementing rate vector calculation for the Klipper print time estimation feature. This is deliverable 2 of 8. Your work builds on the foundation established in Deliverable 1 and provides the rate vectors needed for junction velocity calculation in Deliverable 3.

## Prerequisites

Before starting, read:
1. **Previous agent's work**: `doc/proper_print_time/design/agent_prompts/completed/01_foundation_complete.md`
2. **Design documents**:
   - `doc/proper_print_time/design/02_junction_velocity.md` - Section on rate vectors
   - `doc/proper_print_time/design/01_foundation.md` - TimeBlock structure

## Important Constraints

1. **NO COMPILATION/EXECUTION**: You cannot compile or run the application. All validation must be done through static code analysis and careful review.

2. **NO TEST WRITING**: Do not create test files. Use the validation criteria below as a self-check.

3. **PRESERVE LEGACY BEHAVIOR**: Do not modify any Legacy code paths. Your changes only affect blocks when `estimator_mode == Klipper`.

4. **DOCUMENTATION REQUIRED**: Create completion report when done.

## Background

Rate vectors represent the direction and relative rates of motion for each axis during a move. They are essential for:
- Calculating the angle between consecutive moves (for junction velocity)
- Determining if a move is kinematic (has XY motion) or non-kinematic (E-only)
- Properly handling extruder rate changes at junctions

The rate vector has two components:
- `rate_xyz`: Unit direction vector for X, Y, Z motion (normalized to length 1.0)
- `rate_e`: Signed extruder rate relative to the total move distance

## Files to Modify

1. **`src/libslic3r/GCode/GCodeProcessor.cpp`**
   - Add rate vector calculation function
   - Call rate vector calculation when processing G1 moves
   - Call rate vector calculation when processing G2/G3 arc moves

## Specific Tasks

### Task 1: Add Rate Vector Calculation Function

Add a helper function to calculate rate vectors from move endpoints:

```cpp
namespace {
    // Calculate rate vector for a move
    // start, end: 4D vectors (X, Y, Z, E)
    // Returns: rate_xyz (unit vector) and rate_e (signed scalar)
    void calculate_rate_vector(
        const Vec4d& start,
        const Vec4d& end,
        Vec3f& rate_xyz,
        float& rate_e,
        bool& is_kinematic,
        bool& has_xy_motion)
    {
        Vec3d delta_xyz(end.x() - start.x(), end.y() - start.y(), end.z() - start.z());
        double delta_e = end.w() - start.w();  // w() is the 4th component (E)

        double xyz_length = delta_xyz.norm();
        double total_length = std::sqrt(xyz_length * xyz_length + delta_e * delta_e);

        // Determine move type
        double xy_length = std::sqrt(delta_xyz.x() * delta_xyz.x() +
                                      delta_xyz.y() * delta_xyz.y());
        has_xy_motion = xy_length > 0.0001;  // 0.1 micron threshold
        is_kinematic = has_xy_motion || std::abs(delta_xyz.z()) > 0.0001;

        if (total_length < 0.0001) {
            // Zero-length move
            rate_xyz = Vec3f::Zero();
            rate_e = 0.0f;
            return;
        }

        if (xyz_length > 0.0001) {
            // Normalize XYZ component
            rate_xyz = (delta_xyz / xyz_length).cast<float>();
        } else {
            // Pure E move
            rate_xyz = Vec3f::Zero();
        }

        // E rate relative to XYZ distance (for kinematic moves)
        // or relative to E distance (for E-only moves)
        if (is_kinematic && xyz_length > 0.0001) {
            rate_e = static_cast<float>(delta_e / xyz_length);
        } else {
            rate_e = (delta_e > 0) ? 1.0f : -1.0f;
        }
    }
}
```

### Task 2: Initialize Klipper Fields in TimeBlock

Add a helper to initialize Klipper fields for a new block:

```cpp
namespace {
    void init_klipper_fields(GCodeProcessor::TimeBlock& block) {
        block.klipper.rate_xyz = Vec3f::Zero();
        block.klipper.rate_e = 0.0f;
        block.klipper.max_cruise_v2 = 0.0f;
        block.klipper.max_dv2 = 0.0f;
        block.klipper.smoothed_dv2 = 0.0f;
        block.klipper.max_start_v2 = 0.0f;
        block.klipper.max_smoothed_v2 = 0.0f;
        block.klipper.junction_deviation = 0.0f;
        block.klipper.is_kinematic = true;
        block.klipper.has_xy_motion = true;
        block.klipper.resolved_start_v = 0.0f;
        block.klipper.resolved_cruise_v = 0.0f;
        block.klipper.resolved_end_v = 0.0f;
    }
}
```

### Task 3: Integrate into process_G1()

Find the `process_G1()` function where TimeBlocks are created for G1 moves. After the block is created and before it's added to the processing queue, add rate vector calculation:

```cpp
void GCodeProcessor::process_G1(const GCodeReader::GCodeLine& line) {
    // ... existing code to parse G1 and create block ...

    // After creating the TimeBlock but before adding to queue:
    if (machine.estimator_mode == EstimatorMode::Klipper) {
        init_klipper_fields(block);

        // Get start and end positions
        // Note: Adjust these based on how GCodeProcessor tracks positions
        Vec4d start(m_start_position.x(), m_start_position.y(),
                    m_start_position.z(), m_start_position.e());
        Vec4d end(m_end_position.x(), m_end_position.y(),
                  m_end_position.z(), m_end_position.e());

        calculate_rate_vector(start, end,
            block.klipper.rate_xyz,
            block.klipper.rate_e,
            block.klipper.is_kinematic,
            block.klipper.has_xy_motion);

        // Copy junction deviation from machine state
        block.klipper.junction_deviation = machine.klipper_state.junction_deviation;
    }

    // ... existing code to add block to queue ...
}
```

### Task 4: Integrate into process_G2_G3()

Find the `process_G2_G3()` function that handles arc moves. Arc moves are typically linearized into multiple segments. Apply the same rate vector calculation to each segment:

```cpp
void GCodeProcessor::process_G2_G3(const GCodeReader::GCodeLine& line) {
    // ... existing code that generates arc segments ...

    // For each segment (typically in a loop):
    for (/* each arc segment */) {
        // ... existing segment block creation ...

        if (machine.estimator_mode == EstimatorMode::Klipper) {
            init_klipper_fields(segment_block);

            Vec4d seg_start(/* segment start position */);
            Vec4d seg_end(/* segment end position */);

            calculate_rate_vector(seg_start, seg_end,
                segment_block.klipper.rate_xyz,
                segment_block.klipper.rate_e,
                segment_block.klipper.is_kinematic,
                segment_block.klipper.has_xy_motion);

            segment_block.klipper.junction_deviation =
                machine.klipper_state.junction_deviation;
        }

        // ... existing code to add segment to queue ...
    }
}
```

### Task 5: Handle Special Cases

Ensure proper handling of edge cases:

```cpp
// In calculate_rate_vector, handle these cases:

// 1. Zero-length moves (already handled above)
if (total_length < 0.0001) {
    rate_xyz = Vec3f::Zero();
    rate_e = 0.0f;
    is_kinematic = false;  // No motion
    has_xy_motion = false;
    return;
}

// 2. E-only moves (retraction/prime)
if (!is_kinematic) {
    // rate_xyz already Zero from above
    // rate_e already set to +/-1
    // is_kinematic = false means junction velocity uses extruder-only logic
}

// 3. Z-only moves (layer change, Z-hop)
if (!has_xy_motion && std::abs(delta_xyz.z()) > 0.0001) {
    // rate_xyz will be (0, 0, +/-1)
    // is_kinematic = true (still uses XYZ junction logic)
    // has_xy_motion = false (no XY component)
}

// 4. Travel moves (no extrusion)
if (std::abs(delta_e) < 0.0001) {
    rate_e = 0.0f;
    // Normal XYZ junction calculation applies
}
```

## Self-Validation Checklist

After completing your implementation, verify each item:

- [ ] **calculate_rate_vector() function exists** with correct signature
- [ ] **rate_xyz is normalized** to unit length for non-zero XYZ moves
- [ ] **rate_xyz is zero** for E-only moves
- [ ] **is_kinematic is false** only for E-only moves (no XYZ motion)
- [ ] **has_xy_motion is false** for Z-only and E-only moves
- [ ] **Zero-length moves handled** without division by zero
- [ ] **init_klipper_fields() initializes all fields** to sensible defaults
- [ ] **process_G1() calls rate calculation** only when Klipper mode
- [ ] **process_G2_G3() calls rate calculation** for each arc segment
- [ ] **Legacy mode unaffected** - no changes to non-Klipper code paths
- [ ] **Correct position variables used** (verify m_start_position, m_end_position names)
- [ ] **junction_deviation copied** from machine state to block

## Numerical Validation

Mentally verify these scenarios produce correct results:

1. **Pure X move** (1,0,0,0) to (2,0,0,0.1):
   - rate_xyz = (1, 0, 0)
   - rate_e = 0.1 (extrusion per mm)
   - is_kinematic = true
   - has_xy_motion = true

2. **Diagonal XY move** (0,0,0,0) to (1,1,0,0.14):
   - rate_xyz = (0.707, 0.707, 0)
   - rate_e = 0.1 (0.14 / 1.414)
   - is_kinematic = true
   - has_xy_motion = true

3. **Pure Z move** (0,0,0,0) to (0,0,1,0):
   - rate_xyz = (0, 0, 1)
   - rate_e = 0
   - is_kinematic = true
   - has_xy_motion = false

4. **Retraction** (0,0,0,0) to (0,0,0,-2):
   - rate_xyz = (0, 0, 0)
   - rate_e = -1
   - is_kinematic = false
   - has_xy_motion = false

## Documentation Requirements

Create completion report at:
`doc/proper_print_time/design/agent_prompts/completed/02_rate_vector_complete.md`

Include:
1. **Summary**: What was implemented
2. **Files Modified**: With line numbers
3. **Position Variable Names**: Document the actual variable names used for tracking position (m_start_position, etc.) as the next agent will need these
4. **Design Decisions**: Any choices made
5. **Deviations**: Differences from design (with justification)
6. **Known Issues**: Concerns for next agent
7. **Self-Validation Results**: Completed checklist

## Commit Requirements

Commit with message: "Deliverable 2: Add rate vector calculation for Klipper moves"

Push to branch `claude/fix-print-time-estimation-tmz9u`

## What NOT to Do

- DO NOT modify Legacy time estimation code
- DO NOT implement junction velocity calculation (that's Deliverable 3)
- DO NOT create test files
- DO NOT try to compile or run the code
- DO NOT change the TimeBlock structure (already done in Deliverable 1)
