# Implementation Deliverables Breakdown

This document breaks the implementation into self-contained deliverables that can be implemented and validated iteratively.

---

## Deliverable 1: Foundation (Estimated: 1-2 days)

### Scope
Establish the infrastructure for Klipper-compatible time estimation without changing any runtime behavior.

### Files to Modify
- `src/libslic3r/GCode/GCodeProcessor.hpp`
- `src/libslic3r/GCode/GCodeProcessor.cpp`

### Tasks

1. **Add EstimatorMode enum**
   ```cpp
   enum class EstimatorMode : uint8_t { Legacy = 0, Klipper = 1 };
   ```

2. **Add KlipperState struct to TimeMachine**
   ```cpp
   struct KlipperState {
       float junction_deviation{0.0f};
       float accel_to_decel{50.0f};
       float instant_corner_velocity{1.0f};
   };
   ```

3. **Extend TimeBlock with Klipper fields**
   - `Vec3f rate_xyz`, `float rate_e`
   - `float max_cruise_v2`, `max_dv2`, `smoothed_dv2`
   - `float max_start_v2`, `max_smoothed_v2`
   - `float junction_deviation`
   - `bool is_kinematic`, `has_xy_motion`
   - `float resolved_start_v`, `resolved_cruise_v`, `resolved_end_v`

4. **Implement derived parameter calculations**
   - `compute_junction_deviation(scv, max_accel)`
   - `compute_accel_to_decel(max_accel, cruise_ratio)`

5. **Add feature-flag guards**
   - `calculate_time()` → calls `calculate_time_legacy()` or `calculate_time_klipper()`
   - `calculate_time_klipper()` stub that calls legacy for now

6. **Wire up EstimatorMode selection in apply_config()**

### Validation Criteria
- [ ] Builds without errors
- [ ] All existing tests pass
- [ ] **CRITICAL: Non-Klipper printers produce IDENTICAL time estimates** (run before/after comparison)
- [ ] Time estimates unchanged when EstimatorMode::Legacy is used
- [ ] Klipper flavor printers correctly set EstimatorMode::Klipper
- [ ] Non-Klipper flavor printers use EstimatorMode::Legacy
- [ ] New fields initialized to reasonable defaults

### Non-Regression Test (REQUIRED)

Before merging this deliverable, run the following test:

```bash
# 1. Slice a test model with a MARLIN printer profile BEFORE changes
orca_slicer --export-gcode --load marlin_profile.ini test.3mf -o before.gcode

# 2. Apply changes and rebuild

# 3. Slice the same model with the same profile AFTER changes
orca_slicer --export-gcode --load marlin_profile.ini test.3mf -o after.gcode

# 4. Compare - time estimates MUST be identical
diff <(grep "estimated printing time" before.gcode) \
     <(grep "estimated printing time" after.gcode)
# Expected: No difference
```

Repeat for RepRapFirmware, Smoothieware, and other non-Klipper profiles.

### Dependencies
None (first deliverable)

---

## Deliverable 2: Rate Vector Calculation (Estimated: 0.5-1 day)

### Scope
Calculate the rate vectors needed for junction velocity calculation.

### Files to Modify
- `src/libslic3r/GCode/GCodeProcessor.cpp`

### Tasks

1. **Add rate vector calculation in process_G1()**
   ```cpp
   void calculate_rate_vector(TimeBlock& block,
       const Vec4d& start, const Vec4d& end);
   ```

2. **Add rate vector calculation in process_G2_G3()**
   - Same logic as G1 for the linearized arc segments

3. **Initialize Klipper fields when creating blocks**
   - Call `init_klipper_fields()` for each new block

4. **Handle special cases**
   - Zero-length moves
   - E-only moves (is_kinematic = false)
   - Z-only moves (has_xy_motion = false)

### Validation Criteria
- [ ] Rate vectors have unit length for XYZ component
- [ ] is_kinematic correctly identifies E-only moves
- [ ] has_xy_motion correctly identifies Z-only and E-only moves
- [ ] No impact on time estimation (still using legacy)

### Dependencies
Deliverable 1

---

## Deliverable 3: Junction Velocity (Estimated: 2-3 days)

### Scope
Implement the Klipper junction velocity calculation algorithm.

### Files to Modify
- `src/libslic3r/GCode/GCodeProcessor.cpp`
- (possibly) `src/libslic3r/GCode/GCodeProcessor.hpp` for helper structs

### Tasks

