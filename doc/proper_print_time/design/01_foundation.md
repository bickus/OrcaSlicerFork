# Phase 1: Foundation

## Overview

This phase establishes the foundation for Klipper-compatible time estimation without disrupting existing functionality. The key principle is **feature-flagging**: all new code paths are guarded by an estimator mode flag, allowing gradual rollout and easy rollback.

**CRITICAL REQUIREMENT**: The existing time estimation code for Marlin, RepRapFirmware, and other non-Klipper printers must remain **completely unchanged**. This implementation is ADDITIVE - we are adding a new code path for Klipper, not modifying the existing one.

## Goals

1. Introduce `EstimatorMode` enum to toggle between legacy and Klipper algorithms
2. Expose/add configuration parameters needed for Klipper estimation
3. Create data structures for the new planner
4. **Ensure the Legacy code path is not modified - only renamed and wrapped**

---

## Work Items

### 1.1 Estimator Mode Configuration

**File**: `src/libslic3r/GCode/GCodeProcessor.hpp`

Add enum to control which algorithm is used:

```cpp
enum class EstimatorMode : uint8_t {
    Legacy = 0,      // Current Marlin-like algorithm
    Klipper = 1,     // Full Klipper-compatible algorithm
    KlipperSimple = 2 // Klipper algorithm without lookahead (for validation)
};
```

**File**: `src/libslic3r/GCode/GCodeProcessor.cpp`

In `TimeMachine`, add:

```cpp
struct TimeMachine {
    // Existing fields...

    EstimatorMode estimator_mode{EstimatorMode::Legacy};

    // New Klipper-specific state
    struct KlipperState {
        float junction_deviation{0.0f};      // Derived from SCV
        float accel_to_decel{50.0f};         // Derived from cruise ratio
        float instant_corner_velocity{1.0f}; // From extruder config
    } klipper_state;
};
```

**Selection Logic** (in `apply_config()`):

```cpp
// For Klipper flavor printers, use Klipper estimator mode
if (m_flavor == gcfKlipper) {
    for (size_t i = 0; i < m_time_processor.machines.size(); ++i) {
        m_time_processor.machines[i].estimator_mode = EstimatorMode::Klipper;
    }
}
```

**Validation**:
- Verify non-Klipper printers still use `EstimatorMode::Legacy`
- Verify switching printer flavor correctly updates estimator mode

---

### 1.2 Enhanced TimeBlock Structure

**File**: `src/libslic3r/GCode/GCodeProcessor.hpp`

Extend `TimeBlock` with fields needed for Klipper planning:

```cpp
struct TimeBlock {
    // === Existing fields (keep all) ===

    // === New fields for Klipper planning ===

    // Move rate vector (direction normalized to distance)
    Vec3f rate_xyz{Vec3f::Zero()};    // Direction in XYZ space
    float rate_e{0.0f};               // Extrusion rate (delta_e / distance)

    // Velocity constraints (squared to avoid sqrt)
    float max_cruise_v2{0.0f};        // Maximum cruise velocity²
    float max_dv2{0.0f};              // 2 * distance * acceleration
    float smoothed_dv2{0.0f};         // 2 * distance * accel_to_decel
    float max_start_v2{0.0f};         // Maximum start velocity² (from junction)
    float max_smoothed_v2{0.0f};      // Smoothed start velocity²

    // Junction deviation for this move
    float junction_deviation{0.0f};

    // Move classification
    bool is_kinematic{true};          // true if XYZ movement, false if E-only
    bool has_xy_motion{false};        // true if X or Y changes

    // Final resolved velocities (set by two-pass planner)
    float resolved_start_v{0.0f};     // mm/s
    float resolved_cruise_v{0.0f};    // mm/s
    float resolved_end_v{0.0f};       // mm/s

    // Methods
    void init_klipper_fields(float velocity, float accel, float accel_to_decel);
    bool is_extrude_only() const { return !is_kinematic && std::abs(rate_e) > 0.001f; }
};
```

**Implementation**:

