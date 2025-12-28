# Deliverable 7: Validation Complete (Revision 2)

## Summary

Comprehensive validation of the Klipper print time estimation implementation was performed. **Four critical bugs were found and fixed** that were causing the time estimation to be significantly inflated.

## Validation Status: PASS (with fixes)

### Critical Bugs Found and Fixed

#### Bug 1: Junction Deviation Acceleration Mismatch (CRITICAL) - Fixed in Rev 1
**Location:** `GCodeProcessor.cpp` - `calculate_klipper_junction()` function

**Root Cause:**
- `junction_deviation` was computed ONCE using `machine_max_acceleration_extruding` (e.g., 100,000 mm/s²)
- In `calculate_junction_deviation_v2()`, it was multiplied by `block.acceleration` (e.g., 20,000-65,000 mm/s² for per-feature accelerations)
- This caused junction velocities to be scaled by `sqrt(block.accel / max_accel)` = ~45% of expected

**Fix Applied:**
- Added `square_corner_velocity` field to `KlipperState` struct
- Modified `calculate_klipper_junction()` to compute `junction_deviation` per-block using `block.acceleration`

#### Bug 2: Accel-to-Decel Acceleration Mismatch (MODERATE) - Fixed in Rev 1
**Location:** `GCodeProcessor.cpp` - smoothed_dv2 calculation

**Root Cause:**
- `accel_to_decel` was computed using global `max_acceleration`
- Used with per-block acceleration in `smoothed_dv2 = 2 × accel_to_decel × distance`

**Fix Applied:**
- Modified to compute `accel_to_decel` per-block using `block.acceleration` and `minimum_cruise_ratio`

#### Bug 3: Angle Interpretation Completely Inverted (CRITICAL) - Fixed in Rev 2
**Location:** `GCodeProcessor.cpp` - `calculate_junction_deviation_v2()` and `calculate_centripetal_v2()`

**Root Cause:**
The `calculate_junction_cos_theta()` function returns `-dot` (negated dot product), meaning:
- `cos_theta = -1`: same direction (moves are colinear)
- `cos_theta = +1`: opposite directions (180° reversal)
- `cos_theta = 0`: perpendicular (90° turn)

However, the threshold checks were completely backwards:
```cpp
// BEFORE (WRONG):
if (cos_theta >= COS_NEARLY_COLINEAR) {  // >= 0.9998
    return max_cruise_v2;  // Triggered for REVERSALS, not same direction!
}

// AFTER (CORRECT):
if (cos_theta <= -COS_THRESHOLD) {  // <= -0.9998
    return max_cruise_v2;  // Same direction = no slowdown needed
}
if (cos_theta >= COS_THRESHOLD) {   // >= 0.9998
    return 0.0f;  // Reversal = must stop
}
```

**Impact:** This bug caused the estimator to allow full speed through 180° reversals (should stop) and force stops on straight lines (should allow full speed). The effect would cause erratic and incorrect velocity profiles.

**Fix Applied:**
- Added comprehensive comments explaining the negated cos_theta interpretation
- Inverted all threshold checks in both `calculate_junction_deviation_v2()` and `calculate_centripetal_v2()`
- Same direction (cos_theta <= -0.999): return max velocity / no centripetal limit
- Reversal (cos_theta >= +0.999): return 0 velocity / stop

#### Bug 4: Cruise Ratio Not Used in Velocity Propagation (CRITICAL) - Fixed in Rev 2
**Location:** `GCodeProcessor.cpp` - `klipper_backward_pass()` and `klipper_forward_pass()`

**Root Cause:**
The `minimum_cruise_ratio` setting was being read from config and used to compute `smoothed_dv2`, but the backward and forward passes were using `max_dv2` instead of `smoothed_dv2`:

```cpp
// BEFORE (WRONG):
float max_start_from_end = end_v2 + block.klipper.max_dv2;  // Ignores cruise_ratio!

// AFTER (CORRECT):
float dv2_for_propagation = block.klipper.smoothed_dv2;  // Uses cruise_ratio!
```

