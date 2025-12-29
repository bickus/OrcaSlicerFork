# Deliverable 7: Validation - Completion Report

## Summary

Implemented fixes to align OrcaSlicer's Klipper time estimation with the reference `klipper_estimator` implementation. After analysis and fixes, the time estimation improved but is still not fully accurate.

**Current Status**: Print time estimate is **5h55m** vs actual **6h16m** (approximately 5.6% underestimate)

**Previous Status**: Print time was 54% overestimated before fixes from earlier deliverables.

## Reference Implementation Analyzed

### klipper_estimator Source Files

Location: `/mnt/e/wsl/git/klipper_estimator/lib/src/`

| File | Purpose | Key Functions Analyzed |
|------|---------|----------------------|
| `planner.rs` | Core motion planner | `apply_junction()`, `process()`, `PlanningMove::new_kinematic_move()` |
| `gcode.rs` | G-code parsing | Command processing |
| `arcs.rs` | Arc handling | G2/G3 linearization |
| `firmware_retraction.rs` | Firmware retraction | G10/G11 handling |

### Key Algorithm Details from klipper_estimator

#### Junction Calculation (`apply_junction()` - planner.rs:399-430)

```rust
fn apply_junction(&mut self, previous_move: &PlanningMove, toolhead_state: &ToolheadState) {
    // Skip non-kinematic moves
    if !self.is_kinematic_move() || !previous_move.is_kinematic_move() {
        return;
    }

    // Calculate junction angle
    let mut junction_cos_theta = -self.rate.xyz().dot(previous_move.rate.xyz());
    if junction_cos_theta > 0.999999 {
        return;  // Co-linear, skip
    }
    junction_cos_theta = junction_cos_theta.max(-0.999999);

    // Half-angle calculations
    let sin_theta_d2 = (0.5 * (1.0 - junction_cos_theta)).sqrt();
    let r = sin_theta_d2 / (1.0 - sin_theta_d2);
    let tan_theta_d2 = sin_theta_d2 / (0.5 * (1.0 + junction_cos_theta)).sqrt();

    // Centripetal for BOTH blocks
    let move_centripetal_v2 = 0.5 * self.distance * tan_theta_d2 * self.acceleration;
    let prev_move_centripetal_v2 = 0.5 * previous_move.distance * tan_theta_d2 * previous_move.acceleration;

    let extruder_v2 = toolhead_state.extruder_junction_speed_v2(self, previous_move);

    // Combined constraints - ALL of these must be applied
    self.max_start_v2 = extruder_v2
        .min(r * self.junction_deviation * self.acceleration)
        .min(r * previous_move.junction_deviation * previous_move.acceleration)  // BOTH blocks
        .min(move_centripetal_v2)
        .min(prev_move_centripetal_v2)  // BOTH blocks
        .min(self.max_cruise_v2)
        .min(previous_move.max_cruise_v2)
        .min(previous_move.max_start_v2 + previous_move.max_dv2);  // CRITICAL constraint

    // Smoothed velocity propagation
    self.max_smoothed_v2 = self.max_start_v2
        .min(previous_move.max_smoothed_v2 + previous_move.smoothed_dv2);
}
```

#### Velocity Planning (`process()` - planner.rs:638-710)

```rust
fn process(&mut self, partial: bool) {
    let mut delayed: Vec<(&mut PlanningMove, f64, f64)> = Vec::new();
    let mut next_end_v2 = 0.0;
    let mut next_smoothed_v2 = 0.0;
    let mut peak_cruise_v2 = 0.0;

    // Process backward
    for m in moves.iter_mut().rev() {
        let reachable_start_v2 = next_end_v2 + m.max_dv2;
        let start_v2 = m.max_start_v2.min(reachable_start_v2);
        let reachable_smoothed_v2 = next_smoothed_v2 + m.smoothed_dv2;
        let smoothed_v2 = m.max_smoothed_v2.min(reachable_smoothed_v2);

        if smoothed_v2 < reachable_smoothed_v2 {
            // This is a velocity peak
            peak_cruise_v2 = m.max_cruise_v2.min((smoothed_v2 + reachable_smoothed_v2) * 0.5);

            // Process delayed moves
            if !delayed.is_empty() {
                let mut mc_v2 = peak_cruise_v2;
                for (m, ms_v2, me_v2) in delayed.iter_mut().rev() {
                    mc_v2 = mc_v2.min(*ms_v2);
                    m.set_junction(ms_v2.min(mc_v2), mc_v2, me_v2.min(mc_v2));
                }
                delayed.clear();
            }

            // Set this block's velocities
            let cruise_v2 = ((start_v2 + reachable_start_v2) * 0.5)
                .min(m.max_cruise_v2)
                .min(peak_cruise_v2);
            m.set_junction(start_v2.min(cruise_v2), cruise_v2, next_end_v2.min(cruise_v2));
        } else {
            // Delay this move
            delayed.push((m, start_v2, next_end_v2));
        }

        next_end_v2 = start_v2;
        next_smoothed_v2 = smoothed_v2;
    }
}
```

