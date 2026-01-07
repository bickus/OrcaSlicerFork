# Plate-Level Height Modifiers - Design Document

## Overview

This document describes the design and implementation of plate-level height modifiers for OrcaSlicer. This feature allows users to define configuration overrides for specific Z-height ranges that apply to all objects on a build plate.

## Motivation

Currently, height modifiers can only be applied to individual objects or instances. Users who want to change settings (like infill density, speed, or wall count) at specific heights must configure each object separately. Plate-level height modifiers provide a convenient way to apply such overrides globally to all objects on a plate.

**Use Cases:**
- Reduce infill density above a certain height for all objects
- Increase wall count near the bed for better adhesion across all parts
- Change speeds at specific layers for better quality in critical regions
- Apply consistent seam or surface quality settings at specific heights

## Requirements

1. **Same settings as object height modifiers**: All `PrintRegionConfig` settings currently available in object height modifiers
2. **Applies to all objects**: Plate height modifiers affect every object on the plate
3. **Merge behavior**: Non-conflicting settings from both plate and object modifiers apply; on conflict, object modifier wins (more specific)
4. **Extensible**: Design allows adding "plate-only" settings (temperature, retraction) in the future

## Architecture

### Current Object Height Modifier System

The existing system stores height modifiers in `ModelObject`:

```cpp
// src/libslic3r/Slicing.hpp
typedef std::pair<coordf_t, coordf_t> t_layer_height_range;
typedef std::map<t_layer_height_range, ModelConfig> t_layer_config_ranges;

// src/libslic3r/Model.hpp (ModelObject class)
t_layer_config_ranges layer_config_ranges;
```

During slicing, these are processed by:
1. `LayerRanges` class in `PrintApply.cpp` - converts ranges to non-overlapping sorted intervals
2. `generate_print_object_regions()` - creates PrintRegions with appropriate configs
3. `region_config_from_model_volume()` - applies layer_range_config to volumes

**Critical Implementation Detail**: The `LayerRanges` class stores **pointers** to `DynamicPrintConfig` objects owned by the input `t_layer_config_ranges` map:

```cpp
class LayerRanges {
    struct LayerRange {
        t_layer_height_range layer_height_range;
        const DynamicPrintConfig* config { nullptr };  // POINTER to external config
    };
};
```

The existing `LayerRanges::assign()` does NOT merge configs from overlapping ranges. Instead:
- Ranges are processed in lexicographical order (by z_min, then z_max)
- The **first range wins** - subsequent overlapping ranges are trimmed, not merged
- Gap ranges (with `config = nullptr`) are inserted between configured ranges

### Proposed Plate Height Modifier System

#### Storage

**Layer Separation Issue**: `PartPlate` is in the GUI layer (`src/slic3r/GUI/`), while `Print` is in the core engine (`src/libslic3r/`). The core engine cannot directly access GUI classes.

**Solution**: Use the existing data flow pattern:
1. Store ranges in `PartPlate` (GUI layer)
2. Transfer to `PlateData` struct (bridge layer in `Format/bbs_3mf.hpp`)
3. Pass to `Print` via `BackgroundSlicingProcess::apply()` mechanism

```cpp
// src/slic3r/GUI/PartPlate.hpp
class PartPlate {
private:
    t_layer_config_ranges m_layer_config_ranges;  // NEW
public:
    const t_layer_config_ranges& layer_config_ranges() const;
    t_layer_config_ranges& layer_config_ranges();
    void clear_layer_config_ranges();
    bool has_layer_config_ranges() const;
};

// src/libslic3r/Format/bbs_3mf.hpp - Add to PlateData
struct PlateData {
    // ... existing fields ...
    t_layer_config_ranges layer_config_ranges;  // NEW
};

// src/libslic3r/Print.hpp - Add storage in Print class
class Print {
private:
    t_layer_config_ranges m_plate_layer_config_ranges;  // NEW
public:
    void set_plate_layer_config_ranges(const t_layer_config_ranges& ranges);
    const t_layer_config_ranges& plate_layer_config_ranges() const;
};
```

#### Merge Logic

**Critical Design Issue**: The original approach of returning a temporary merged `t_layer_config_ranges` is problematic because `LayerRanges` stores **pointers** to configs. A temporary map would cause dangling pointers after the function returns.

**Solution**: Store the merged ranges in `ModelObject` (which has stable lifetime) OR use the alternative approach below that avoids merging at the `t_layer_config_ranges` level.

**Recommended Approach - Two-Phase Lookup**:

Instead of pre-merging ranges, modify `region_config_from_model_volume()` to accept both plate and object configs and merge at the point of use:

```cpp
// src/libslic3r/PrintApply.cpp

// Modified to accept plate-level layer config
PrintRegionConfig region_config_from_model_volume(
    const PrintRegionConfig &default_or_parent_region_config,
    const DynamicPrintConfig *plate_layer_config,    // NEW: from plate ranges
    const DynamicPrintConfig *object_layer_config,   // from object ranges
    const ModelVolume &volume,
    size_t num_extruders)
{
    PrintRegionConfig config = default_or_parent_region_config;

    // Apply plate-level layer config first (lower priority)
    if (plate_layer_config)
        config.apply(*plate_layer_config, true);

    // Apply object-level layer config second (higher priority, overwrites plate)
    if (object_layer_config)
        config.apply(*object_layer_config, true);

    // Apply volume-specific config last (highest priority)
    normalize_and_apply_config(config, volume.config.get(), num_extruders);

    return config;
}
```

**Alternative Approach - Persistent Merged Storage**:

If pre-merging is preferred, store the merged result in a stable location:

```cpp
// Store merged ranges in Print (stable lifetime during slicing)
class Print {
private:
    // Per-object merged layer configs (object_id -> merged ranges)
    std::map<ObjectID, t_layer_config_ranges> m_merged_layer_config_ranges;

public:
    // Called before slicing each object
    const t_layer_config_ranges& get_merged_layer_config_ranges(
        ObjectID object_id,
        const t_layer_config_ranges& plate_ranges,
        const t_layer_config_ranges& object_ranges);
};

// Implementation
const t_layer_config_ranges& Print::get_merged_layer_config_ranges(
    ObjectID object_id,
    const t_layer_config_ranges& plate_ranges,
    const t_layer_config_ranges& object_ranges)
{
    if (plate_ranges.empty())
        return object_ranges;  // Return reference to object's stable storage
    if (object_ranges.empty())
        return plate_ranges;   // Return reference to Print's stable storage

    // Merge and store persistently
    auto& merged = m_merged_layer_config_ranges[object_id];
    merged.clear();

    // Collect all unique Z boundaries
    std::set<coordf_t> boundaries;
    for (const auto& [range, config] : plate_ranges) {
        boundaries.insert(range.first);
        boundaries.insert(range.second);
    }
    for (const auto& [range, config] : object_ranges) {
        boundaries.insert(range.first);
        boundaries.insert(range.second);
    }

    // Create merged ranges
    std::vector<coordf_t> sorted_bounds(boundaries.begin(), boundaries.end());

    for (size_t i = 0; i + 1 < sorted_bounds.size(); ++i) {
        coordf_t z_min = sorted_bounds[i];
        coordf_t z_max = sorted_bounds[i + 1];
        if (z_max <= z_min + EPSILON) continue;

        t_layer_height_range new_range{z_min, z_max};
        DynamicPrintConfig merged_config;

        // Apply plate ranges first (lower priority)
        for (const auto& [range, config] : plate_ranges) {
            if (range.first < z_max && range.second > z_min) {
                merged_config.apply(config.get(), true);
            }
        }

        // Apply object ranges second (higher priority)
        for (const auto& [range, config] : object_ranges) {
            if (range.first < z_max && range.second > z_min) {
                merged_config.apply(config.get(), true);
            }
        }

        if (!merged_config.empty()) {
            merged[new_range].assign_config(merged_config);  // Note: no std::move
        }
    }

    return merged;  // Return reference to stable storage
}
```

**Note on ModelConfig::assign_config()**: The method signature is:
```cpp
void assign_config(const DynamicPrintConfig &rhs);  // Takes const ref, not rvalue
```
Using `std::move` has no effect here.

#### Integration Point

**Data Flow from GUI to Core**:

```cpp
// src/slic3r/GUI/BackgroundSlicingProcess.cpp
Print::ApplyStatus BackgroundSlicingProcess::apply(...)
{
    // Existing: merge plate config into new_config
    new_config.apply(*m_current_plate->config());

    // NEW: Pass plate layer config ranges to Print
    m_print->set_plate_layer_config_ranges(m_current_plate->layer_config_ranges());

    return m_print->apply(model, new_config);
}
```

**In `Print::apply()` (PrintApply.cpp line ~1645)**:

Using the **Recommended Two-Phase Approach**:
```cpp
// No changes to LayerRanges construction - keep using object ranges only
LayerRanges object_layer_ranges(print_object.model_object()->layer_config_ranges);

// Create plate LayerRanges separately
LayerRanges plate_layer_ranges(m_plate_layer_config_ranges);

// Modify generate_print_object_regions to accept both
print_object_regions = generate_print_object_regions(
    print_object_regions,
    print_object.model_object()->volumes,
    object_layer_ranges,
    plate_layer_ranges,  // NEW parameter
    m_default_region_config,
    // ... other params
);
```