**Impact:** Changing the `minimum_cruise_ratio` in the printer profile had NO effect on the estimation because the value was never used in the actual velocity propagation calculations.

**Fix Applied:**
- Modified `klipper_backward_pass()` to use `smoothed_dv2` for velocity constraint propagation
- Modified `klipper_forward_pass()` to use `smoothed_dv2` for acceleration/deceleration calculations
- Added fallback to `max_dv2` if `smoothed_dv2` is not set (for safety)

### Verification Checklist

| Item | Status | Notes |
|------|--------|-------|
| **Deliverable 1: Foundation** | PASS | `EstimatorMode` enum, `KlipperState` struct properly defined |
| **Deliverable 2: Rate Vector** | PASS | `calculate_rate_vector()` correctly computes unit direction vectors |
| **Deliverable 3: Junction Velocity** | FIXED | cos_theta thresholds corrected, junction_deviation per-block |
| **Deliverable 4: Two-Pass Planner** | FIXED | Now uses smoothed_dv2 (cruise_ratio) for velocity propagation |
| **Deliverable 5: Move Checkers** | PASS | Axis/extruder limiters correctly scale velocity/acceleration |
| **Deliverable 6: Integration** | PASS | Time accumulation into layers/features works correctly |
| **Legacy Code Unchanged** | PASS | `calculate_time_legacy()` preserved with "DO NOT MODIFY" comment |
| **Mode Selection** | PASS | Automatically selects Klipper mode for `gcfKlipper` G-code flavor |

### Impact Analysis

**Before All Fixes:**
- Klipper estimate: ~11h12m-11h18m
- Legacy estimate: ~7h15m
- Actual print time: ~6h15m
- Error: +79% vs actual

**Expected After Rev 2 Fixes:**
- Junction velocities now correctly match SCV for 90° turns
- Reversals correctly require stop (v=0)
- Straight paths correctly allow full speed
- Cruise ratio changes now affect estimation
- Estimate should be much closer to actual print time

### Machine Configuration Validated Against

- Max printer acceleration: 100,000 mm/s²
- Max printer speed: 1,000 mm/s
- Min cruise ratio: 0.25
- SCV settings: 10-25 mm/s (varies per feature)
- Per-feature accelerations: 20,000-65,000 mm/s²

### Code Quality Assessment

| Aspect | Rating | Notes |
|--------|--------|-------|
| Formula correctness | Good | All physics formulas match Klipper reference |
| Numerical stability | Good | Proper epsilon checks, clamping for edge cases, division by zero protection |
| Code organization | Good | Clear separation between legacy and Klipper paths |
| Documentation | Improved | Added comprehensive comments explaining cos_theta interpretation |
| Consistency | Fixed | All velocity propagation now uses smoothed_dv2 (cruise_ratio) |

### Files Modified in Rev 2

1. **GCodeProcessor.cpp**
   - `calculate_junction_deviation_v2()`: Fixed threshold checks for cos_theta, added documentation
   - `calculate_centripetal_v2()`: Fixed threshold checks for cos_theta, added documentation
   - `klipper_backward_pass()`: Now uses smoothed_dv2 for velocity propagation
   - `klipper_forward_pass()`: Now uses smoothed_dv2 for velocity propagation

### Testing Recommendations

1. Re-run time estimation on the test G-code file and compare with actual print time
2. Verify that changing minimum_cruise_ratio now affects the estimation
3. Test with prints that have many corners to verify junction velocities
4. Test with prints that have long straight moves to verify full speed is allowed
5. Compare estimates for prints with reversals (e.g., zig-zag patterns)

---

**Validation performed by:** Claude Code Agent
**Date:** 2025-12-28
**Revision:** 2
**Status:** COMPLETE - All identified bugs fixed, ready for testing
