# Deliverable 6: Integration and Consistency

## Task Overview

You are integrating all Klipper time estimation components and ensuring consistency across the application. This is deliverable 6 of 8. Your work connects the time calculation to layer times, feature times, and the preview visualization.

**Branch**: `claude/fix-print-time-estimation-tmz9u`

## Prerequisites

Before starting, read:
1. **Previous agents' work**:
   - `doc/proper_print_time/design/agent_prompts/completed/01_foundation_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/02_rate_vector_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/03_junction_velocity_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/04_two_pass_planning_complete.md`
   - `doc/proper_print_time/design/agent_prompts/completed/05_move_checkers_complete.md`
2. **Design documents**:
   - `doc/proper_print_time/design/05_integration.md` - **Primary reference**

## Important Constraints

1. **NO COMPILATION/EXECUTION**: All validation through static code analysis only.
2. **NO TEST WRITING**: Use validation criteria for self-checking.
3. **PRESERVE LEGACY BEHAVIOR**: Only modify Klipper code paths.
4. **DOCUMENTATION REQUIRED**: Create completion report when done.

## Background

The integration work ensures that:
1. **Total print time** is correctly accumulated from all blocks
2. **Per-layer times** are correctly attributed to each layer
3. **Per-feature times** (infill, perimeter, travel, etc.) are correctly tracked
4. **Preview visualization** shows correct velocities
5. **Block flushing** happens at appropriate points (finalize, layer changes)

## Files to Modify

1. **`src/libslic3r/GCode/GCodeProcessor.cpp`**
   - Finalize function updates
   - Layer time accumulation
   - Feature time accumulation
   - Preview kinematics sync

2. **Possibly** other files for preview integration

## Specific Tasks

### Task 1: Update finalize() for Klipper

Ensure all blocks are processed when G-code processing completes:

```cpp
void GCodeProcessor::finalize() {
    // ... existing finalization code ...

    for (auto& machine : m_time_processor.machines) {
        if (machine.estimator_mode == EstimatorMode::Klipper) {
            // Force flush all remaining blocks
            flush_klipper_blocks_final(machine);
        }
    }

    // ... rest of existing finalization ...
}

// Helper to flush all blocks at end
void GCodeProcessor::flush_klipper_blocks_final(TimeMachine& machine) {
    if (machine.blocks.empty()) return;

    // Run the two-pass planner on all remaining blocks
    std::vector<DelayedMove> delayed;
    klipper_backward_pass(machine.blocks, delayed);
    klipper_forward_pass(machine.blocks);

    // Calculate time for each block and accumulate
    for (auto& block : machine.blocks) {
        float block_time = calculate_klipper_block_time(block);
        block.time = block_time;

        // Accumulate into layer and feature times
        accumulate_block_time(machine, block);
    }

    machine.blocks.clear();  // Or move to processed storage
}
```

### Task 2: Implement Per-Layer Time Accumulation

Track time for each layer:

```cpp
namespace {
    void accumulate_layer_time(
        GCodeProcessor::TimeMachine& machine,
        const GCodeProcessor::TimeBlock& block,
        float block_time)
    {
        // Get current layer ID from the block or machine state
        int layer_id = block.layer_id;  // Or however layer is tracked

        // Handle edge cases
        if (layer_id < 0) {
            // Pre-print moves (e.g., initial positioning)
            // Add to a special "setup" category or layer 0
            layer_id = 0;
        }

        // Ensure layer times vector is large enough
        if (layer_id >= static_cast<int>(machine.layer_times.size())) {
            machine.layer_times.resize(layer_id + 1, 0.0f);
        }

        // Accumulate time
        machine.layer_times[layer_id] += block_time;
    }
}
```

### Task 3: Implement Per-Feature Time Accumulation

Track time for each extrusion role/feature:

```cpp
namespace {
    void accumulate_feature_time(
        GCodeProcessor::TimeMachine& machine,
        const GCodeProcessor::TimeBlock& block,
        float block_time)
    {
        // Get the extrusion role from the block
        ExtrusionRole role = block.role;

        // Map role to feature category
        // OrcaSlicer likely has existing feature time tracking - use that structure
        // This might be a map or fixed-size array

        // Example using array indexed by role:
        size_t role_index = static_cast<size_t>(role);
        if (role_index < machine.feature_times.size()) {
            machine.feature_times[role_index] += block_time;
        }

        // Also track move types if needed
        if (block.move_type == MoveType::Travel) {
            machine.travel_time += block_time;
        } else if (block.move_type == MoveType::Extrude) {
            machine.extrude_time += block_time;
        } else if (block.move_type == MoveType::Retract) {
            machine.retract_time += block_time;
        }
    }
}
```

### Task 4: Combined Block Time Accumulation

Create a unified function that handles all accumulation:

```cpp
void GCodeProcessor::accumulate_block_time(
    TimeMachine& machine,
    const TimeBlock& block)
{
    float block_time = block.time;

    // Accumulate total time
    machine.total_time += block_time;

    // Accumulate layer time
    accumulate_layer_time(machine, block, block_time);

    // Accumulate feature time
    accumulate_feature_time(machine, block, block_time);
}
```

### Task 5: Synchronize Preview Kinematics

Update the preview to use resolved velocities from Klipper planning:

```cpp
void GCodeProcessor::record_block_kinematics(
    const TimeBlock& block,
    KinematicsRecord& record)
{
    // Use resolved velocities for Klipper mode
    if (/* machine is in Klipper mode */) {
        // Store actual achieved velocities for visualization
        record.start_velocity = block.klipper.resolved_start_v;
        record.cruise_velocity = block.klipper.resolved_cruise_v;
        record.end_velocity = block.klipper.resolved_end_v;

        // Store acceleration phase info if needed
        record.acceleration = block.acceleration;
    } else {
        // Legacy mode - use existing logic
        // ... existing code ...
    }
}
```

### Task 6: Handle Layer Change Events

Flush blocks when layer changes to get accurate per-layer times:

```cpp
void GCodeProcessor::process_layer_change(int new_layer_id) {
    // ... existing layer change handling ...

    for (auto& machine : m_time_processor.machines) {
        if (machine.estimator_mode == EstimatorMode::Klipper) {
            // Flush blocks for previous layer
            // This ensures per-layer times are accurate
            flush_klipper_blocks(machine, /*force=*/true);
        }
    }

    // ... rest of layer change handling ...
}
```

### Task 7: Verify Time Consistency

Add consistency checks (for debug/validation):

```cpp
namespace {
    // Verify that times are internally consistent
    void verify_time_consistency(const GCodeProcessor::TimeMachine& machine) {
        // Sum of layer times should approximately equal total time
        float layer_sum = 0.0f;
        for (float t : machine.layer_times) {
            layer_sum += t;
        }

        // Allow 1% tolerance for rounding
        float tolerance = machine.total_time * 0.01f;
        bool layers_match = std::abs(layer_sum - machine.total_time) < tolerance;

        // Sum of feature times should approximately equal total time
        float feature_sum = 0.0f;
        for (float t : machine.feature_times) {
            feature_sum += t;
        }
        bool features_match = std::abs(feature_sum - machine.total_time) < tolerance;

        // In debug builds, assert on inconsistency
        // In release, log warning
        #ifdef DEBUG
        assert(layers_match && "Layer times don't sum to total");
        assert(features_match && "Feature times don't sum to total");
        #endif
    }
}
```

### Task 8: Update Result Output

Ensure the final results include Klipper times correctly:

```cpp
// In the function that outputs/returns print time results:
void GCodeProcessor::get_time_results(TimeResults& results) {
    for (const auto& machine : m_time_processor.machines) {
        // Use the mode-appropriate total time
        results.total_time = machine.total_time;
        results.layer_times = machine.layer_times;

        // Copy feature times
        for (size_t i = 0; i < machine.feature_times.size(); ++i) {
            results.feature_times[i] = machine.feature_times[i];
        }

        // Include mode information for debugging/display
        results.estimation_mode = (machine.estimator_mode == EstimatorMode::Klipper)
            ? "Klipper" : "Legacy";
    }
}
```

## Self-Validation Checklist

After completing your implementation, verify:

**Finalization**:
- [ ] **finalize() flushes all Klipper blocks**
- [ ] **All blocks processed** before returning
- [ ] **No blocks left in buffer** after finalization

**Layer Time Accumulation**:
- [ ] **Layer times vector resized** as needed
- [ ] **Negative layer IDs handled** (mapped to 0 or setup)
- [ ] **Each block contributes** to exactly one layer
- [ ] **Sum of layer times ≈ total time**

**Feature Time Accumulation**:
- [ ] **All extrusion roles mapped** to feature times
- [ ] **Travel, extrude, retract** separately tracked
- [ ] **Sum of feature times ≈ total time**

**Preview Integration**:
- [ ] **Resolved velocities used** for visualization
- [ ] **Start, cruise, end velocities** all available
- [ ] **Legacy mode unchanged**

**Layer Change Handling**:
- [ ] **Blocks flushed on layer change** (optional optimization)
- [ ] **Per-layer times accurate**

**Consistency**:
- [ ] **Verification function implemented**
- [ ] **No double-counting** of time
- [ ] **No missing time** (all blocks accounted)

**Output**:
- [ ] **Results include mode indicator**
- [ ] **All time data accessible**
- [ ] **Legacy results unchanged**

## Consistency Validation Scenarios

1. **Simple single-layer print**:
   - Layer 0 time = total time
   - Feature times sum to total

2. **Multi-layer print (e.g., 10 layers)**:
   - Sum of 10 layer times = total time
   - Each layer has non-zero time

3. **Print with retractions**:
   - Retract time > 0
   - Retract + travel + extrude ≈ total

4. **Print with multiple features**:
   - Perimeter + infill + travel ≈ total
   - Each feature has expected proportion

## Documentation Requirements

Create completion report at:
`doc/proper_print_time/design/agent_prompts/completed/06_integration_complete.md`

Include:
1. **Summary**: Integration points completed
2. **Files Modified**: With line numbers
3. **Data Flow**: How time flows from blocks to totals
4. **Existing Structures Used**: What OrcaSlicer structures were leveraged
5. **Design Decisions**: Choices made during implementation
6. **Deviations**: Differences from design
7. **Known Issues**: Concerns for Deliverable 7
8. **Self-Validation Results**: Completed checklist
9. **Consistency Check Results**: From validation scenarios

## Commit Requirements

Commit with message: "Deliverable 6: Integrate Klipper time estimation with layers, features, and preview"

Push to branch `claude/fix-print-time-estimation-tmz9u`

## What NOT to Do

- DO NOT modify the core velocity planning algorithm (Deliverable 4)
- DO NOT modify move checkers (Deliverable 5)
- DO NOT modify Legacy time calculation
- DO NOT create test files
- DO NOT try to compile or run the code
- DO NOT change how results are displayed in UI (that's separate work)
