# Bridge Infill Reordering Feature - Technical Design

## Problem Statement

When a layer contains multiple bridges interspersed with other infill features, OrcaSlicer currently prints them in shortest-path order. This leads to:

1. **Frequent fan toggling**: Fan turns on/off for each bridge, causing wear and potential issues
2. **Rapid travel movements**: Moving back and forth between bridges and other features
3. **Poor bridge quality**: Fan may not reach optimal speed before bridge completes

Example problematic sequence:
```
Something -> Bridge 1 -> Something -> Bridge 2 -> Something -> Bridge 3 -> Something -> Bridge 4
```

Ideal sequence:
```
Something -> Something -> Something -> Bridge 1 -> Bridge 2 -> Bridge 3 -> Bridge 4
```

## Solution Overview

Add a new boolean setting `bridge_infill_at_end` that, when enabled, reorders infill extrusions so that non-bridge infills print before bridge infills **within each island/region**.

### Key Decisions (Confirmed with User)

1. **Scope**: Only bridge **infill** roles are reordered. Overhang perimeters (`erOverhangPerimeter`) remain in normal perimeter sequence.

2. **Grouping**: Per-island/region grouping (not entire layer). This minimizes travel between disconnected regions.

3. **Default**: Setting is disabled by default for backward compatibility.

## Technical Analysis

### Current Code Flow

#### Layer Printing Orchestration
**File**: `/src/libslic3r/GCode.cpp` - `process_layer()` (lines 4441-4467)

```cpp
// Current order within each island:
1. Perimeters (non-infill_first)
2. Infill (ALL types mixed, including bridges)  <- modification point
3. Perimeters (infill_first)
4. Ironing
```

#### Infill Extrusion
**File**: `/src/libslic3r/GCode.cpp` - `extrude_infill()` (lines 5093-5119)

Current implementation:
```cpp
std::string GCode::extrude_infill(...) {
    for (const Region &region : by_region) {
        // Collect all non-ironing infills (bridges mixed with others)
        for (ExtrusionEntity *ee : region.infills)
            if ((ee->role() == erIroning) == ironing)
                extrusions.emplace_back(ee);

        // Optimize travel for ALL infills together
        chain_and_reorder_extrusion_entities(extrusions, &m_last_pos);

        // Print all infills
        for (const ExtrusionEntity *fill : extrusions)
            gcode += this->extrude_entity(*fill, ...);
    }
}
```

### Bridge Extrusion Roles

**File**: `/src/libslic3r/ExtrusionEntity.hpp` (lines 20-45)

Bridge infill roles to reorder:
- `erBridgeInfill` - External bridge infill (bottom bridges)
- `erExtraBridgeInfill` - Secondary external bridge layer
- `erInternalBridgeInfill` - First internal bridge over sparse infill
- `erExtraInternalBridgeInfill` - Second internal bridge layer

**NOT reordered** (stays with perimeters):
- `erOverhangPerimeter` - Bridge perimeters/walls

### Existing Helper Function

```cpp
// ExtrusionEntity.hpp lines 105-111
inline bool is_bridge(ExtrusionRole role) {
    return role == erBridgeInfill
        || role == erInternalBridgeInfill
        || role == erExtraBridgeInfill
        || role == erExtraInternalBridgeInfill
        || role == erOverhangPerimeter;  // Includes perimeters - NOT what we want
}
```

We need a new helper that excludes `erOverhangPerimeter`.

### Settings System Pattern

**Declaration** (`PrintConfig.hpp`):
```cpp
PRINT_CONFIG_CLASS_DEFINE(
    PrintRegionConfig,
    ...
    ((ConfigOptionBool, is_infill_first))
    ((ConfigOptionBool, bridge_infill_at_end))  // NEW
    ...
)
```

**Definition** (`PrintConfig.cpp`):
```cpp
def = this->add("bridge_infill_at_end", coBool);
def->label    = L("Print bridge infill last");
def->tooltip  = L("...");
def->category = L("Quality");
def->mode     = comAdvanced;
def->set_default_value(new ConfigOptionBool{false});
```