Using the **Alternative Persistent Storage Approach**:
```cpp
// Get merged ranges with stable lifetime
const t_layer_config_ranges& merged = get_merged_layer_config_ranges(
    print_object.model_object()->id(),
    m_plate_layer_config_ranges,
    print_object.model_object()->layer_config_ranges);

LayerRanges(merged)  // Safe: merged has stable lifetime in Print
```

### Data Flow

```
GUI Layer                           Core Layer (libslic3r)
────────────────────────────────    ────────────────────────────────
┌─────────────────┐
│   PartPlate     │
│ layer_config_   │
│ ranges          │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────┐
│ BackgroundSlicingProcess    │
│ apply()                     │──────────────────┐
└─────────────────────────────┘                  │
                                                 ▼
                              ┌────────────────────────────────────┐
                              │ Print::set_plate_layer_config_     │
                              │ ranges()                           │
                              └────────────────┬───────────────────┘
                                               │
                                               ▼
┌──────────────────┐          ┌────────────────────────────────────┐
│   ModelObject    │          │    Print                           │
│ layer_config_    │──────────│    m_plate_layer_config_ranges     │
│ ranges           │          └────────────────┬───────────────────┘
└──────────────────┘                           │
         │                                     │
         │         ┌───────────────────────────┘
         │         │
         ▼         ▼
    ┌─────────────────────────────────────────────────────┐
    │ Option A: Two-Phase Lookup                          │
    │ region_config_from_model_volume() merges at use     │
    ├─────────────────────────────────────────────────────┤
    │ Option B: Persistent Merged Storage                 │
    │ get_merged_layer_config_ranges() → stable storage   │
    └───────────────────────────┬─────────────────────────┘
                                │
                                ▼
                  ┌───────────────────────┐
                  │ LayerRanges class     │
                  │ (non-overlapping)     │
                  └───────────┬───────────┘
                              │
                              ▼
                  ┌───────────────────────┐
                  │ generate_print_       │
                  │ object_regions()      │
                  └───────────┬───────────┘
                              │
                              ▼
                  ┌───────────────────────┐
                  │ PrintRegion configs   │
                  └───────────────────────┘
```

## UI Design

### Location

Add to `PlateSettingsDialog` as a new collapsible section.

### Components

1. **Height Range List**: Scrollable list showing all defined ranges
2. **Range Editor**: Min Z / Max Z input fields
3. **Settings Selector**: Button to add/edit settings for selected range
4. **Add/Delete Buttons**: Manage ranges

### Mockup

```
┌─────────────────────────────────────────────────────┐
│ Plate Settings                                   [X] │
├─────────────────────────────────────────────────────┤
│ Plate Name: [Plate 1                           ]    │
│                                                     │
│ Bed Type: [Engineering Plate (PEI)     ▼]           │
│ Print Sequence: [By Layer              ▼]           │
│                                                     │
│ ▼ Height Range Modifiers                            │
│ ┌─────────────────────────────────────────────────┐ │
│ │ [0.00] - [10.00] mm  [Settings...] [Delete]     │ │
│ │   └ sparse_infill_density: 10%                  │ │
│ │   └ wall_loops: 3                               │ │
│ │                                                 │ │
│ │ [15.00] - [25.00] mm  [Settings...] [Delete]    │ │
│ │   └ inner_wall_speed: 100 mm/s                  │ │
│ └─────────────────────────────────────────────────┘ │
│ [+ Add Range]                                       │
│                                                     │
│                          [Cancel]  [OK]             │
└─────────────────────────────────────────────────────┘
```

## File Format

### 3MF Extension

Add new file `Metadata/plate_layer_config_ranges.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<plates>
  <plate index="0">
    <range min_z="0.0" max_z="10.0">
      <option opt_key="sparse_infill_density">10%</option>
      <option opt_key="wall_loops">3</option>
    </range>
    <range min_z="15.0" max_z="25.0">
      <option opt_key="inner_wall_speed">100</option>
    </range>
  </plate>
  <plate index="1">
    <!-- ranges for plate 1 -->
  </plate>
</plates>
```

## Implementation Phases

### Phase 1: Core Data Structures
- Add `m_layer_config_ranges` to `PartPlate`
- Add `layer_config_ranges` to `PlateData` struct
- Add `m_plate_layer_config_ranges` to `Print` class
- Add `set_plate_layer_config_ranges()` and accessor in `Print`
- **Note**: `PartPlate` uses Cereal for internal serialization (see line 508 of PartPlate.hpp)

