# Klipper Time Estimation - Implementation Summary

## Entry Points

- `GCodeProcessor::process_G1()` - Creates TimeBlock with Klipper fields
- `TimeMachine::calculate_time_klipper()` - Dispatches to Klipper planner
- `klipper_process_moves()` - Backward pass velocity resolution
- `calculate_klipper_block_time()` - Per-block time calculation
- `process_SET_VELOCITY_LIMIT()` - Handles runtime parameter changes

## Key Files

| File | Functions |
|------|-----------|
| `GCodeProcessor.cpp` | All Klipper logic (lines 100-700, 1060-1200, 5142-5223) |
| `GCodeProcessor.hpp` | KlipperFields struct, TimeMachine members |
| `PrintConfig.cpp` | Settings definitions |

## Core Formulas

```
junction_deviation = SCV² × (√2 - 1) / max_acceleration
accel_to_decel = max_acceleration × (1 - minimum_cruise_ratio)
max_dv2 = 2 × distance × acceleration
smoothed_dv2 = min(2 × distance × accel_to_decel, max_dv2)
```

Junction velocity (r = sin(θ/2) / (1 - sin(θ/2))):
```
max_start_v2 = min(extruder_v2, r×JD×accel_curr, r×JD×accel_prev,
                   centripetal_curr, centripetal_prev,
                   max_cruise_curr, max_cruise_prev,
                   prev_max_start_v2 + prev_max_dv2)
```

Time: `t = (v_final - v_initial) / acceleration` for accel/decel phases

## Settings (PrintConfig.cpp)

| Setting | Default | Purpose |
|---------|---------|---------|
| `square_corner_velocity` | 5 mm/s | Junction smoothing |
| `minimum_cruise_ratio` | 0.5 | Cruise phase minimum |
| `machine_max_acceleration_*` | varies | Per-axis limits |
| `machine_max_speed_*` | varies | Per-axis limits |

## Critical State Updates

When these change via SET_VELOCITY_LIMIT, dependent values MUST update:

| Parameter Changed | Must Update |
|-------------------|-------------|
| ACCEL | accel_to_decel, junction_deviation |
| SQUARE_CORNER_VELOCITY | junction_deviation |
| MINIMUM_CRUISE_RATIO | accel_to_decel |

## Algorithm Flow

1. Parse G1 → create TimeBlock with rate_xyz, rate_e, distance
2. Apply velocity/accel limits (axis, extruder)
3. Calculate max_dv2, smoothed_dv2, max_cruise_v2
4. Calculate junction with previous block → max_start_v2, max_smoothed_v2
5. Batch blocks, periodically run klipper_process_moves()
6. Backward pass resolves actual start_v, cruise_v, end_v
7. calculate_klipper_block_time() computes trapezoid time

## Special Cases

- **E-only moves**: max_start_v2 = 0 (force stop before retraction)
- **180° reversals**: max_start_v2 = 0 (force stop)
- **First move**: Starts from v=0 (max_start_v2 initialized to 0)
- **Last move**: Ends at v=0 (next_end_v2 initialized to 0)

## Reference Implementation

`klipper_estimator` (Rust): `/mnt/e/wsl/git/klipper_estimator/lib/src/planner.rs`
- Uses f64 (double precision)
- Key functions: `apply_junction()`, `process()`, `total_time()`

## Accuracy

- Target: Match klipper_estimator (within 1 minute)
- Achieved: ~1% error (~4 min on 6h print)
- Remaining gap: Float vs double precision accumulation

## Debugging Tips

1. Compare resolved velocities between OrcaSlicer and klipper_estimator
2. Check SET_VELOCITY_LIMIT handling - state updates often missed
3. Verify E-only moves and direction reversals force v=0
4. Check smoothed_dv2 <= max_dv2 invariant maintained