**GUI** (`Tab.cpp`):
```cpp
optgroup->append_single_option_line("bridge_infill_at_end", "...");
```

## Implementation Plan

### File Changes

#### 1. ExtrusionEntity.hpp
**Location**: After `is_bridge()` function (~line 111)

Add new helper function:
```cpp
// Returns true for bridge INFILL roles only (excludes overhang perimeters)
inline bool is_bridge_infill(ExtrusionRole role) {
    return role == erBridgeInfill
        || role == erInternalBridgeInfill
        || role == erExtraBridgeInfill
        || role == erExtraInternalBridgeInfill;
}
```

#### 2. PrintConfig.hpp
**Location**: In `PrintRegionConfig` section (~line 1090), after `is_infill_first`

Add declaration:
```cpp
((ConfigOptionBool,                is_infill_first))
((ConfigOptionBool,                bridge_infill_at_end))  // NEW
```

#### 3. PrintConfig.cpp
**Location**: After `is_infill_first` definition (~line 1751)

Add definition:
```cpp
def = this->add("bridge_infill_at_end", coBool);
def->label    = L("Print bridge infill last");
def->tooltip  = L("When enabled, bridge infill (both internal and external) will be printed "
                  "after other infill types within each island. This reduces fan speed toggling "
                  "between bridge and non-bridge areas, which can improve print quality and "
                  "reduce wear on the part cooling fan.");
def->category = L("Quality");
def->mode     = comAdvanced;
def->set_default_value(new ConfigOptionBool{false});
```

#### 4. GCode.cpp
**Location**: `extrude_infill()` function (lines 5093-5119)

Modified implementation:
```cpp
std::string GCode::extrude_infill(const Print &print,
    const std::vector<ObjectByExtruder::Island::Region> &by_region, bool ironing)
{
    std::string          gcode;
    ExtrusionEntitiesPtr extrusions;
    const char*          extrusion_name = ironing ? "ironing" : "infill";

    for (const ObjectByExtruder::Island::Region &region : by_region)
        if (! region.infills.empty()) {
            extrusions.clear();
            extrusions.reserve(region.infills.size());
            for (ExtrusionEntity *ee : region.infills)
                if ((ee->role() == erIroning) == ironing)
                    extrusions.emplace_back(ee);
            if (! extrusions.empty()) {
                m_config.apply(print.get_print_region(&region - &by_region.front()).config());

                // NEW: Check if we should print bridge infill at the end
                if (m_config.bridge_infill_at_end && !ironing) {
                    // Partition into non-bridge and bridge infill
                    ExtrusionEntitiesPtr non_bridge_extrusions;
                    ExtrusionEntitiesPtr bridge_extrusions;
                    non_bridge_extrusions.reserve(extrusions.size());
                    bridge_extrusions.reserve(extrusions.size());

                    for (ExtrusionEntity *ee : extrusions) {
                        if (is_bridge_infill(ee->role()))
                            bridge_extrusions.emplace_back(ee);
                        else
                            non_bridge_extrusions.emplace_back(ee);
                    }

                    // Print non-bridge infills first (with travel optimization)
                    if (!non_bridge_extrusions.empty()) {
                        chain_and_reorder_extrusion_entities(non_bridge_extrusions, &m_last_pos);
                        for (const ExtrusionEntity *fill : non_bridge_extrusions) {
                            auto *eec = dynamic_cast<const ExtrusionEntityCollection*>(fill);
                            if (eec) {
                                for (ExtrusionEntity *ee : eec->chained_path_from(m_last_pos).entities)
                                    gcode += this->extrude_entity(*ee, extrusion_name);
                            } else
                                gcode += this->extrude_entity(*fill, extrusion_name);
                        }
                    }

                    // Print bridge infills last (with travel optimization)
                    if (!bridge_extrusions.empty()) {
                        chain_and_reorder_extrusion_entities(bridge_extrusions, &m_last_pos);
                        for (const ExtrusionEntity *fill : bridge_extrusions) {
                            auto *eec = dynamic_cast<const ExtrusionEntityCollection*>(fill);
                            if (eec) {
                                for (ExtrusionEntity *ee : eec->chained_path_from(m_last_pos).entities)
                                    gcode += this->extrude_entity(*ee, extrusion_name);
                            } else
                                gcode += this->extrude_entity(*fill, extrusion_name);
                        }
                    }
                } else {
                    // Original behavior: chain all extrusions together
                    chain_and_reorder_extrusion_entities(extrusions, &m_last_pos);
                    for (const ExtrusionEntity *fill : extrusions) {
                        auto *eec = dynamic_cast<const ExtrusionEntityCollection*>(fill);
                        if (eec) {
                            for (ExtrusionEntity *ee : eec->chained_path_from(m_last_pos).entities)
                                gcode += this->extrude_entity(*ee, extrusion_name);
                        } else
                            gcode += this->extrude_entity(*fill, extrusion_name);
                    }
                }
            }
        }
    return gcode;
}
```