### Phase 2: Data Flow Integration
- Modify `BackgroundSlicingProcess::apply()` to transfer plate ranges to `Print`
- Choose implementation approach (Two-Phase Lookup vs Persistent Storage)
- Implement chosen approach in `PrintApply.cpp`
- Modify `generate_print_object_regions()` signature if using Two-Phase approach
- Modify `region_config_from_model_volume()` to accept plate config

### Phase 3: UI
- Create `PlateHeightRangesPanel` class
- Integrate into `PlateSettingsDialog`
- Wire up to `PartPlate`
- Add range validation (see Edge Cases)

### Phase 4: Serialization
- Add 3MF export function in `bbs_3mf.cpp`
- Add 3MF import function in `_BBS_3MF_Importer`
- Update PlateData population from PartPlate
- Handle backward compatibility (empty ranges for old files)

### Phase 5: Polish
- Trigger re-slice on change (invalidation logic in `Print::apply()`)
- Undo/redo support (leverage `ModelConfig` timestamp mechanism)
- Copy plate handling (deep copy `layer_config_ranges`)
- Plate delete handling (clean up merged cache if using Persistent Storage)

## Files to Modify

| File | Changes |
|------|---------|
| `src/slic3r/GUI/PartPlate.hpp` | Add member, accessors, Cereal |
| `src/slic3r/GUI/PartPlate.cpp` | Set plate ranges before slicing |
| `src/libslic3r/Print.hpp` | Add plate ranges storage |
| `src/libslic3r/PrintApply.cpp` | Merge logic, integration |
| `src/slic3r/GUI/PlateSettingsDialog.hpp` | Add panel member |
| `src/slic3r/GUI/PlateSettingsDialog.cpp` | Add UI section |
| `src/slic3r/GUI/GUI_PlateHeightRanges.hpp` | New file |
| `src/slic3r/GUI/GUI_PlateHeightRanges.cpp` | New file |
| `src/libslic3r/Format/bbs_3mf.hpp` | Add to PlateData |
| `src/libslic3r/Format/bbs_3mf.cpp` | Import/export |

## Edge Cases and Validation

### Range Validation

**Input validation required in UI**:
```cpp
bool validate_height_range(coordf_t min_z, coordf_t max_z, std::string& error_msg) {
    if (min_z < 0) {
        error_msg = "Minimum Z cannot be negative";
        return false;
    }
    if (max_z <= min_z) {
        error_msg = "Maximum Z must be greater than minimum Z";
        return false;
    }
    if (max_z - min_z < EPSILON) {
        error_msg = "Range too small (< " + std::to_string(EPSILON) + " mm)";
        return false;
    }
    return true;
}
```

### Print-By-Object Behavior

**Critical Decision Required**: When using "Print Sequence: By Object", each object is printed completely before moving to the next. The Z height resets to 0 for each object.

**Options**:
1. **Absolute Z (Recommended)**: Plate modifiers apply based on absolute Z from bed
   - Range [10, 20] affects layers at Z=10-20mm for ALL objects
   - Consistent with current object modifier behavior
   - User expectation: "reduce infill above 10mm from bed"

2. **Object-Relative Z**: Plate modifiers apply relative to each object's base
   - Range [10, 20] affects 10-20mm above each object's lowest point
   - More complex to implement (need per-object Z offset calculation)
   - User expectation: "reduce infill in upper portion of each object"

**Recommendation**: Use Absolute Z. Document this behavior clearly in UI.

### Edge Cases

1. **Range overlaps**: Plate [0-10] and object [5-15] overlap
   - Interval [5-10] gets both configs merged, object takes priority
   - Must handle floating-point comparison with EPSILON tolerance

2. **Empty ranges**: Return the non-empty source directly (no merge needed)

3. **Backward compatibility**: Old files load with empty plate ranges
   - Import code should handle missing `plate_layer_config_ranges.xml` gracefully

4. **Multi-plate**: Each plate maintains independent layer_config_ranges

5. **Copy plate**: Deep copy layer_config_ranges (ModelConfig copy constructor handles this)

6. **Range extends beyond object height**:
   - Plate range [50, 100] when tallest object is 30mm
   - Range applies to nothing - acceptable, just store as-is
   - Consider UI warning for user awareness

7. **Object positioned above bed** (using Z offset):
   - Object at Z=20mm, plate range [0, 10]
   - Range doesn't intersect object - no effect
   - This is expected behavior (absolute Z)

8. **Identical settings in plate and object modifiers**:
   - No conflict, object value used (same result either way)
   - Don't optimize away - keep logic simple

