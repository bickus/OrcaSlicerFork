# Deliverable 4: Two-Pass Velocity Planning

## Task Overview

You are implementing the Klipper two-pass velocity planning algorithm. This is deliverable 4 of 8 and is the **core** of the Klipper time estimation algorithm. Your work takes the junction velocities from Deliverable 3 and resolves them into actual achievable velocities using backward and forward passes.

## Prerequisites

Before starting, read:
1. **Previous agents' work**:
   - `doc/proper_print_time/design/agent_prompts/completed/01_foundation_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/02_rate_vector_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/03_junction_velocity_complete.md`
2. **Design documents**:
   - `doc/proper_print_time/design/03_two_pass_planner.md` - **Primary reference**
   - `doc/proper_print_time/KLIPPER_PRINT_TIME_ESTIMATION_LOGIC.md` - Section on velocity planning

## Important Constraints

1. **NO COMPILATION/EXECUTION**: All validation through static code analysis only.
2. **NO TEST WRITING**: Use validation criteria for self-checking.
3. **PRESERVE LEGACY BEHAVIOR**: Only modify Klipper code paths.
4. **DOCUMENTATION REQUIRED**: Create completion report when done.

## Background

The two-pass planner resolves junction velocities into actual motion profiles:

1. **Backward Pass**: Propagates velocity limits from end to start, ensuring each move can decelerate to its end velocity
2. **Forward Pass**: Resolves delayed moves that couldn't be finalized during backward pass
3. **Trapezoid Calculation**: Computes acceleration/cruise/deceleration times for each move

### Key Concepts

**Delayed Moves**: In Klipper's streaming architecture, some moves must wait for future information before their velocities can be finalized. These are called "delayed moves."

**Velocity² Throughout**: All calculations use velocity squared to avoid square roots in the inner loop.

**Kinematic Equations**:
- `v² = v₀² + 2*a*d` (velocity from acceleration over distance)
- `d = v²/(2*a)` (distance to accelerate/decelerate)
- `t = v/a` (time to accelerate/decelerate)

## Files to Modify

1. **`src/libslic3r/GCode/GCodeProcessor.hpp`**
   - Add `DelayedMove` struct if needed
   - Add method declarations

2. **`src/libslic3r/GCode/GCodeProcessor.cpp`**
   - Implement backward pass
   - Implement forward pass
   - Implement trapezoid calculation
   - Implement `calculate_time_klipper()`

## Specific Tasks

### Task 1: Add DelayedMove Structure

In header file (or anonymously in cpp):

```cpp
struct DelayedMove {
    size_t block_index;         // Index into blocks vector
    float start_v2;             // Start velocity² constraint
    float cruise_v2;            // Cruise velocity² (may be reduced)
    bool can_finalize{false};   // True when end velocity is known
};
```

### Task 2: Implement Backward Pass

The backward pass processes moves from newest to oldest, propagating end velocity constraints backward:

```cpp
namespace {
    // Backward pass: propagate velocity limits from end to start
    // Sets max_start_v2 and max_cruise_v2 for each block
    void klipper_backward_pass(
        std::vector<GCodeProcessor::TimeBlock>& blocks,
        std::vector<DelayedMove>& delayed_moves)
    {
        if (blocks.empty()) return;

        // Process from end to start
        for (size_t i = blocks.size(); i > 0; --i) {
            size_t idx = i - 1;
            auto& block = blocks[idx];

            // Skip non-Klipper blocks
            if (!block.klipper.is_kinematic && block.klipper.rate_e == 0) {
                continue;
            }

            // Get end velocity constraint
            float end_v2;
            if (idx == blocks.size() - 1) {
                // Last block must end at zero
                end_v2 = 0.0f;
            } else {
                // End velocity is next block's start velocity
                end_v2 = blocks[idx + 1].klipper.max_start_v2;
            }

            // Calculate max start velocity from end velocity and acceleration
            // Using v_start² = v_end² + 2*a*d (note: + because we're going backward)
            float max_start_from_end = end_v2 + block.klipper.max_dv2;

            // Start velocity is minimum of junction limit and kinematic limit
            float new_start_v2 = std::min(block.klipper.max_start_v2, max_start_from_end);
            block.klipper.max_start_v2 = new_start_v2;

            // Update cruise velocity based on achievable start velocity
            float max_cruise_v2 = new_start_v2 + block.klipper.max_dv2;
            block.klipper.max_cruise_v2 = std::min(block.klipper.max_cruise_v2, max_cruise_v2);

            // Check if this move needs to be delayed
            // A move is delayed if we can't determine its velocity yet
            // (In simplified form, we may not need delays for batch processing)
        }
    }
}
```

