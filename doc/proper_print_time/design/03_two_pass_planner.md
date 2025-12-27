# Phase 3: Two-Pass Velocity Planning

## Overview

This phase implements Klipper's two-pass velocity planning algorithm. This algorithm resolves conflicts between the maximum junction velocities (from Phase 2) and what is actually achievable given the acceleration constraints and move distances.

## Goals

1. Implement backward pass to propagate velocity constraints
2. Implement forward pass to resolve delayed moves
3. Handle velocity smoothing (accel_to_decel)
4. Calculate final `start_v`, `cruise_v`, `end_v` for each move

---

## Algorithm Overview

The two-pass algorithm works as follows:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         BACKWARD PASS                                │
│  Process moves from LAST to FIRST                                   │
│                                                                      │
│  For each move:                                                      │
│    1. Can we reach next move's end_v from our max_start_v?         │
│    2. Calculate reachable start velocity from next move's end      │
│    3. Determine if this is a "peak" (smoothed constraint binds)    │
│    4. Either set velocities now, or delay for forward pass         │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         FORWARD PASS                                 │
│  Process DELAYED moves from FIRST to LAST                           │
│                                                                      │
│  For each delayed move:                                              │
│    1. Calculate actual cruise velocity based on entry constraints   │
│    2. Set final start_v, cruise_v, end_v                            │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Work Items

### 3.1 Backward Pass Implementation

The backward pass propagates velocity constraints from the end of the move sequence backward:

```cpp
struct DelayedMove {
    size_t index;          // Index in blocks vector
    float start_v2;        // Calculated start velocity²
    float end_v2;          // Required end velocity²
};

void klipper_backward_pass(
    std::vector<TimeBlock>& blocks,
    std::vector<DelayedMove>& delayed
) {
    if (blocks.empty()) return;

    // Last move must end at zero velocity
    float next_end_v2 = 0.0f;
    float next_smoothed_v2 = 0.0f;
    float peak_cruise_v2 = 0.0f;

    for (int i = static_cast<int>(blocks.size()) - 1; i >= 0; --i) {
        TimeBlock& block = blocks[i];

        // Calculate reachable start velocity from next move's end
        float reachable_start_v2 = next_end_v2 + block.max_dv2;
        float start_v2 = std::min(block.max_start_v2, reachable_start_v2);

        // Calculate smoothed velocity constraint
        float reachable_smoothed_v2 = next_smoothed_v2 + block.smoothed_dv2;
        float smoothed_v2 = std::min(block.max_smoothed_v2, reachable_smoothed_v2);

        // Check if this move is a "peak" (smoothed constraint is binding)
        if (smoothed_v2 < reachable_smoothed_v2) {
            // This move constrains the peak velocity

            // Check if we need to process delayed moves
            if (smoothed_v2 + block.smoothed_dv2 > next_smoothed_v2 || !delayed.empty()) {
                peak_cruise_v2 = std::min(
                    block.max_cruise_v2,
                    (smoothed_v2 + reachable_smoothed_v2) * 0.5f
                );

                // Process delayed moves in forward order
                if (!delayed.empty()) {
                    float mc_v2 = peak_cruise_v2;
                    for (auto it = delayed.rbegin(); it != delayed.rend(); ++it) {
                        TimeBlock& delayed_block = blocks[it->index];
                        mc_v2 = std::min(mc_v2, it->start_v2);
                        delayed_block.resolved_start_v = std::sqrt(std::min(it->start_v2, mc_v2));
                        delayed_block.resolved_cruise_v = std::sqrt(mc_v2);
                        delayed_block.resolved_end_v = std::sqrt(std::min(it->end_v2, mc_v2));
                    }
                    delayed.clear();
                }
            }

            // Calculate final velocities for this move
            float cruise_v2 = std::min({
                (start_v2 + reachable_start_v2) * 0.5f,
                block.max_cruise_v2,
                peak_cruise_v2
            });

            block.resolved_start_v = std::sqrt(std::min(start_v2, cruise_v2));
            block.resolved_cruise_v = std::sqrt(cruise_v2);
            block.resolved_end_v = std::sqrt(std::min(next_end_v2, cruise_v2));
        } else {
            // Delay this move for forward pass processing
            delayed.push_back({static_cast<size_t>(i), start_v2, next_end_v2});
        }

        // Propagate for next iteration (previous move)
        next_end_v2 = start_v2;
        next_smoothed_v2 = smoothed_v2;
    }
}
```

### 3.2 Forward Pass (Delayed Move Resolution)

After the backward pass, some moves may still be delayed. Process them in forward order:

```cpp
void klipper_forward_pass(
    std::vector<TimeBlock>& blocks,
    std::vector<DelayedMove>& delayed,
    float peak_cruise_v2
) {
    // Process remaining delayed moves
    float mc_v2 = peak_cruise_v2;

    for (auto it = delayed.rbegin(); it != delayed.rend(); ++it) {
        TimeBlock& block = blocks[it->index];
        mc_v2 = std::min(mc_v2, it->start_v2);
        block.resolved_start_v = std::sqrt(std::min(it->start_v2, mc_v2));
        block.resolved_cruise_v = std::sqrt(mc_v2);
        block.resolved_end_v = std::sqrt(std::min(it->end_v2, mc_v2));
    }
}
```

### 3.3 Trapezoid Calculation with Resolved Velocities

After velocities are resolved, calculate the trapezoid profile:

```cpp
void calculate_klipper_trapezoid(TimeBlock& block) {
    float start_v = block.resolved_start_v;
    float cruise_v = block.resolved_cruise_v;
    float end_v = block.resolved_end_v;
    float accel = block.acceleration;

    // Acceleration distance
    float accel_d = (cruise_v * cruise_v - start_v * start_v) / (2.0f * accel);

    // Deceleration distance
    float decel_d = (cruise_v * cruise_v - end_v * end_v) / (2.0f * accel);

    // Cruise distance (remainder)
    float cruise_d = std::max(0.0f, block.distance - accel_d - decel_d);

    // Handle triangular profile (no cruise phase)
    if (cruise_d < 0.001f) {
        // Recalculate cruise_v for triangular profile
        // v² = (start_v² + end_v²) / 2 + accel * distance
        cruise_v = std::sqrt(
            (start_v * start_v + end_v * end_v) / 2.0f +
            accel * block.distance
        );

        // Recalculate distances
        accel_d = (cruise_v * cruise_v - start_v * start_v) / (2.0f * accel);
        decel_d = block.distance - accel_d;
        cruise_d = 0.0f;

        block.resolved_cruise_v = cruise_v;
    }

    // Store in trapezoid structure
    block.trapezoid.accelerate_until = accel_d;
    block.trapezoid.decelerate_after = accel_d + cruise_d;
    block.trapezoid.cruise_feedrate = cruise_v;
}
```

### 3.4 Time Calculation

Calculate the time for each phase:

```cpp
float calculate_klipper_block_time(const TimeBlock& block) {
    float start_v = block.resolved_start_v;
    float cruise_v = block.resolved_cruise_v;
    float end_v = block.resolved_end_v;

    float accel_d = block.trapezoid.accelerate_until;
    float cruise_d = block.trapezoid.decelerate_after - block.trapezoid.accelerate_until;
    float decel_d = block.distance - block.trapezoid.decelerate_after;

    // Acceleration time: t = d / ((v1 + v2) / 2)
    float accel_time = (accel_d > 0.0001f) ?
        accel_d / ((start_v + cruise_v) / 2.0f) : 0.0f;

    // Cruise time: t = d / v
    float cruise_time = (cruise_d > 0.0001f) ?
        cruise_d / cruise_v : 0.0f;

    // Deceleration time: t = d / ((v1 + v2) / 2)
    float decel_time = (decel_d > 0.0001f) ?
        decel_d / ((cruise_v + end_v) / 2.0f) : 0.0f;

    return accel_time + cruise_time + decel_time;
}
```

### 3.5 Main Klipper Time Calculation Entry Point

```cpp
void TimeMachine::calculate_time_klipper() {
    if (blocks.empty()) return;

    std::vector<DelayedMove> delayed;

    // Initialize Klipper fields if not already done
    for (auto& block : blocks) {
        if (block.max_cruise_v2 < 0.001f) {
            float accel = block.acceleration;
            block.max_cruise_v2 = block.feedrate_profile.cruise * block.feedrate_profile.cruise;
            block.max_dv2 = 2.0f * block.distance * accel;
            block.smoothed_dv2 = 2.0f * block.distance * klipper_state.accel_to_decel;
        }
    }

    // Run backward pass
    klipper_backward_pass(blocks, delayed);

    // If there are still delayed moves, process them
    if (!delayed.empty()) {
        // Use the last processed block's cruise as peak
        float peak_v2 = blocks.back().resolved_cruise_v * blocks.back().resolved_cruise_v;
        klipper_forward_pass(blocks, delayed, peak_v2);
    }

    // Calculate trapezoids and accumulate time
    for (auto& block : blocks) {
        calculate_klipper_trapezoid(block);
        float block_time = calculate_klipper_block_time(block);

        // Accumulate time (same as legacy)
        time += block_time;
        gcode_time.gcode_ids.push_back(block.g1_line_id);
        gcode_time.times.push_back(time);

        // Accumulate by move type
        moves_time[static_cast<size_t>(block.move_type)] += block_time;

        // Accumulate by role (if applicable)
        // ... existing role accumulation code ...
    }
}
```

### 3.6 Incremental Processing (Batching)

To handle large G-code files, process blocks in batches:

```cpp
static constexpr size_t KLIPPER_BATCH_SIZE = 64;   // Process this many blocks
static constexpr size_t KLIPPER_BUFFER_SIZE = 256; // Trigger when buffer reaches this

void TimeMachine::flush_klipper_blocks(bool force_all) {
    if (blocks.empty()) return;

    // Don't process if buffer isn't full yet
    if (!force_all && blocks.size() < KLIPPER_BUFFER_SIZE) return;

    // Process oldest blocks, keeping some for lookahead context
    size_t process_count = force_all ? blocks.size() :
        std::min(blocks.size() - KLIPPER_BATCH_SIZE, KLIPPER_BATCH_SIZE);

    if (process_count == 0) return;

    // Run velocity planning on the batch
    std::vector<TimeBlock> batch(blocks.begin(), blocks.begin() + process_count);
    std::vector<DelayedMove> delayed;

    // Adjust first block's max_start_v2 based on previous batch's end velocity
    if (!batch.empty()) {
        batch.front().max_start_v2 = std::min(
            batch.front().max_start_v2,
            prev_batch_end_v * prev_batch_end_v
        );
    }

    klipper_backward_pass(batch, delayed);
    if (!delayed.empty()) {
        klipper_forward_pass(batch, delayed, batch.back().max_cruise_v2);
    }

    // Calculate times and record
    for (auto& block : batch) {
        calculate_klipper_trapezoid(block);
        float block_time = calculate_klipper_block_time(block);
        time += block_time;
        // ... accumulation code ...
    }

    // Remember end velocity for next batch
    prev_batch_end_v = batch.back().resolved_end_v;

    // Remove processed blocks
    blocks.erase(blocks.begin(), blocks.begin() + process_count);
}
```

### 3.7 Edge Cases

**Empty move (zero distance)**:
```cpp
if (block.distance < 0.0001f) {
    block.resolved_start_v = 0.0f;
    block.resolved_cruise_v = 0.0f;
    block.resolved_end_v = 0.0f;
    // Block time is 0
    continue;
}
```

**Extrude-only moves**:
```cpp
if (block.is_extrude_only()) {
    // Extrude-only moves don't participate in lookahead
    // They always start and end at zero
    block.resolved_start_v = 0.0f;
    block.resolved_cruise_v = block.feedrate_profile.cruise;
    block.resolved_end_v = 0.0f;

    // Calculate trapezoid with full accel/decel
    // This is simpler than kinematic moves
}
```

**Very short moves**:
```cpp
// For very short moves, the cruise phase may not exist (triangular profile)
// This is handled in calculate_klipper_trapezoid() above
```

---

## Testing & Validation

### Unit Tests

1. **Single move (starts and ends at 0)**:
   - Input: 10mm move, 100mm/s max, 1000mm/s² accel
   - Expected: Triangular profile, no cruise

2. **Two co-linear moves**:
   - Both should reach max velocity at junction
   - Total time = single move at max velocity

3. **Two perpendicular moves**:
   - Junction velocity should be limited by junction deviation
   - Verify correct accel/decel phases

4. **Long sequence with varying speeds**:
   - Verify backward pass correctly propagates constraints
   - Verify no velocity exceeds max_cruise_v

### Integration Tests

1. **Square outline**:
   - Four 90° corners
   - Compare total time with klipper_estimator

2. **Zigzag infill pattern**:
   - Many sharp turns
   - Should see significant time difference from legacy

3. **Long straight line**:
   - Should reach and maintain cruise velocity
   - Time should match simple distance/velocity calculation

### Performance Tests

1. Process 100,000 blocks
2. Verify total time < 1 second
3. Memory usage stays bounded (batching works)

---

## Deliverables Checklist

- [ ] Backward pass implementation
- [ ] Forward pass implementation
- [ ] Trapezoid calculation with resolved velocities
- [ ] Time calculation per block
- [ ] Main calculate_time_klipper() method
- [ ] Incremental batching support
- [ ] Edge case handling (zero distance, E-only, short moves)
- [ ] Time accumulation (by layer, role, type)
- [ ] Unit tests
- [ ] Integration tests
- [ ] Performance validation

---

## Notes for Implementation

### Numerical Precision

- Use v² throughout to avoid accumulating sqrt errors
- Only take sqrt at the final step when storing resolved velocities
- Use epsilon comparisons (0.0001f) for near-zero checks

### Debugging

Add optional logging:
```cpp
#ifdef KLIPPER_PLANNER_DEBUG
    BOOST_LOG_TRIVIAL(debug) << "Block " << i
        << " start_v=" << block.resolved_start_v
        << " cruise_v=" << block.resolved_cruise_v
        << " end_v=" << block.resolved_end_v
        << " time=" << block_time;
#endif
```

### Memory Efficiency

- DelayedMove vector will typically be small (<100 elements)
- Use reserve() if profiling shows allocation overhead
- Consider using indices instead of pointers for cache efficiency