```cpp
void TimeBlock::init_klipper_fields(float velocity, float accel, float accel_to_decel) {
    if (is_kinematic) {
        max_cruise_v2 = velocity * velocity;
        max_dv2 = 2.0f * distance * accel;
        smoothed_dv2 = 2.0f * distance * accel_to_decel;
    } else {
        // Extrude-only moves don't propagate velocity constraints
        max_cruise_v2 = velocity * velocity;
        max_dv2 = std::numeric_limits<float>::infinity();
        smoothed_dv2 = std::numeric_limits<float>::infinity();
    }
    max_start_v2 = 0.0f;
    max_smoothed_v2 = 0.0f;
}
```

---

### 1.3 Derived Klipper Parameters

Instead of adding new printer profile options, derive all needed values from existing parameters.

**Junction Deviation** (derived from SCV):

```cpp
// In TimeMachine or as a helper function
float compute_junction_deviation(float scv, float max_accel) {
    // junction_deviation = scv² × (√2 - 1) / max_acceleration
    static constexpr float SQRT2_MINUS_1 = 0.41421356f;
    return (scv * scv * SQRT2_MINUS_1) / max_accel;
}
```

**Accel-to-Decel** (derived from cruise ratio):

```cpp
float compute_accel_to_decel(float max_accel, float cruise_ratio) {
    // accel_to_decel = max_acceleration × (1.0 - cruise_ratio)
    float ratio = std::clamp(cruise_ratio, 0.0f, 0.99f);
    return max_accel * (1.0f - ratio);
}
```

**Instant Corner Velocity** (use extruder jerk):

```cpp
// Use machine_max_jerk_e as instant_corner_velocity
float instant_corner_velocity = machine_limits.machine_max_jerk_e.values[mode];
```

**Location**: Add these calculations in `GCodeProcessor::apply_config()`:

```cpp
if (m_flavor == gcfKlipper) {
    for (size_t i = 0; i < m_time_processor.machines.size(); ++i) {
        auto& machine = m_time_processor.machines[i];
        auto& limits = m_time_processor.machine_limits;

        // Get SCV from jerk setting (Klipper uses jerk field for SCV)
        float scv = std::min(
            limits.machine_max_jerk_x.values[i],
            limits.machine_max_jerk_y.values[i]
        );

        // Get max acceleration (use extruding accel as default)
        float max_accel = limits.machine_max_acceleration_extruding.values[i];

        // Derive Klipper parameters
        machine.klipper_state.junction_deviation = compute_junction_deviation(scv, max_accel);
        machine.klipper_state.accel_to_decel = compute_accel_to_decel(
            max_accel, machine.minimum_cruise_ratio
        );
        machine.klipper_state.instant_corner_velocity = limits.machine_max_jerk_e.values[i];
    }
}
```

---

### 1.4 Feature-Flag Guard Pattern (CRITICAL)

All new code paths should be guarded to ensure legacy behavior is preserved. **The existing code is NOT modified - only renamed and wrapped.**

```cpp
// STEP 1: Rename the existing calculate_time() to calculate_time_legacy()
// This is a RENAME ONLY - the implementation is NOT changed
void TimeMachine::calculate_time_legacy() {
    // ============================================================
    // THIS IS THE EXISTING CODE - COPIED VERBATIM, NOT MODIFIED
    // All existing logic for Marlin, RRF, etc. remains here
    // ============================================================

    // ... existing forward pass ...
    // ... existing reverse pass ...
    // ... existing trapezoid calculation ...
    // ... existing time accumulation ...
}

// STEP 2: Create new wrapper that dispatches based on mode
void TimeMachine::calculate_time() {
    if (estimator_mode == EstimatorMode::Klipper) {
        calculate_time_klipper();  // NEW code for Klipper only
    } else {
        calculate_time_legacy();   // EXISTING code for all other firmware
    }
}

// STEP 3: New method for Klipper calculation (implemented in Phase 3)
void TimeMachine::calculate_time_klipper() {
    // NEW implementation for Klipper printers only
    // This is completely separate from calculate_time_legacy()
}
```

**Key Points:**
1. `calculate_time_legacy()` IS the existing `calculate_time()` - just renamed
2. The implementation of `calculate_time_legacy()` is NOT changed in any way
3. `calculate_time_klipper()` is entirely NEW code
4. Non-Klipper printers never execute any Klipper-specific code