#### 5. Tab.cpp
**Location**: In "Bridging" optgroup section (~line 2219)

Add GUI line:
```cpp
optgroup->append_single_option_line("bridge_infill_at_end", "quality_settings_bridging#bridge-infill-at-end");
```

## Design Considerations

### Why Partition Before Chaining?

The `chain_and_reorder_extrusion_entities()` function optimizes travel distance using a greedy algorithm. If we sorted after chaining, we would break this optimization. By partitioning first and chaining each group separately, we maintain travel optimization within each group.

### Why Per-Region Instead of Per-Layer?

1. The current `extrude_infill()` already iterates per-region
2. Each region gets its own config applied via `m_config.apply()`
3. Per-region is more efficient (less travel between disconnected areas)
4. Setting naturally applies per-region since it's a `PrintRegionConfig` option

### Ironing Exclusion

The `&& !ironing` check ensures we don't apply bridge reordering to ironing passes. Ironing should always be last and has dedicated handling.

### ExtrusionEntityCollection Handling

When an extrusion is an `ExtrusionEntityCollection`, its `role()` returns `erMixed` if contents differ. The `is_bridge_infill()` check returns `false` for mixed collections, which is acceptable - mixed collections go to non-bridge group.

### Backward Compatibility

- Default value is `false` - existing profiles/projects unchanged
- Missing setting in old projects automatically uses default
- No migration code needed

## Testing Recommendations

1. **Basic functionality**: Enable setting, slice model with bridges, verify bridge infill appears after other infill in G-code preview

2. **Fan commands**: Check G-code for reduced `;_OVERHANG_FAN_START/END` and `;_INTERNAL_BRIDGE_FAN_START/END` toggling

3. **Travel optimization**: Verify reasonable travel distances within each group

4. **Edge cases**:
   - Model with only bridge infill
   - Model with no bridge infill
   - Multi-region objects
   - Internal bridges only
   - External bridges only

5. **Compatibility**: Verify ironing behavior unchanged

## Files Summary

| File | Change |
|------|--------|
| `src/libslic3r/ExtrusionEntity.hpp` | Add `is_bridge_infill()` helper |
| `src/libslic3r/PrintConfig.hpp` | Declare `bridge_infill_at_end` setting |
| `src/libslic3r/PrintConfig.cpp` | Define setting with defaults/tooltip |
| `src/libslic3r/GCode.cpp` | Modify `extrude_infill()` to partition bridges |
| `src/slic3r/GUI/Tab.cpp` | Add checkbox in Bridging section |

## Assumptions Recorded

1. **Assumption**: Travel optimization within each partition (non-bridge and bridge) is sufficient; we don't need global optimization across both groups.
   - **Rationale**: The `m_last_pos` is updated continuously, so bridges start chaining from where non-bridges ended.

2. **Assumption**: `erMixed` role collections should go to non-bridge group.
   - **Rationale**: Conservative approach - only explicitly bridge infill is moved.

3. **Assumption**: The setting should be in `PrintRegionConfig` (per-object/region setting) not `PrintConfig` (global).
   - **Rationale**: Follows pattern of `is_infill_first` and allows per-object configuration.