9. **Multi-filament considerations**:
   - Some settings are per-filament (sparse_infill_filament, etc.)
   - Plate modifier may set filament-specific setting
   - Object may use different filament
   - **Behavior**: Setting applies to whatever filament is active at that region
   - Document this interaction for users

10. **Floating-point boundaries**:
    - Range [0, 9.999999999] and [10, 20] might create micro-gaps
    - Use EPSILON comparison in boundary merging
    - `z_max <= z_min + EPSILON` check handles this

11. **Undo/Redo state management**:
    - ModelConfig uses timestamps for change tracking
    - PartPlate should follow same pattern
    - Consider: should plate modifier changes be undoable?
    - Existing plate settings (bed type, name) may not be in undo stack

12. **Re-slice triggering**:
    - Plate layer config changes must invalidate affected PrintObjects
    - In `Print::apply()`, detect changes to `m_plate_layer_config_ranges`
    - Set appropriate invalidation flags (similar to object config changes)

## Performance Considerations

### Merge Algorithm Complexity

The merge algorithm collects boundaries and iterates:
- **Time**: O(P + O) for boundary collection + O(B × (P + O)) for interval iteration
  - P = number of plate ranges
  - O = number of object ranges
  - B = number of unique boundaries (≤ 2×(P+O))
- **Space**: O(B) for sorted boundaries + O(B) for merged configs

For typical use (< 10 ranges each), this is negligible.

### Caching Considerations

**If using Persistent Storage approach**:
- Cache is per-object: `std::map<ObjectID, t_layer_config_ranges>`
- Must invalidate when:
  - Plate ranges change
  - Object ranges change
  - Object is deleted

**Invalidation in Print::apply()**:
```cpp
// Detect plate range changes
if (m_plate_layer_config_ranges != new_plate_ranges) {
    m_plate_layer_config_ranges = new_plate_ranges;
    m_merged_layer_config_ranges.clear();  // Invalidate all cached merges
    // Mark all objects as needing region regeneration
}
```

### Memory Usage

Each merged range stores a `ModelConfig` containing a `DynamicPrintConfig`:
- Typical config: ~100-200 bytes (only non-default values stored)
- With 10 merged intervals: ~1-2 KB per object
- Negligible compared to model/mesh data

## Future Extensions

### Plate-Only Settings

For settings like temperature and retraction that don't belong in `PrintRegionConfig`:

1. Create new config category or extend merge function
2. Modify G-code generation in `GCode.cpp`
3. Handle temperature change timing (before layer starts)
4. Consider multi-filament implications

### Potential Plate-Only Settings

- `temperature_override` - Override nozzle temperature
- `first_layer_temperature_override` - Override first layer temp
- `retract_length_override` - Override retraction length
- `retract_speed_override` - Override retraction speed
- `fan_min_speed_override` - Override minimum fan speed
- `custom_gcode` - Inject custom G-code at height

---

## Design Review Summary

This section documents issues identified during design review and their resolutions.

### Critical Issues Addressed

| Issue | Original Design | Resolution |
|-------|-----------------|------------|
| **Pointer lifetime** | `merge_layer_config_ranges()` returned temporary map; `LayerRanges` stores pointers | Two approaches provided: (1) Two-Phase Lookup at point of use, (2) Persistent storage in `Print` |
| **LayerRanges behavior** | Assumed configs are merged at overlap | Clarified: first range wins, no merging. Design adapted accordingly |
| **GUI/Core layer separation** | Called `plate_layer_config_ranges()` from `Print::apply()` | Data flows via `BackgroundSlicingProcess::apply()` to `Print::set_plate_layer_config_ranges()` |
| **ModelConfig API** | Used `std::move` with `assign_config()` | Corrected: method takes `const&`, move has no effect |

### Missing Considerations Added

| Area | Details |
|------|---------|
| **Print-by-object behavior** | Clarified: use Absolute Z (from bed), not object-relative Z |
| **Range validation** | Added validation function for min_z, max_z bounds |
| **Performance** | Added complexity analysis and caching strategy |
| **12 additional edge cases** | Multi-filament, floating-point, undo/redo, re-slice triggering, etc. |

### Implementation Recommendation

**Recommended approach**: Two-Phase Lookup (Option A)
- Simpler implementation
- No cache management complexity
- Config merging happens at `region_config_from_model_volume()` call site
- Both plate and object `LayerRanges` maintained separately

**Alternative**: Persistent Storage (Option B)
- Better if many regions reference same merged config (deduplication)
- Requires careful cache invalidation
- More complex but potentially more efficient for complex models