---

### 1.5 Move Rate Vector Calculation

When creating a TimeBlock, calculate the rate vector needed for junction calculations:

**File**: `GCodeProcessor.cpp` in `process_G1()` around line 2950

Add after distance calculation:

```cpp
// Calculate rate vector for Klipper planning
if (machine.estimator_mode != EstimatorMode::Legacy) {
    if (block.distance > 0.0f) {
        Vec3d delta_xyz = m_end_position.head<3>() - m_start_position.head<3>();
        block.rate_xyz = (delta_xyz / block.distance).cast<float>();
        block.rate_e = static_cast<float>((m_end_position.e() - m_start_position.e()) / block.distance);
        block.is_kinematic = delta_xyz.squaredNorm() > 1e-10;
        block.has_xy_motion = std::abs(delta_xyz.x()) > 1e-6 || std::abs(delta_xyz.y()) > 1e-6;
    } else {
        block.rate_xyz = Vec3f::Zero();
        block.rate_e = 0.0f;
        block.is_kinematic = false;
        block.has_xy_motion = false;
    }
}
```

---

### 1.6 Configuration Parsing Updates

Ensure all Klipper G-code commands update the correct state:

**SET_VELOCITY_LIMIT Updates** (already parsed, but ensure klipper_state is updated):

```cpp
void GCodeProcessor::process_SET_VELOCITY_LIMIT(const GCodeReader::GCodeLine& line) {
    // Existing parsing code...

    // After parsing SCV, update junction_deviation
    if (scv_parsed) {
        for (size_t i = 0; i < m_time_processor.machines.size(); ++i) {
            auto& machine = m_time_processor.machines[i];
            if (machine.estimator_mode != EstimatorMode::Legacy) {
                float max_accel = m_time_processor.machine_limits.machine_max_acceleration_extruding.values[i];
                machine.klipper_state.junction_deviation = compute_junction_deviation(scv, max_accel);
            }
        }
    }

    // After parsing cruise ratio, update accel_to_decel
    if (cruise_ratio_parsed) {
        for (size_t i = 0; i < m_time_processor.machines.size(); ++i) {
            auto& machine = m_time_processor.machines[i];
            if (machine.estimator_mode != EstimatorMode::Legacy) {
                float max_accel = m_time_processor.machine_limits.machine_max_acceleration_extruding.values[i];
                machine.klipper_state.accel_to_decel = compute_accel_to_decel(max_accel, cruise_ratio);
            }
        }
    }
}
```

---

## Testing & Validation

### Unit Tests

1. **Junction deviation calculation**:
   - Input: SCV=5 mm/s, accel=1000 mm/s² → Expected: ~0.0104 mm
   - Input: SCV=10 mm/s, accel=2000 mm/s² → Expected: ~0.0207 mm

2. **Accel-to-decel calculation**:
   - Input: accel=1000, ratio=0.5 → Expected: 500 mm/s²
   - Input: accel=2000, ratio=0.0 → Expected: 2000 mm/s²

3. **Rate vector calculation**:
   - Move (0,0,0) → (10,0,0): rate_xyz = (1,0,0), is_kinematic=true
   - Move (0,0,0) → (0,0,0) E+5: rate_xyz = (0,0,0), is_kinematic=false

### Integration Tests

1. Load a Klipper printer profile → Verify `estimator_mode == Klipper`
2. Load a Marlin printer profile → Verify `estimator_mode == Legacy`
3. Parse G-code with `SET_VELOCITY_LIMIT SCV=10` → Verify junction_deviation updated
4. Process G-code → Verify time estimate unchanged (feature-flagged to legacy)

---

## Deliverables Checklist

- [ ] `EstimatorMode` enum added
- [ ] `TimeMachine::klipper_state` structure added
- [ ] `TimeBlock` extended with Klipper fields
- [ ] Derived parameter calculations implemented
- [ ] Feature-flag guards in place
- [ ] Rate vector calculation in process_G1/G2_G3
- [ ] SET_VELOCITY_LIMIT updates klipper_state
- [ ] Unit tests for derived parameters
- [ ] No behavioral changes in legacy mode