#### Time Calculation (planner.rs:486-512)

```rust
pub fn accel_distance(&self) -> f64 {
    (self.cruise_v * self.cruise_v - self.start_v * self.start_v) * 0.5 / self.acceleration
}

pub fn accel_time(&self) -> f64 {
    self.accel_distance() / ((self.start_v + self.cruise_v) * 0.5)
}

pub fn cruise_distance(&self) -> f64 {
    (self.distance - self.accel_distance() - self.decel_distance()).max(0.0)
}

pub fn cruise_time(&self) -> f64 {
    self.cruise_distance() / self.cruise_v
}

pub fn decel_distance(&self) -> f64 {
    (self.cruise_v * self.cruise_v - self.end_v * self.end_v) * 0.5 / self.acceleration
}

pub fn decel_time(&self) -> f64 {
    self.decel_distance() / ((self.end_v + self.cruise_v) * 0.5)
}

pub fn total_time(&self) -> f64 {
    self.accel_time() + self.cruise_time() + self.decel_time()
}
```

## OrcaSlicer Files Modified

### `src/libslic3r/GCode/GCodeProcessor.cpp`

#### 1. `calculate_klipper_junction()` (lines 284-369)

**Changes Made:**
- Added co-linear move early exit (>0.999999 threshold) matching klipper_estimator
- Added centripetal velocity calculation for **both** current and previous blocks
- Added junction deviation constraint using **both** blocks' accelerations
- Added missing `prev_block->max_start_v2 + prev_block->max_dv2` constraint
- Fixed smoothed velocity propagation to use `prev.max_smoothed_v2 + prev.smoothed_dv2`

**Before:**
```cpp
float max_start_v2 = std::min({jd_v2, cent_v2, ext_v2});
max_start_v2 = std::min(max_start_v2, prev_block->klipper.max_cruise_v2);
max_start_v2 = std::min(max_start_v2, curr_block.klipper.max_cruise_v2);
curr_block.klipper.max_smoothed_v2 = max_start_v2;  // Just copied!
```

**After:**
```cpp
float max_start_v2 = ext_v2;
max_start_v2 = std::min(max_start_v2, r * junction_deviation * curr_block.acceleration);
max_start_v2 = std::min(max_start_v2, r * prev_block->klipper.junction_deviation * prev_block->acceleration);
max_start_v2 = std::min(max_start_v2, move_centripetal_v2);
max_start_v2 = std::min(max_start_v2, prev_move_centripetal_v2);
max_start_v2 = std::min(max_start_v2, curr_block.klipper.max_cruise_v2);
max_start_v2 = std::min(max_start_v2, prev_block->klipper.max_cruise_v2);
max_start_v2 = std::min(max_start_v2, prev_block->klipper.max_start_v2 + prev_block->klipper.max_dv2);

curr_block.klipper.max_smoothed_v2 = std::min(
    max_start_v2,
    prev_block->klipper.max_smoothed_v2 + prev_block->klipper.smoothed_dv2);
```

#### 2. `klipper_process_moves()` (lines 501-593)

**Changes Made:**
- Replaced separate `klipper_backward_pass()` and `klipper_forward_pass()` with unified algorithm
- Implements `smoothed_v2` and `reachable_smoothed_v2` calculations
- Added delayed move handling for velocity peaks
- Uses proper cruise velocity formula: `min((start_v2 + reachable_start_v2) * 0.5, max_cruise_v2, peak_cruise_v2)`

#### 3. `calculate_time_klipper()` (lines 1057-1097)

**Changes Made:**
- Updated to use new `klipper_process_moves()` instead of old separate passes
- Removed `klipper_prev_batch_end_v` (no longer needed with new algorithm)

### `src/libslic3r/GCode/GCodeProcessor.hpp`

#### Changes Made:
- Removed unused `klipper_prev_batch_end_v` field (line 579)

## Issues Identified But NOT Fixed

### 1. Time Calculation Formula Differences

klipper_estimator uses average velocity for time calculation:
```rust
pub fn accel_time(&self) -> f64 {
    self.accel_distance() / ((self.start_v + self.cruise_v) * 0.5)
}
```