1. **Implement angle calculation**
   ```cpp
   float calculate_junction_cos_theta(const Vec3f& prev, const Vec3f& curr);
   ```

2. **Implement junction deviation velocity**
   ```cpp
   JunctionResult calculate_junction_deviation_v2(...);
   ```

3. **Implement centripetal velocity**
   ```cpp
   float calculate_centripetal_v2(float dist, float accel, float cos_theta);
   ```

4. **Implement extruder junction velocity**
   ```cpp
   float calculate_extruder_junction_v2(float prev_e, float curr_e, float icv);
   ```

5. **Implement combined junction calculation**
   ```cpp
   void calculate_klipper_junction(TimeBlock& curr, const TimeBlock& prev,
       const KlipperState& state);
   ```

6. **Integrate into process_G1() and process_G2_G3()**
   - Call junction calculation after adding block to queue

### Validation Criteria
- [ ] Co-linear moves have full-speed junctions
- [ ] 90° turns have reduced junction velocity
- [ ] 180° turns (reversals) have near-zero junction velocity
- [ ] Extruder rate changes limit junction velocity
- [ ] Unit tests pass for known angle/velocity combinations

### Dependencies
Deliverable 2

---

## Deliverable 4: Two-Pass Velocity Planning (Estimated: 2-3 days)

### Scope
Implement the backward/forward velocity planning passes.

### Files to Modify
- `src/libslic3r/GCode/GCodeProcessor.cpp`
- `src/libslic3r/GCode/GCodeProcessor.hpp`

### Tasks

1. **Implement backward pass**
   ```cpp
   void klipper_backward_pass(std::vector<TimeBlock>& blocks,
       std::vector<DelayedMove>& delayed);
   ```

2. **Implement forward pass for delayed moves**
   ```cpp
   void klipper_forward_pass(std::vector<TimeBlock>& blocks,
       std::vector<DelayedMove>& delayed, float peak_cruise_v2);
   ```

3. **Implement trapezoid calculation**
   ```cpp
   void calculate_klipper_trapezoid(TimeBlock& block);
   ```

4. **Implement block time calculation**
   ```cpp
   float calculate_klipper_block_time(const TimeBlock& block);
   ```

5. **Implement calculate_time_klipper()**
   - Run passes, calculate times, accumulate

6. **Implement incremental batching**
   ```cpp
   void flush_klipper_blocks(bool force_all);
   ```

### Validation Criteria
- [ ] Single moves correctly accelerate from/to zero
- [ ] Consecutive co-linear moves maintain velocity
- [ ] Complex sequences produce smooth velocity profiles
- [ ] Batch processing produces same results as full processing
- [ ] No numerical instabilities (inf, nan)

### Dependencies
Deliverable 3

---

## Deliverable 5: Move Checkers (Estimated: 1-2 days)

### Scope
Implement axis-specific and extruder-specific velocity limiters.

### Files to Modify
- `src/libslic3r/GCode/GCodeProcessor.cpp`
- `src/libslic3r/GCode/GCodeProcessor.hpp`

### Tasks

1. **Implement AxisLimiter**
   - Apply velocity/acceleration limits per axis
   - Handle diagonal moves correctly

2. **Implement ExtruderLimiter**
   - Limit extrude-only moves

3. **Implement MoveCheckers container**
   - Setup from config
   - Apply all checkers in correct order

4. **Integrate into TimeMachine**
   - Initialize checkers during setup
   - Apply before junction calculation

5. **Add feature-specific acceleration support**
   - Per-role acceleration (infill, perimeter, etc.)

### Validation Criteria
- [ ] Pure Z moves limited to Z axis speed
- [ ] Diagonal XY moves respect both X and Y limits
- [ ] Extrude-only moves respect extruder limits
- [ ] Feature accelerations applied correctly

### Dependencies
Deliverable 4

---

## Deliverable 6: Integration and Consistency (Estimated: 1-2 days)

### Scope
Ensure all parts work together and results are consistent across the UI.

### Files to Modify
- `src/libslic3r/GCode/GCodeProcessor.cpp`
- Possibly preview/UI files for kinematics sync

### Tasks

1. **Update finalize() to flush all blocks**

2. **Implement per-layer time accumulation**
   - Match layer IDs correctly
   - Handle edge cases (negative layer IDs)

3. **Implement per-feature time accumulation**
   - By ExtrusionRole
   - By move type

4. **Synchronize preview kinematics**
   - Update record_block_kinematics()
   - Use resolved velocities for visualization

