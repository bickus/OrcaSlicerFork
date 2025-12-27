# Phase 5: Integration and Testing

## Overview

This phase integrates the Klipper time estimation algorithm into the OrcaSlicer codebase, ensures all existing features continue to work, and validates accuracy against the reference klipper_estimator tool.

## Goals

1. Wire new planner into GCodeProcessor
2. Update per-layer and per-feature time accumulation
3. Ensure preview kinematics use same values
4. Comprehensive validation testing
5. Performance optimization if needed

---

## Work Items

### 5.1 GCodeProcessor Integration

The main integration point is in `GCodeProcessor::process_G1()` and `process_G2_G3()`:

**File**: `src/libslic3r/GCode/GCodeProcessor.cpp`

```cpp
void GCodeProcessor::process_G1(const GCodeReader::GCodeLine& line) {
    // ... existing parsing code ...

    for (size_t i = 0; i < m_time_processor.machines.size(); ++i) {
        TimeMachine& machine = m_time_processor.machines[i];

        // Create TimeBlock (existing code)
        TimeBlock block;
        // ... populate block fields ...

        if (machine.estimator_mode != EstimatorMode::Legacy) {
            // === NEW: Klipper-specific processing ===

            // 1. Calculate rate vector
            calculate_rate_vector(block, m_start_position, m_end_position);

            // 2. Initialize Klipper fields
            float max_accel = get_current_acceleration(machine);
            block.junction_deviation = machine.klipper_state.junction_deviation;
            block.init_klipper_fields(
                block.feedrate_profile.cruise,
                max_accel,
                machine.klipper_state.accel_to_decel
            );

            // 3. Apply move checkers
            machine.move_checkers.apply(block);

            // 4. Add to queue
            machine.blocks.push_back(block);

            // 5. Calculate junction velocity with previous block
            if (machine.blocks.size() > 1) {
                TimeBlock& curr = machine.blocks.back();
                const TimeBlock& prev = machine.blocks[machine.blocks.size() - 2];
                calculate_klipper_junction(curr, prev, machine.klipper_state);
            }

            // 6. Trigger flush if buffer is full
            machine.flush_klipper_blocks(false);
        } else {
            // === EXISTING: Legacy processing ===
            // ... existing jerk-based code unchanged ...
        }
    }
}
```

### 5.2 Final Flush on G-code End

Ensure all remaining blocks are processed when G-code parsing completes:

```cpp
void GCodeProcessor::finalize() {
    // ... existing finalization code ...

    for (size_t i = 0; i < m_time_processor.machines.size(); ++i) {
        TimeMachine& machine = m_time_processor.machines[i];

        if (machine.estimator_mode != EstimatorMode::Legacy) {
            // Flush all remaining blocks
            machine.flush_klipper_blocks(true);
        } else {
            // Existing finalization
            machine.calculate_time();
        }
    }
}
```

### 5.3 Per-Layer Time Accumulation

Update layer time tracking to use Klipper-calculated times:

```cpp
void TimeMachine::accumulate_layer_time(const TimeBlock& block, float block_time) {
    // Get current layer (from block metadata)
    int layer_id = block.layer_id;

    if (layer_id >= 0) {
        // Extend layers vector if needed
        while (layers_time.size() <= static_cast<size_t>(layer_id)) {
            layers_time.push_back(0.0f);
        }
        layers_time[layer_id] += block_time;
    }

    // Also accumulate by role
    if (block.role != ExtrusionRole::erNone) {
        roles_time[static_cast<size_t>(block.role)] += block_time;
    }

    // And by move type
    moves_time[static_cast<size_t>(block.move_type)] += block_time;
}
```

### 5.4 Preview Kinematics Synchronization

The preview system (`klipper_actual_speed` feature) should use the same velocities as the time estimator. Update `record_block_kinematics()`:

```cpp
void GCodeProcessor::record_block_kinematics(const TimeBlock& block, size_t machine_index) {
    const TimeMachine& machine = m_time_processor.machines[machine_index];

    if (machine.estimator_mode == EstimatorMode::Legacy) {
        // Existing behavior
        // ...
        return;
    }

    // For Klipper mode, use the resolved velocities directly
    if (block.g1_line_id >= 0 && block.g1_line_id < m_result.moves.size()) {
        auto& move = m_result.moves[block.g1_line_id];

        move.kinematics.entry_speed = block.resolved_start_v;
        move.kinematics.peak_speed = block.resolved_cruise_v;
        move.kinematics.exit_speed = block.resolved_end_v;

        move.kinematics.accel_distance = block.trapezoid.accelerate_until;
        move.kinematics.cruise_distance =
            block.trapezoid.decelerate_after - block.trapezoid.accelerate_until;
        move.kinematics.decel_distance =
            block.distance - block.trapezoid.decelerate_after;

        // Determine limiting factor
        move.kinematics.limiting_factor = determine_limiting_factor(block);
    }
}

MoveKinematics::LimitingFactor determine_limiting_factor(const TimeBlock& block) {
    // Compare resolved velocities to various limits

    float requested_v = block.feedrate_profile.cruise;
    float cruise_v = block.resolved_cruise_v;

    if (std::abs(cruise_v - requested_v) < 0.01f) {
        return MoveKinematics::LimitingFactor::Requested;
    }

    // Check if SCV limited the junction
    if (block.scv_limited) {
        return MoveKinematics::LimitingFactor::SCV;
    }

    // Check if cruise ratio limited the peak
    float max_cruise_v = std::sqrt(block.max_cruise_v2);
    if (cruise_v < max_cruise_v * 0.99f) {
        // Could be cruise ratio or lookahead
        if (block.lookahead_limited) {
            return MoveKinematics::LimitingFactor::Lookahead;
        }
        return MoveKinematics::LimitingFactor::CruiseRatio;
    }

    return MoveKinematics::LimitingFactor::Acceleration;
}
```

### 5.5 Result Consistency

Ensure the time reported in various places is consistent:

```cpp
// Total time from all machines
float GCodeProcessor::get_total_time() const {
    float total = 0.0f;
    for (const auto& machine : m_time_processor.machines) {
        if (machine.enabled) {
            total = std::max(total, machine.time);
        }
    }
    return total;
}

// Per-layer times for preview
std::vector<float> GCodeProcessor::get_layer_times(size_t machine_index) const {
    return m_time_processor.machines[machine_index].layers_time;
}

// Per-feature times for statistics
std::map<ExtrusionRole, float> GCodeProcessor::get_role_times(size_t machine_index) const {
    std::map<ExtrusionRole, float> result;
    const auto& machine = m_time_processor.machines[machine_index];
    for (size_t i = 0; i < machine.roles_time.size(); ++i) {
        if (machine.roles_time[i] > 0.0f) {
            result[static_cast<ExtrusionRole>(i)] = machine.roles_time[i];
        }
    }
    return result;
}
```

---

## Validation Testing

### 5.6 Test Harness Setup

Create a test harness that compares OrcaSlicer estimates with klipper_estimator:

```bash
#!/bin/bash
# validation_test.sh

# Generate G-code with OrcaSlicer
orca_slicer --export-gcode --load profile.ini model.3mf -o test.gcode

# Run OrcaSlicer's internal time estimation
orca_time=$(grep "estimated printing time" test.gcode | ...)

# Run klipper_estimator
klipper_time=$(klipper_estimator --config printer.cfg test.gcode | ...)

# Compare
echo "OrcaSlicer: $orca_time"
echo "Klipper:    $klipper_time"
echo "Difference: $(echo "scale=2; ($orca_time - $klipper_time) / $klipper_time * 100" | bc)%"
```

### 5.7 Test Cases

Create test G-code files covering various scenarios:

**test_simple_cube.gcode**
- Simple cube with minimal infill
- Expected accuracy: ±2%

**test_complex_infill.gcode**
- Object with dense zigzag infill
- Many direction changes
- Expected accuracy: ±5%

**test_spiral_vase.gcode**
- Continuous spiral with no retracts
- Tests constant motion scenarios
- Expected accuracy: ±2%

**test_travel_heavy.gcode**
- Many separate islands requiring travels
- Tests travel move estimation
- Expected accuracy: ±5%

**test_layer_changes.gcode**
- Tall thin object with many layer changes
- Tests Z-axis limiting
- Expected accuracy: ±3%

**test_variable_speed.gcode**
- G-code with many speed changes via F parameter
- SET_VELOCITY_LIMIT commands
- Expected accuracy: ±5%

### 5.8 Automated Testing

Add to the test suite:

```cpp
// tests/libslic3r/test_klipper_time_estimation.cpp

#include <catch2/catch.hpp>
#include "libslic3r/GCode/GCodeProcessor.hpp"

TEST_CASE("Klipper time estimation - simple moves", "[KlipperEstimator]") {
    GCodeProcessor processor;
    processor.set_gcode_flavor(gcfKlipper);

    // Test G-code
    std::string gcode = R"(
        G28
        G1 X100 Y100 F6000
        G1 X0 Y0
    )";

    processor.process_buffer(gcode);

    float time = processor.get_total_time();

    // Expected: ~2 seconds for 200mm travel at ~100mm/s with accel
    REQUIRE(time > 1.5f);
    REQUIRE(time < 3.0f);
}

TEST_CASE("Klipper time estimation - junction velocity", "[KlipperEstimator]") {
    GCodeProcessor processor;
    processor.set_gcode_flavor(gcfKlipper);

    // 90° corner
    std::string gcode = R"(
        G1 X100 F6000
        G1 Y100
    )";

    processor.process_buffer(gcode);

    // Get junction velocity from first move's end
    auto moves = processor.get_moves();
    REQUIRE(moves.size() >= 2);

    float junction_v = moves[0].kinematics.exit_speed;

    // Junction velocity should be reduced for 90° corner
    // Jerk (used as SCV) of 5mm/s at 90° → ~7 mm/s junction
    REQUIRE(junction_v < 50.0f);  // Much less than cruise
    REQUIRE(junction_v > 1.0f);   // But not zero
}
```

---

## Performance Optimization

### 5.9 Profiling Points

Add timing instrumentation:

```cpp
#ifdef PROFILE_TIME_ESTIMATION
    auto start = std::chrono::high_resolution_clock::now();
#endif

    // ... processing code ...

#ifdef PROFILE_TIME_ESTIMATION
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    BOOST_LOG_TRIVIAL(info) << "Time estimation took " << duration.count() << "ms";
#endif
```

### 5.10 Optimization Strategies

If performance is insufficient:

1. **Reduce memory allocations**:
   - Pre-allocate vectors
   - Use object pools for TimeBlocks

2. **Simplify math**:
   - Use lookup tables for common angles
   - Approximate sqrt with fast inverse sqrt

3. **Parallelize**:
   - Process multiple machines in parallel
   - Batch junction calculations

4. **Cache results**:
   - Cache junction_deviation when SCV doesn't change
   - Cache feature accelerations

---

## Rollout Strategy

### 5.11 Feature Flag

Use a feature flag for gradual rollout:

```cpp
// In preferences/settings
bool use_klipper_time_estimation = true;  // Default on for Klipper printers

// In GCodeProcessor::apply_config
if (m_flavor == gcfKlipper && use_klipper_time_estimation) {
    for (auto& machine : m_time_processor.machines) {
        machine.estimator_mode = EstimatorMode::Klipper;
    }
}
```

### 5.12 Logging for Debugging

Add optional verbose logging:

```cpp
#ifdef KLIPPER_ESTIMATOR_VERBOSE
    BOOST_LOG_TRIVIAL(debug) << "Block " << block.g1_line_id
        << ": dist=" << block.distance
        << ", req_v=" << block.feedrate_profile.cruise
        << ", start_v=" << block.resolved_start_v
        << ", cruise_v=" << block.resolved_cruise_v
        << ", end_v=" << block.resolved_end_v
        << ", time=" << block_time;
#endif
```

---

## Deliverables Checklist

- [ ] Integration into process_G1()
- [ ] Integration into process_G2_G3()
- [ ] Final flush on G-code end
- [ ] Per-layer time accumulation
- [ ] Per-feature time accumulation
- [ ] Preview kinematics synchronization
- [ ] Result consistency across all interfaces
- [ ] Test harness for klipper_estimator comparison
- [ ] Automated unit tests
- [ ] Performance profiling
- [ ] Feature flag for rollout
- [ ] Verbose logging option
- [ ] Documentation updates

---

## Success Criteria

1. **Accuracy Target**: Print time estimates within ±5% of klipper_estimator for the test suite
2. **Performance Target**: Time estimation adds <100ms for a typical 1-hour print
3. **Regression Target**: Non-Klipper printers show no change in estimates
4. **Stability Target**: No crashes or hangs on any tested G-code

---

## Documentation Updates

Update the following documentation:

1. **User documentation**:
   - Explain that Klipper printers now use improved time estimation
   - List any new configuration options

2. **Developer documentation**:
   - Architecture overview of the new estimator
   - How to add new printer types
   - How to debug time estimation issues

3. **Changelog**:
   - Feature announcement
   - Breaking changes (if any)
