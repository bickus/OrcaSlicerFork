# Assumptions and Design Decisions

This document records all assumptions made during the design of the Klipper-compatible print time estimation system, along with rationale for key design decisions.

---

## Parameter Assumptions

### A1: SCV from Jerk Parameters

**Assumption**: For Klipper printers, `machine_max_jerk_x` and `machine_max_jerk_y` represent `square_corner_velocity`, not traditional jerk.

**Rationale**: OrcaSlicer's existing klipper_actual_speed feature already uses this mapping. The Klipper documentation and codebase confirm that the "jerk" setting in Klipper profiles corresponds to SCV.

**Source**: `doc/klipper_actual_speed/design/02_motion_model.md`
> "Orca don't have parameters named Square Corner Velocity, however for Klipper machines parameters specified in Jerk are used as Square Corner Velocity"

**Impact**: No new UI parameters needed for SCV.

---

### A2: Instant Corner Velocity from Extruder Jerk

**Assumption**: `machine_max_jerk_e` is used as `instant_corner_velocity` for extruder junction calculations.

**Rationale**: Klipper's `instantaneous_corner_velocity` defaults to 1.0 mm/s in most configurations. The extruder jerk setting provides a reasonable approximation for how fast the extruder can change direction.

**Impact**: Uses existing parameter; may need tuning for specific printers.

---

### A3: Junction Deviation is Derived, Not Configured

**Assumption**: Junction deviation is calculated from SCV and max acceleration rather than being a separate configuration parameter.

**Formula**: `junction_deviation = scv² × 0.41421356 / max_acceleration`

**Rationale**:
1. Reduces configuration complexity
2. Klipper itself derives junction deviation from SCV
3. Matches the behavior described in the Klipper estimator documentation

**Trade-off**: Cannot override junction deviation independently. This is acceptable because the SCV-to-junction-deviation relationship is a fixed physical/mathematical relationship.

---

### A4: Accel-to-Decel from Cruise Ratio

**Assumption**: Use `klipper_cruise_ratio` to derive `accel_to_decel` rather than having a separate parameter.

**Formula**: `accel_to_decel = max_acceleration × (1.0 - cruise_ratio)`

**Rationale**:
1. `klipper_cruise_ratio` is already implemented (from klipper_actual_speed feature)
2. Reduces duplicate configuration
3. Matches Klipper's `minimum_cruise_ratio` behavior

