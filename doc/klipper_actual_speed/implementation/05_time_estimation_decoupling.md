# Subtask 5 – Time Estimation Decoupling

## Overview
This document describes the fix that decouples time estimation from preview kinematics calculations. The original implementation inadvertently changed time estimation behavior for Klipper printers, which was not the intended design.

## Problem Statement
The initial Klipper cruise ratio and SCV (Square Corner Velocity) implementation modified the time estimation planner in ways that affected print time calculations:

1. **Cruise ratio in `calculate_trapezoid()`**: Limited peak speeds based on cruise ratio constraints
2. **Cruise ratio in lookahead passes**: Used reduced distances in `max_allowable_speed()` calculations
3. **SCV junction limiting**: Applied corner velocity limits in process_G1/G2_G3
4. **Multiple lookahead iterations**: Changed from 1 to 4 iterations for Klipper mode

These changes made time estimates more conservative (longer) for Klipper printers, which contradicted the design intent stated in Subtask 1: *"Current work only records and forwards the ratio; no planner math consumes it yet."*

## Solution
The fix separates time estimation from preview kinematics by:

1. **Reverting time estimation to original algorithm**:
   - `calculate_trapezoid()` uses the standard trapezoidal motion profile without cruise ratio limiting
   - Lookahead passes use full block distance (not cruise-ratio-reduced distance)
   - No SCV junction limiting in time calculation
   - Single lookahead iteration (original behavior)

2. **Preserving preview kinematics with Klipper constraints**:
   - `record_block_kinematics()` now recalculates trapezoid segments with cruise ratio limiting
   - Preview visualization shows "actual" speeds that respect Klipper's motion constraints
   - Limiting factor detection (`CruiseRatio`, `SCV`, etc.) remains functional for tooltips

## Implementation Details

### Files Modified
- `src/libslic3r/GCode/GCodeProcessor.cpp`

### Key Changes

#### 1. `TimeBlock::calculate_trapezoid()`
Reverted to original implementation that calculates standard trapezoidal motion profile:
```cpp
void GCodeProcessor::TimeBlock::calculate_trapezoid()
{
    trapezoid.cruise_feedrate = feedrate_profile.cruise;
    float accelerate_distance = estimated_acceleration_distance(...);
    float decelerate_distance = estimated_acceleration_distance(...);
    // Standard trapezoid calculation without cruise ratio
}
```

#### 2. Lookahead Passes
Reverted to use `block.distance` instead of `cruise_ratio_delta_distance(block)`:
```cpp
static void planner_forward_pass_kernel(...)
{
    float entry_speed = std::min(curr.feedrate_profile.entry,
        max_allowable_speed(-prev.acceleration, prev.feedrate_profile.entry, prev.distance));
}
```

#### 3. `TimeMachine::calculate_time()`
Reverted to single iteration:
```cpp
// forward_pass
for (size_t i = 0; i + 1 < blocks.size(); ++i)
    planner_forward_pass_kernel(blocks[i], blocks[i + 1]);

// reverse_pass
for (int i = static_cast<int>(blocks.size()) - 1; i > 0; --i)
    planner_reverse_pass_kernel(blocks[i - 1], blocks[i]);

recalculate_trapezoids(blocks);
```

#### 4. `record_block_kinematics()`
Now recalculates preview kinematics with cruise ratio constraints:
```cpp
void GCodeProcessor::record_block_kinematics(const TimeBlock& block, ...)
{
    // Start with time estimation values
    float peak_speed = block.trapezoid.cruise_feedrate;
    // ...

    // For preview, apply cruise ratio limiting
    if (block.has_cruise_ratio() && ...) {
        // Recalculate trapezoid with limited peak speed
        // This affects preview display but NOT time estimation
    }
}
```

#### 5. Removed SCV Junction Limiting
Removed the SCV corner velocity limiting code from `process_G1()` and `process_G2_G3()` that was affecting junction velocity calculations for time estimation.

#### 6. Removed Unused Function
Removed `cruise_ratio_delta_distance()` as it's no longer needed in the time estimation path.

## Behavior Summary

| Feature | Time Estimation | Preview Kinematics |
|---------|-----------------|-------------------|
| Cruise ratio limiting | Not applied | Applied |
| SCV corner limiting | Not applied | Not applied* |
| Multiple iterations | 1 iteration | N/A |
| Trapezoid calculation | Original algorithm | Recalculated with limits |

*Note: SCV limiting for preview would require storing junction velocity data from the planning phase, which adds complexity. The current fix focuses on cruise ratio limiting which has the most significant impact on preview accuracy.

## Testing Considerations
- Time estimates should match pre-Klipper-feature behavior
- Preview "Actual Speed" visualization should still show cruise-ratio-limited speeds
- Limiting factor tooltips should correctly identify "CruiseRatio" when applicable
- Non-Klipper printers should be completely unaffected

## Future Enhancements
If more accurate Klipper preview kinematics are desired (including SCV and full lookahead effects), a more comprehensive refactor could:
1. Run the planner twice: once for time, once for preview
2. Store both trapezoid sets in the TimeBlock
3. Use appropriate values for each purpose

However, this adds complexity and performance overhead. The current solution provides a good balance of accuracy and simplicity.