OrcaSlicer uses delta-v / acceleration:
```cpp
accel_time = (cruise_v - start_v) / accel;
```

Both are mathematically equivalent for constant acceleration, but floating-point behavior may differ slightly.

### 2. Potential Issues Still to Investigate

1. **Arc handling (G2/G3)**: klipper_estimator linearizes arcs into small segments. OrcaSlicer may handle them differently.

2. **Move checkers**: klipper_estimator has `AxisLimiter` and `ExtruderLimiter` that may apply additional velocity constraints not fully replicated.

3. **Firmware retraction**: G10/G11 handling may differ.

4. **Minimum cruise ratio**: klipper_estimator supports `minimum_cruise_ratio` as alternative to `max_accel_to_decel`. This affects `accel_to_decel` calculation.

5. **Per-feature acceleration**: OrcaSlicer has feature-specific accelerations (outer_wall, infill, etc.) that may not be correctly integrated.

6. **Batch processing edge effects**: The batch processing in OrcaSlicer may cause edge effects at batch boundaries that affect time accumulation.

### 3. Current Estimate Analysis

| Metric | Value |
|--------|-------|
| OrcaSlicer Estimate | 5h 55m |
| Actual Print Time | 6h 16m |
| Difference | -21 minutes |
| Error | -5.6% (underestimate) |

The estimate went from 54% overestimate to 5.6% underestimate, which is a significant improvement but still not accurate.

## Files Reviewed During Investigation

### OrcaSlicer Design Documents
- `/mnt/e/wsl/git/OrcaSlicerFork/doc/proper_print_time/KLIPPER_PRINT_TIME_ESTIMATION_LOGIC.md`
- `/mnt/e/wsl/git/OrcaSlicerFork/doc/proper_print_time/design/agent_prompts/07_validation.md`
- `/mnt/e/wsl/git/OrcaSlicerFork/doc/proper_print_time/design/agent_prompts/completed/03_junction_velocity_complete.md`
- `/mnt/e/wsl/git/OrcaSlicerFork/doc/proper_print_time/design/agent_prompts/completed/04_two_pass_planning_complete.md`
- `/mnt/e/wsl/git/OrcaSlicerFork/doc/proper_print_time/design/agent_prompts/completed/06_integration_complete.md`

### OrcaSlicer Implementation Files
- `/mnt/e/wsl/git/OrcaSlicerFork/src/libslic3r/GCode/GCodeProcessor.cpp`
- `/mnt/e/wsl/git/OrcaSlicerFork/src/libslic3r/GCode/GCodeProcessor.hpp`

### klipper_estimator Reference Files
- `/mnt/e/wsl/git/klipper_estimator/lib/src/planner.rs` (primary reference)
- `/mnt/e/wsl/git/klipper_estimator/lib/src/gcode.rs`
- `/mnt/e/wsl/git/klipper_estimator/lib/src/arcs.rs`
- `/mnt/e/wsl/git/klipper_estimator/lib/src/firmware_retraction.rs`

## Recommendations for Future Agents

### High Priority
1. **Compare G-code processing**: Run klipper_estimator on the same G-code file and compare per-move times to identify where discrepancies occur.

2. **Debug logging**: Add temporary logging to output `start_v`, `cruise_v`, `end_v`, and `time` for each block, then compare with klipper_estimator output.

3. **Arc handling**: Verify G2/G3 arc linearization matches klipper_estimator's `mm_per_arc_segment` behavior.

### Medium Priority
4. **Move checkers**: Implement full `AxisLimiter` logic if per-axis velocity/acceleration limits are configured.

5. **Minimum cruise ratio**: Add support for `minimum_cruise_ratio` parameter as alternative to `max_accel_to_decel`.

6. **Batch boundary effects**: Investigate if batch processing causes time accumulation errors at boundaries.

### Low Priority
7. **Floating-point precision**: Consider using double precision for critical calculations.

8. **Unit test**: Create unit tests comparing OrcaSlicer calculations with klipper_estimator for known inputs.

## Conclusion

The implementation now much more closely matches klipper_estimator's algorithm, but there are still discrepancies causing ~5.6% underestimation. The most likely causes are:
1. Differences in arc handling
2. Move checker implementation gaps
3. Batch processing edge effects
4. Potential differences in how feature-specific accelerations are applied

Further debugging with side-by-side comparison of per-move times is recommended to identify the remaining discrepancy source.

**Status: PARTIALLY COMPLETE** - Algorithm alignment improved, but accuracy target not yet met.