**Default**: When no cruise ratio is set, use 50.0 mm/s² as the default accel_to_decel (matching Klipper's default).

---

### A5: Feature-Specific Acceleration Takes Priority

**Assumption**: When a feature-specific acceleration (e.g., `machine_max_acceleration_infill`) is set, it overrides the general `max_acceleration` for that feature.

**Rationale**: This matches existing OrcaSlicer behavior and allows fine-grained control over print speed.

**Implementation**: Apply feature acceleration after axis limiters but before junction calculation.

---

## Algorithm Assumptions

### A6: Two-Pass is Sufficient

**Assumption**: A single backward-forward pass is sufficient for most G-code files.

**Rationale**:
1. Klipper's actual planner uses streaming with limited lookahead
2. The backward pass catches most constraint conflicts
3. Testing with klipper_estimator shows convergence in one iteration for typical prints

**Fallback**: If validation shows cases requiring multiple iterations, can add iteration loop.

---

### A7: Batch Processing Maintains Accuracy

**Assumption**: Processing blocks in batches of 64-256 doesn't significantly impact accuracy vs. processing all at once.

**Rationale**:
1. Lookahead effects rarely span more than ~20 blocks
2. Sharp corners constrain velocity locally
3. Batch boundaries are handled by carrying end velocity to next batch

**Validation Required**: Compare batched vs. non-batched results on complex G-code.

---

### A8: Extrude-Only Moves Always Start/End at Zero

**Assumption**: Extrude-only moves (retractions, prime moves) always start and end at zero velocity.

**Rationale**:
1. Matches Klipper behavior for E-only moves
2. Simplifies junction calculation
3. E-only moves don't have XYZ momentum to carry

**Impact**: Slightly conservative for quick retractions; negligible impact on total time.

---

### A9: Z-Only Moves Don't Use SCV

**Assumption**: Moves that only change Z position don't participate in SCV-based junction limiting.

**Rationale**:
1. SCV is about XY corner velocity
2. Z moves (layer changes, Z-hop) are typically much slower
3. Klipper's SCV implementation focuses on XY motion

**Implementation**: Skip SCV calculation when `!has_xy_motion`.

---

## Timing Assumptions

### A10: Dwell Times are Added Directly

**Assumption**: G4 dwell times are added directly to total time without modification.

**Formula**: `delay = P_parameter / 1000` (milliseconds to seconds)

**Default**: 0.25 seconds if P not specified.

---

### A11: Indeterminate Delays Use Placeholder

**Assumption**: Commands with unknown duration (G28, M109, M190) use a small placeholder time.

**Placeholder**: 0.1 seconds

**Rationale**: These operations vary widely based on physical conditions; a placeholder acknowledges the delay without attempting to estimate it.

**User Impact**: Actual print time will exceed estimate when these operations are significant.

---

### A12: Initial Move Delay

**Assumption**: When motion starts after idle (e.g., after homing, temperature wait), add 0.25s delay.

**Rationale**: Klipper's estimator includes this to account for lookahead buffer filling and initial acceleration.

---

## Compatibility Assumptions

### A13: Legacy Mode Unchanged

**Assumption**: When `EstimatorMode::Legacy` is selected, all existing behavior is preserved exactly.

**Rationale**: This allows:
1. Gradual rollout
2. Easy A/B testing
3. Fallback for unexpected issues
4. Non-Klipper printers to continue working

**Implementation**: Feature-flagged code paths with no shared state mutations.

---

### A14: Non-Klipper Printers Use Legacy

**Assumption**: Only printers with `gcode_flavor == gcfKlipper` use the new estimator by default.

**Rationale**:
1. Algorithm is specifically designed for Klipper's motion system
2. Other firmware (Marlin, RRF) have different motion planners
3. Prevents unexpected changes for existing users

**Future**: Could add support for other firmware with their specific algorithms.

---

### A15: Preview Kinematics Match Time Estimation

**Assumption**: The velocities shown in preview ("Actual Speed" view) should match what the time estimator uses.

**Rationale**:
1. Consistency reduces user confusion
2. Debugging is easier when visualization matches calculation
3. The klipper_actual_speed feature already expects this

**Implementation**: Store resolved velocities in MoveVertex for preview to use.

---

## Performance Assumptions

### A16: Memory Bound for Block Buffer

**Assumption**: Block buffer should not exceed 256 blocks to bound memory usage.

**Rationale**:
1. Each TimeBlock is ~200 bytes
2. 256 blocks = ~50KB per time machine
3. Large G-code files may have millions of moves

**Implementation**: Flush oldest blocks when buffer reaches 256.

---

### A17: Float Precision Sufficient

**Assumption**: 32-bit float precision is sufficient for all calculations.

**Rationale**:
1. Distances are typically 0.001 to 1000 mm
2. Velocities are 0.1 to 500 mm/s
3. Accumulated error over millions of moves is negligible
4. Matches existing OrcaSlicer implementation

---

## Configuration Source Priority

When multiple sources provide the same parameter, use this priority:

1. **G-code commands** (e.g., `SET_VELOCITY_LIMIT`) - highest priority
2. **Printer profile settings** - default values
3. **Hardcoded defaults** - fallback

This matches how the actual printer firmware would behave.

---

## Known Limitations

### L1: Arc Handling

**Current State**: Arcs (G2/G3) are processed as single moves, not segmented.

**Impact**: Time estimation for arc-heavy G-code may be less accurate.

**Future Work**: Could segment arcs into linear moves for more accurate estimation, matching `mm_per_arc_segment` behavior.

---

### L2: Pressure Advance Not Modeled

**Current State**: Pressure advance timing effects are not included.

**Impact**: For high pressure advance values, actual print time may be slightly longer due to PA smoothing.

**Rationale**: PA effects are typically <1% of total time; complexity doesn't justify benefit.

---

### L3: Input Shaping Not Modeled

**Current State**: Input shaping (resonance compensation) timing effects are not included.

**Impact**: Input shaping can reduce maximum acceleration and thus increase print time.

**Future Work**: Could model input shaping's effective acceleration reduction.

---

### L4: Extruder Pressure Dynamics

**Current State**: Extruder is assumed to respond instantly to velocity changes.

**Impact**: Actual extrusion may lag, affecting corner quality but not significantly affecting time.

---

## Validation Requirements

Before final release, validate against:

1. **klipper_estimator** tool on the same G-code files
2. **Actual prints** with measured time
3. **Edge cases**: Very short moves, very long moves, complex infill patterns
4. **Different printers**: Various Klipper configurations

Target accuracy: ±5% of klipper_estimator for typical prints.

---

## Change Log

| Date | Change | Author |
|------|--------|--------|
| Initial | Document created | - |