5. **Verify result consistency**
   - Total time matches sum of layers
   - Feature times sum to total

### Validation Criteria
- [ ] Layer times sum to approximately total time
- [ ] Feature times sum to approximately total time
- [ ] Preview velocities match estimation velocities
- [ ] No missing or double-counted time

### Dependencies
Deliverable 5

---

## Deliverable 7: Validation and Testing (Estimated: 2-3 days)

### Scope
Comprehensive testing and validation against klipper_estimator, plus verification that non-Klipper printers remain unchanged.

### Files to Create/Modify
- `tests/libslic3r/test_klipper_time_estimation.cpp`
- `tests/libslic3r/test_legacy_time_estimation_unchanged.cpp`
- Validation scripts

### Tasks

1. **Create unit test suite for Klipper algorithm**
   - Junction velocity tests
   - Trapezoid calculation tests
   - Edge case tests

2. **Create integration test suite for Klipper**
   - Simple shapes (cube, cylinder)
   - Complex infill patterns
   - Travel-heavy prints

3. **Create validation harness for klipper_estimator comparison**
   - Compare with klipper_estimator
   - Automated comparison script

4. **Create NON-REGRESSION test suite for Legacy algorithm**
   - Test Marlin profiles produce identical results
   - Test RepRapFirmware profiles produce identical results
   - Test other non-Klipper profiles produce identical results
   - Compare against baseline captures from BEFORE implementation

5. **Profile performance**
   - Measure time estimation overhead
   - Optimize if needed

6. **Document known limitations**
   - Update ASSUMPTIONS.md with any new findings

### Validation Criteria
- [ ] All Klipper unit tests pass
- [ ] ±5% accuracy vs klipper_estimator on test suite
- [ ] **CRITICAL: All Legacy/non-Klipper tests produce IDENTICAL results to baseline**
- [ ] <100ms overhead for 1-hour print
- [ ] No crashes on edge cases

### Non-Klipper Regression Testing

Create baseline captures BEFORE implementation:
```bash
# Generate baselines for multiple firmware types
for profile in marlin rrf smoothie repetier; do
    orca_slicer --export-gcode --load ${profile}.ini test.3mf -o baseline_${profile}.gcode
done
```

After implementation, verify identical output:
```bash
for profile in marlin rrf smoothie repetier; do
    orca_slicer --export-gcode --load ${profile}.ini test.3mf -o after_${profile}.gcode
    diff baseline_${profile}.gcode after_${profile}.gcode
    # MUST show no difference
done
```

### Dependencies
Deliverable 6

---

## Deliverable 8: Documentation and Polish (Estimated: 1 day)

### Scope
Final documentation and code cleanup.

### Tasks

1. **Code review and cleanup**
   - Remove debug code
   - Ensure consistent style
   - Add comments where needed

2. **Update user documentation**
   - Explain improved Klipper estimation
   - Document any new settings

3. **Update developer documentation**
   - Architecture overview
   - How to modify/extend

4. **Create changelog entry**

5. **Final validation**
   - Full test suite
   - Manual testing on various printers

### Deliverables
- Clean, reviewed code
- Updated documentation
- Ready for merge

### Dependencies
Deliverable 7

---

## Summary Timeline

| Deliverable | Estimated Days | Dependencies |
|-------------|---------------|--------------|
| 1. Foundation | 1-2 | None |
| 2. Rate Vector | 0.5-1 | D1 |
| 3. Junction Velocity | 2-3 | D2 |
| 4. Two-Pass Planning | 2-3 | D3 |
| 5. Move Checkers | 1-2 | D4 |
| 6. Integration | 1-2 | D5 |
| 7. Validation | 2-3 | D6 |
| 8. Documentation | 1 | D7 |
| **Total** | **11-17 days** | - |

---

## Iteration Strategy

Each deliverable should be:

1. **Implemented** on a feature branch
2. **Tested** with unit tests and manual verification
3. **Validated** against specific criteria
4. **Merged** before starting next deliverable

This allows:
- Early detection of issues
- Course correction if needed
- Partial rollback if necessary
- Clear progress tracking

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Algorithm complexity | Start with simplified version, add features incrementally |
| Performance regression | Profile early, optimize if needed |
| Numerical instability | Use v² throughout, test edge cases |
| Breaking existing behavior | Feature flags, comprehensive testing |
| Integration issues | Test with actual Klipper printers |
