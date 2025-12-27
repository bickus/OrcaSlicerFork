# Assumptions and Design Decisions

This document records all assumptions made during the design of the Klipper-compatible print time estimation system, along with rationale for key design decisions.

---

## Parameter Assumptions (CRITICAL)

---

### A1: Jerk Parameters ARE Square Corner Velocity for Klipper

**CRITICAL: OrcaSlicer does NOT have a separate "Square Corner Velocity" parameter.**

**Assumption**: For Klipper printers, the jerk parameters serve as Square Corner Velocity:

| OrcaSlicer Parameter | Used As (Klipper) |
|---------------------|-------------------|
| `machine_max_jerk_x` | Square Corner Velocity (X component) |
| `machine_max_jerk_y` | Square Corner Velocity (Y component) |
| `machine_max_jerk_e` | Instantaneous Corner Velocity (extruder) |

**How to get SCV**: `scv = min(machine_max_jerk_x, machine_max_jerk_y)`

**Rationale**: OrcaSlicer's existing klipper_actual_speed feature already uses this mapping. The Klipper documentation and codebase confirm that the "jerk" setting in Klipper profiles corresponds to SCV.

**Source**: `doc/klipper_actual_speed/design/02_motion_model.md`
> "Orca don't have parameters named Square Corner Velocity, however for Klipper machines parameters specified in Jerk are used as Square Corner Velocity"

**Runtime Override**: The `SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=` G-code command can override the jerk-derived SCV at runtime. The implementation should update the internal SCV value when this command is parsed.

**Impact**: No new UI parameters needed. The existing jerk settings are used directly.

---

### A2: Instant Corner Velocity from Extruder Jerk

**Assumption**: `machine_max_jerk_e` is used as `instant_corner_velocity` for extruder junction calculations.

**Rationale**: Klipper's `instantaneous_corner_velocity` defaults to 1.0 mm/s in most configurations. The extruder jerk setting provides a reasonable approximation for how fast the extruder can change direction.

**Impact**: Uses existing parameter; may need tuning for specific printers.

---

### A3: Junction Deviation is Derived from Jerk (as SCV)

**Assumption**: Junction deviation is calculated from jerk-as-SCV and max acceleration rather than being a separate configuration parameter.

**Formula**: `junction_deviation = jerk² × 0.41421356 / max_acceleration`

Where `jerk = min(machine_max_jerk_x, machine_max_jerk_y)`.

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

## Compatibility Assumptions (CRITICAL)

### A13: Legacy Mode Code Path is UNTOUCHED

**Assumption**: The existing time estimation code (Legacy mode) is **not modified in any way**. The new Klipper algorithm is implemented as a completely separate code path.

**Rationale**: This guarantees:
1. Zero risk of regression for Marlin, RepRapFirmware, and other firmware
2. Byte-identical time estimates for non-Klipper printers before and after this change
3. Easy rollback by simply not selecting Klipper mode
4. Clear separation of concerns

**Implementation**:
```cpp
void TimeMachine::calculate_time() {
    if (estimator_mode == EstimatorMode::Klipper) {
        calculate_time_klipper();  // NEW code path
    } else {
        calculate_time_legacy();   // EXISTING code, renamed but unchanged
    }
}
```

The `calculate_time_legacy()` function is the EXISTING `calculate_time()` function, renamed. Its implementation is NOT modified.

---

### A14: Non-Klipper Printers ALWAYS Use Legacy

**Assumption**: Only printers with `gcode_flavor == gcfKlipper` use the new estimator. All other printers use the existing Legacy algorithm.

**Affected firmware (use Legacy - UNCHANGED):**
- Marlin / Marlin 2
- RepRapFirmware (RRF)
- Smoothieware
- Repetier
- Mach3 / Mach4
- Sailfish
- Teacup
- Any other non-Klipper firmware

**Only affected firmware (uses NEW Klipper algorithm):**
- Klipper (gcfKlipper)

**Rationale**:
1. The Klipper algorithm is specifically designed for Klipper's unique motion system
2. Marlin's jerk-based system is fundamentally different from Klipper's junction deviation
3. Applying the wrong algorithm would make estimates WORSE, not better
4. Each firmware should ideally have its own algorithm matching its motion planner

**Future Consideration**: Other firmware-specific algorithms could be added later (e.g., Marlin 2's S-curve acceleration), but that is OUT OF SCOPE for this implementation.

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