### Task 3: Implement Forward Pass

The forward pass processes moves from oldest to newest, finalizing velocities:

```cpp
namespace {
    // Forward pass: finalize velocities from start to end
    // This resolves delayed moves and sets resolved_* fields
    void klipper_forward_pass(std::vector<GCodeProcessor::TimeBlock>& blocks)
    {
        if (blocks.empty()) return;

        float prev_end_v2 = 0.0f;  // First block starts from rest

        for (size_t i = 0; i < blocks.size(); ++i) {
            auto& block = blocks[i];

            // Skip non-Klipper blocks
            if (!block.klipper.is_kinematic && block.klipper.rate_e == 0) {
                block.klipper.resolved_start_v = 0.0f;
                block.klipper.resolved_cruise_v = 0.0f;
                block.klipper.resolved_end_v = 0.0f;
                prev_end_v2 = 0.0f;
                continue;
            }

            // Start velocity is constrained by previous end and junction limit
            float start_v2 = std::min(prev_end_v2, block.klipper.max_start_v2);

            // Calculate achievable cruise velocity
            float cruise_v2 = std::min(
                block.klipper.max_cruise_v2,
                start_v2 + block.klipper.max_dv2
            );

            // Get next block's start velocity limit for end velocity
            float next_start_v2;
            if (i + 1 < blocks.size()) {
                next_start_v2 = blocks[i + 1].klipper.max_start_v2;
            } else {
                next_start_v2 = 0.0f;  // Last block ends at rest
            }

            // End velocity constrained by next junction and decel capability
            float end_v2 = std::min(next_start_v2, cruise_v2);

            // Verify end velocity is achievable from cruise
            float max_end_from_cruise = cruise_v2;  // Can maintain or decelerate
            end_v2 = std::min(end_v2, max_end_from_cruise);

            // Also verify we can decelerate to end within the block distance
            // Using v_end² = v_cruise² - 2*a*d_decel
            // d_decel = (v_cruise² - v_end²) / (2*a)
            // If d_decel > block.distance, we need to reduce cruise
            float decel_accel = block.acceleration;
            float needed_decel_dist = (cruise_v2 - end_v2) / (2.0f * decel_accel);
            if (needed_decel_dist > block.distance) {
                // Reduce end velocity or cruise velocity
                end_v2 = cruise_v2 - 2.0f * decel_accel * block.distance;
                end_v2 = std::max(end_v2, 0.0f);
            }

            // Store resolved velocities (convert from v² to v)
            block.klipper.resolved_start_v = std::sqrt(start_v2);
            block.klipper.resolved_cruise_v = std::sqrt(cruise_v2);
            block.klipper.resolved_end_v = std::sqrt(end_v2);

            prev_end_v2 = end_v2;
        }
    }
}
```

### Task 4: Implement Trapezoid Calculation

Calculate the time for each move segment:

```cpp
namespace {
    // Calculate time for a block using resolved velocities
    // Returns total time for the move
    float calculate_klipper_block_time(GCodeProcessor::TimeBlock& block)
    {
        float start_v = block.klipper.resolved_start_v;
        float cruise_v = block.klipper.resolved_cruise_v;
        float end_v = block.klipper.resolved_end_v;
        float accel = block.acceleration;
        float distance = block.distance;

        if (distance < 0.0001f || accel < 0.0001f) {
            return 0.0f;
        }

        // Calculate acceleration distance: d_accel = (v_cruise² - v_start²) / (2*a)
        float accel_dist = 0.0f;
        if (cruise_v > start_v) {
            accel_dist = (cruise_v * cruise_v - start_v * start_v) / (2.0f * accel);
        }

        // Calculate deceleration distance: d_decel = (v_cruise² - v_end²) / (2*a)
        float decel_dist = 0.0f;
        if (cruise_v > end_v) {
            decel_dist = (cruise_v * cruise_v - end_v * end_v) / (2.0f * accel);
        }

        // Check if we have a cruise phase
        float cruise_dist = distance - accel_dist - decel_dist;

        if (cruise_dist < 0.0f) {
            // No cruise phase - triangle profile
            // Need to find the peak velocity where accel meets decel
            // v_peak² = v_start² + 2*a*d_accel = v_end² + 2*a*d_decel
            // d_accel + d_decel = distance
            // Solving: v_peak² = (v_start² + v_end² + 2*a*d) / 2
            float peak_v2 = (start_v * start_v + end_v * end_v + 2.0f * accel * distance) / 2.0f;
            float peak_v = std::sqrt(std::max(peak_v2, 0.0f));

            // Recalculate with peak velocity
            accel_dist = (peak_v * peak_v - start_v * start_v) / (2.0f * accel);
            decel_dist = distance - accel_dist;
            cruise_dist = 0.0f;
            cruise_v = peak_v;  // Peak velocity becomes "cruise" for time calc
        }

        // Calculate times for each phase
        // t = (v_final - v_initial) / a for accel/decel
        // t = d / v for cruise
        float accel_time = 0.0f;
        if (cruise_v > start_v && accel > 0.0f) {
            accel_time = (cruise_v - start_v) / accel;
        }

        float cruise_time = 0.0f;
        if (cruise_dist > 0.0f && cruise_v > 0.0f) {
            cruise_time = cruise_dist / cruise_v;
        }

        float decel_time = 0.0f;
        if (cruise_v > end_v && accel > 0.0f) {
            decel_time = (cruise_v - end_v) / accel;
        }

        float total_time = accel_time + cruise_time + decel_time;

        // Sanity check: time should be at least distance / cruise_v
        if (cruise_v > 0.0f) {
            float min_time = distance / cruise_v;
            total_time = std::max(total_time, min_time * 0.99f);  // Allow small tolerance
        }

        return total_time;
    }
}
```

### Task 5: Implement calculate_time_klipper()

Replace the stub from Deliverable 1:

```cpp
void GCodeProcessor::TimeMachine::calculate_time_klipper(
    size_t start_index,
    size_t count)
{
    if (blocks.empty() || count == 0) return;

    size_t end_index = std::min(start_index + count, blocks.size());

    // Create working copy of relevant blocks for planning
    // (or work directly on blocks if that's the design)

    // Step 1: Backward pass - propagate velocity constraints
    // Process only the blocks in range, but consider neighbors
    std::vector<DelayedMove> delayed;
    klipper_backward_pass(blocks, delayed);

    // Step 2: Forward pass - resolve velocities
    klipper_forward_pass(blocks);

    // Step 3: Calculate time for each block
    float total_time = 0.0f;
    for (size_t i = start_index; i < end_index; ++i) {
        float block_time = calculate_klipper_block_time(blocks[i]);
        blocks[i].time = block_time;  // Store in existing time field
        total_time += block_time;
    }

    // Note: Accumulation into layer/feature times happens elsewhere
}
```

### Task 6: Implement Batch Processing (Incremental)

For streaming G-code processing, implement incremental batch processing:

```cpp
void GCodeProcessor::TimeMachine::flush_klipper_blocks(bool force_all)
{
    if (blocks.empty()) return;

    // Keep some blocks in buffer for look-ahead
    constexpr size_t LOOK_AHEAD_BLOCKS = 32;

    size_t process_count;
    if (force_all) {
        process_count = blocks.size();
    } else if (blocks.size() > LOOK_AHEAD_BLOCKS) {
        process_count = blocks.size() - LOOK_AHEAD_BLOCKS;
    } else {
        return;  // Not enough blocks yet
    }

    if (process_count > 0) {
        calculate_time_klipper(0, process_count);

        // After processing, could move processed blocks to "done" storage
        // or accumulate their times
    }
}
```

## Self-Validation Checklist

After completing your implementation, verify:

**Backward Pass**:
- [ ] **Last block ends at v=0** (or very small velocity)
- [ ] **Start velocity limited by kinematic equation** v² = v_end² + 2ad
- [ ] **Start velocity limited by junction velocity** from Deliverable 3
- [ ] **Processes from end to start** correctly
- [ ] **Handles empty block list** gracefully

**Forward Pass**:
- [ ] **First block starts at v=0**
- [ ] **Start velocity limited by previous end velocity**
- [ ] **Start velocity limited by junction limit**
- [ ] **Cruise velocity achievable** from start
- [ ] **End velocity achievable** from cruise
- [ ] **Stores resolved velocities** (v, not v²)
- [ ] **Handles last block ending at zero**

**Trapezoid Calculation**:
- [ ] **Handles triangle profile** (no cruise phase)
- [ ] **Handles trapezoidal profile** (with cruise phase)
- [ ] **Acceleration time correct**: t = (v_cruise - v_start) / a
- [ ] **Cruise time correct**: t = d_cruise / v_cruise
- [ ] **Deceleration time correct**: t = (v_cruise - v_end) / a
- [ ] **Total distance matches** block.distance
- [ ] **No division by zero** for zero velocities
- [ ] **No negative times** returned

**Integration**:
- [ ] **calculate_time_klipper() calls all passes**
- [ ] **Block times stored** in blocks[i].time
- [ ] **flush_klipper_blocks() uses look-ahead**
- [ ] **Legacy code unchanged**

## Numerical Validation Scenarios

1. **Single isolated move** (start and end at rest):
   - Should be symmetric trapezoid or triangle
   - Total time = 2 * (v_cruise / a) + cruise_time

2. **Two consecutive same-direction moves**:
   - First move: accel to cruise, maintain or slight decel
   - Second move: maintain or accel, decel to zero
   - Junction should be at cruise velocity

3. **Move followed by 90° turn**:
   - First move ends at reduced velocity
   - Second move starts at same reduced velocity

4. **Very short move**:
   - May not reach cruise velocity
   - Triangle profile (accel then decel)

5. **Very slow feedrate**:
   - Cruise phase dominates
   - Small accel/decel times

## Documentation Requirements

Create completion report at:
`doc/proper_print_time/design/agent_prompts/completed/04_two_pass_planning_complete.md`

Include:
1. **Summary**: Core algorithm implemented
2. **Files Modified**: With line numbers
3. **Algorithm Flow**: Step-by-step execution order
4. **Kinematic Equations Used**: With code references
5. **Edge Cases Handled**: Zero velocity, short moves, etc.
6. **Design Decisions**: Buffer sizes, processing order
7. **Deviations**: Any differences from design
8. **Known Issues**: Concerns for Deliverable 5
9. **Self-Validation Results**: Completed checklist

## Commit Requirements

Commit with message: "Deliverable 4: Implement Klipper two-pass velocity planning"

Push to branch `claude/fix-print-time-estimation-tmz9u`

## What NOT to Do

- DO NOT implement move checkers (that's Deliverable 5)
- DO NOT modify per-layer/per-feature time accumulation yet (that's Deliverable 6)
- DO NOT modify Legacy code paths
- DO NOT create test files
- DO NOT try to compile or run the code
