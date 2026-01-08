# Plate-Level Height Modifiers - Implementation Completion Report

## Summary

Plate-level height modifiers have been fully implemented for OrcaSlicer. This feature allows users to define configuration overrides for specific Z-height ranges that apply to all objects on a build plate.

**Implementation Date**: January 2026
**Design Document**: [design.md](./design.md)

---

## Implementation Status

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | Core Data Structures | Complete |
| Phase 2 | Data Flow Integration | Complete |
| Phase 3 | UI Implementation | Complete |
| Phase 4 | 3MF Serialization | Complete |
| Phase 5 | Polish (copy plate, edge cases) | Complete |

---

## Implementation Details

### Phase 1: Core Data Structures

#### 1.1 PartPlate Storage (GUI Layer)
**File**: `src/slic3r/GUI/PartPlate.hpp`

```cpp
// Private member
t_layer_config_ranges m_layer_config_ranges;  // Plate-level height modifiers

// Public accessors
const t_layer_config_ranges& layer_config_ranges() const;
t_layer_config_ranges& layer_config_ranges();
void clear_layer_config_ranges();
bool has_layer_config_ranges() const;
```

Updated Cereal serialization for undo/redo support.

#### 1.2 PlateData Bridge Struct
**File**: `src/libslic3r/Format/bbs_3mf.hpp`

```cpp
struct PlateData {
    t_layer_config_ranges layer_config_ranges;  // Plate-level height modifiers
};
```

#### 1.3 Print Core Storage
**File**: `src/libslic3r/Print.hpp`

```cpp
// Private
t_layer_config_ranges m_plate_layer_config_ranges;

// Public
void set_plate_layer_config_ranges(const t_layer_config_ranges& ranges);
const t_layer_config_ranges& plate_layer_config_ranges() const;
```

Setter implementation with automatic re-slice triggering on change.

---

### Phase 2: Data Flow Integration

#### 2.1 BackgroundSlicingProcess Transfer
**File**: `src/slic3r/GUI/BackgroundSlicingProcess.cpp`

Transfers plate layer config ranges to Print in `apply()` method.

#### 2.2 Modified region_config_from_model_volume()
**File**: `src/libslic3r/PrintObject.cpp`

Added `plate_layer_config` parameter. Config application order (lowest to highest priority):
1. Default region config
2. Plate layer config (new)
3. Object layer config
4. Volume-specific config

#### 2.3-2.4 generate_print_object_regions() Update
**File**: `src/libslic3r/PrintApply.cpp`

Added `plate_layer_ranges` parameter. All 6 call sites updated to pass both plate and object configs.

---

### Phase 3: UI Implementation

#### New Files
- `src/slic3r/GUI/GUI_PlateHeightRanges.hpp`
- `src/slic3r/GUI/GUI_PlateHeightRanges.cpp`

#### Classes
- **PlateHeightRangeEditor**: Text input for Z values with validation
- **PlateHeightRangeItem**: Single range row with min/max Z, settings button, delete button
- **PlateHeightRangesPanel**: Container panel with add button and list of range items

#### Integration
- Added to `PlateSettingsDialog` as a new section below "Other layer filament sequence"
- Settings selector dialog using `SettingsFactory::get_all_visible_options()`
- Wired in `Plater.cpp::open_platesettings_dialog()` for sync and save

---

### Phase 4: 3MF Serialization

#### Export
**File**: `src/libslic3r/Format/bbs_3mf.cpp`

- New file: `Metadata/plate_layer_config_ranges.xml`
- Function: `_add_plate_layer_config_ranges_file_to_archive()`

XML format:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<plates>
  <plate index="0">
    <range min_z="0.0" max_z="10.0">
      <option opt_key="sparse_infill_density">10%</option>
    </range>
  </plate>
</plates>
```

#### Import
- Member: `std::map<int, t_layer_config_ranges> m_plate_layer_config_ranges`
- Function: `_extract_plate_layer_config_ranges_from_archive()`
- File detection in import loop
- Transfer to PlateData in both import paths

#### PartPlate Store/Load
**File**: `src/slic3r/GUI/PartPlate.cpp`

- `store_to_3mf_structure`: Copies ranges to PlateData
- `load_from_3mf_structure`: Restores ranges from PlateData

---

### Phase 5: Polish

#### Copy Plate
**File**: `src/slic3r/GUI/PartPlate.cpp`

`duplicate_plate()` now copies `layer_config_ranges` to the new plate.

#### Backward Compatibility
Import gracefully handles missing `plate_layer_config_ranges.xml` in old 3MF files.

#### Re-slice Triggering
`Print::set_plate_layer_config_ranges()` invalidates all PrintObjects when ranges change.

#### Undo/Redo
Handled via Cereal serialization in PartPlate.

---

## Data Flow Diagram

```
GUI Layer                                Core Layer
---------                                ----------

PlateSettingsDialog
        |
        v
   PartPlate
   m_layer_config_ranges
        |
        v
BackgroundSlicingProcess::apply()
        |
        v
Print::set_plate_layer_config_ranges()
        |
        v
   Print::apply()
   LayerRanges(m_plate_layer_config_ranges)
        |
        v
generate_print_object_regions()
        |
        v
region_config_from_model_volume(plate_config, object_config, ...)
        |
        v
PrintRegion with merged configs


Persistence Layer
-----------------

PlateData.layer_config_ranges
        |
        +--- Export: _add_plate_layer_config_ranges_file_to_archive()
        |            -> Metadata/plate_layer_config_ranges.xml
        |
        +--- Import: _extract_plate_layer_config_ranges_from_archive()
                     <- Metadata/plate_layer_config_ranges.xml
```

---

## Config Priority Order

When a layer falls within both plate and object height modifier ranges, configs are applied in this order (later overwrites earlier):

1. **Default PrintRegionConfig** - Base settings from profile
2. **Plate layer config** - From plate height modifiers (lowest modifier priority)
3. **Object layer config** - From object height modifiers (higher priority)
4. **Volume config** - From volume-specific settings (highest priority)

---

## Files Modified

| File | Changes |
|------|---------|
| `src/slic3r/GUI/PartPlate.hpp` | Member, accessors, Cereal |
| `src/slic3r/GUI/PartPlate.cpp` | Store/load 3MF, duplicate_plate |
| `src/libslic3r/Format/bbs_3mf.hpp` | PlateData member |
| `src/libslic3r/Format/bbs_3mf.cpp` | Export/import functions |
| `src/libslic3r/Print.hpp` | Member and setter |
| `src/libslic3r/Print.cpp` | Setter implementation |
| `src/libslic3r/PrintObject.cpp` | region_config_from_model_volume() |
| `src/libslic3r/PrintApply.cpp` | generate_print_object_regions(), LayerRanges::config_at_z(), boundary merging |
| `src/slic3r/GUI/BackgroundSlicingProcess.cpp` | Transfer ranges |
| `src/slic3r/GUI/PlateSettingsDialog.hpp` | Member and methods |
| `src/slic3r/GUI/PlateSettingsDialog.cpp` | Panel creation, sync/get |
| `src/slic3r/GUI/Plater.cpp` | Dialog wiring |
| `src/slic3r/GUI/GUI_Factories.cpp` | Plate context menu item |
| `src/slic3r/CMakeLists.txt` | New source files |

## New Files

| File | Description |
|------|-------------|
| `src/slic3r/GUI/GUI_PlateHeightRanges.hpp` | UI panel classes header |
| `src/slic3r/GUI/GUI_PlateHeightRanges.cpp` | UI panel implementation |

---

## Usage

1. Open **Plate Settings** dialog (right-click on plate)
2. Scroll down to **Height Range Modifiers** section
3. Click **+** to add a new height range
4. Enter min and max Z values
5. Click the **gear** icon to select settings to override
6. Click **OK** to apply

The modifiers will apply to all objects on the plate during slicing.

---

## Testing Recommendations

### Functional Testing
1. Create plate with height modifier (e.g., reduce infill above 10mm)
2. Slice and verify G-code shows changed settings in that Z range
3. Add object-level modifier at same height - verify object modifier wins
4. Save/load 3MF and verify ranges persist
5. Duplicate plate and verify ranges are copied
6. Test undo/redo functionality

### Edge Case Testing
1. Empty plate ranges (should work same as before)
2. Overlapping plate and object ranges
3. Print-by-object mode with plate modifiers
4. Multi-plate projects with different modifiers per plate
5. Old 3MF files without plate ranges (backward compatibility)
6. Range extends beyond object height (no effect expected)

### Performance Testing
1. Large number of ranges (10+ per plate)
2. Complex models with many volumes
3. Re-slice performance after plate modifier changes

---

## Known Limitations

1. **Print-by-Object mode**: Uses absolute Z from bed, not object-relative Z
2. **Settings availability**: Only PrintRegionConfig settings (infill, speed, walls, etc.) are available, not temperature/retraction settings
3. **UI refresh**: May need to scroll to see all settings after adding many ranges

---

## Bugs Fixed (Post-Implementation Review)

The following issues were identified and fixed during code review:

### 1. Settings Selection Index Mismatch (Critical)
**File**: `src/slic3r/GUI/GUI_PlateHeightRanges.cpp`

**Issue**: In `on_settings()`, the selection indices were based on `all_options` array, but `choices` only contained options with valid `ConfigOptionDef`. This caused index mismatch when some options lacked definitions.

**Fix**: Added a `valid_options` vector that tracks only options that made it into `choices`, ensuring selection indices map correctly.

### 2. Zero-Height Range Validation
**File**: `src/slic3r/GUI/GUI_PlateHeightRanges.cpp`

**Issue**: The UI allowed `min_z == max_z`, creating invalid zero-height ranges.

**Fix**: Changed validation from `min_z > max_z` to `max_z <= min_z`, and auto-corrects to `min_z + 0.01mm` minimum.

### 3. Missing Exception Handling in XML Import
**File**: `src/libslic3r/Format/bbs_3mf.cpp`

**Issue**: `pt::read_xml()` and `get_child()` can throw exceptions on malformed XML, causing crashes.

**Fix**: Added try-catch blocks around XML parsing, with proper error logging.

### 4. Missing Attribute Handling in XML Import
**File**: `src/libslic3r/Format/bbs_3mf.cpp`

**Issue**: `get<double>("<xmlattr>.min_z")` would throw if attribute is missing.

**Fix**: Changed to `get_optional<double>()` with validation, gracefully skipping invalid ranges with warnings.

### 5. Invalid Range Validation in Import
**File**: `src/libslic3r/Format/bbs_3mf.cpp`

**Issue**: Imported ranges with `max_z <= min_z` would be silently accepted.

**Fix**: Added validation to skip invalid ranges with warning log.

### 6. Removed Misleading std::move
**File**: `src/libslic3r/Format/bbs_3mf.cpp`

**Issue**: `assign_config(std::move(config))` was misleading since the method takes `const&`.

**Fix**: Removed unnecessary `std::move()` call.

### 7. Layer Range Boundary Mismatch (Critical)
**File**: `src/libslic3r/PrintApply.cpp`

**Issue**: The `LayerRanges::config()` method required exact boundary matches. When plate ranges had different boundaries than object ranges (e.g., plate [0-10] vs object [0-DBL_MAX]), the plate config lookup would fail and return nullptr.

**Fix**:
1. Added `config_at_z()` method to `LayerRanges` class for point-based lookup
2. Modified `generate_print_object_regions()` to merge boundaries from both plate and object layer ranges
3. Layer ranges are now built from the union of all Z boundaries, ensuring plate modifiers are correctly applied at their intended heights

This was a fundamental design issue that would have prevented plate modifiers from working correctly in most scenarios.

### 8. Missing operator== for ModelConfig Comparison
**File**: `src/libslic3r/Print.cpp`

**Issue**: The `t_layer_config_ranges` map comparison in `set_plate_layer_config_ranges()` failed to compile because `ModelConfig` doesn't have `operator==`.

**Fix**: Added `layer_config_ranges_equal()` helper function that compares ranges with EPSILON tolerance and configs via their underlying `DynamicPrintConfig` which does have `operator==`.

### 9. Missing UI Access Point for Plate Settings
**File**: `src/slic3r/GUI/GUI_Factories.cpp`

**Issue**: Users could not easily access the PlateSettingsDialog (which contains height range modifiers) from the plate right-click context menu.

**Fix**: Added "Plate Settings" menu item to the plate context menu that opens the PlateSettingsDialog.

### 10. Cached Volume IDs Not Cleared When Layer Ranges Change (Critical)
**File**: `src/libslic3r/PrintApply.cpp`

**Issue**: When plate height modifiers were added or changed, `generate_print_object_regions()` rebuilt the `layer_ranges_regions` structure but did not clear `cached_volume_ids`. In the subsequent call to `update_volume_bboxes()`, volumes were incorrectly treated as "already cached" and looked up from the old (now invalid) cache. Since the layer_ranges had been cleared and rebuilt, the volumes weren't registered in the new ranges, resulting in `volumes: 0` for all layer ranges. This caused `slice_volumes_inner()` to return empty slices, making all layers empty and triggering "No layers were detected" error.

**Symptom**: After creating a plate height modifier and changing any setting (e.g., layer height), clicking Slice would fail with "No layers were detected". The error persisted even after removing the plate height modifier.

**Root Cause**: The `cached_volume_ids` cache invalidation was missing when `can_reuse` was false (i.e., when layer_ranges structure changed).

**Fix**: Added `out->cached_volume_ids.clear()` in `generate_print_object_regions()` when layer ranges are rebuilt:
```cpp
} else {
    out->trafo_bboxes = trafo;
    layer_ranges_regions.clear();
    layer_ranges_regions.reserve(merged_ranges.size());
    for (const auto &range : merged_ranges)
        layer_ranges_regions.push_back({ range.layer_height_range, range.config });
    // Clear cached volume IDs since layer ranges structure changed - volumes need to be re-processed
    out->cached_volume_ids.clear();
}
```

### 11. Plate/Object Layer Height Range Merge Logic
**File**: `src/libslic3r/PrintObject.cpp`

**Issue**: When plate height ranges overlapped with object height ranges, the merge logic in `update_layer_height_profile()` did not properly split plate ranges around object ranges. For example, if plate range [0, 10] contained object range [3.3, 4.3], the entire plate range would be overwritten instead of being split into [0, 3.3], [3.3, 4.3], [4.3, 10].

**Fix**: Rewrote the merge logic to:
1. For each plate range, find all overlapping object ranges
2. Split the plate range around object ranges to find uncovered segments
3. Add uncovered segments as plate range entries
4. Add all object ranges (they take precedence)

This ensures object height modifiers properly override plate modifiers only in their specific Z range while plate modifiers remain active elsewhere.

---

## Debug Logging

Comprehensive debug logging with `[PHM]` prefix was added throughout the plate modifier workflow for troubleshooting:

| File | Function | What's Logged |
|------|----------|---------------|
| `Print.cpp` | `set_plate_layer_config_ranges()` | Ranges received, invalidation |
| `BackgroundSlicingProcess.cpp` | `apply()` | Plate ranges passed to Print |
| `PrintObject.cpp` | `update_layer_height_profile()` | Range merging, input/output ranges |
| `PrintObjectSlice.cpp` | `slice_volumes()` | slice_zs count, layer_ranges, volume counts |
| `PrintApply.cpp` | `generate_print_object_regions()` | Layer ranges, merged ranges, cache clearing |
| `PrintApply.cpp` | `update_volume_bboxes()` | Volume registration, bbox calculations |
| `Slicing.cpp` | `layer_height_profile_from_ranges()` | Input ranges, processing |
| `Slicing.cpp` | `generate_object_layers()` | Layer generation |

To filter these logs: `grep "\[PHM\]" debug.log`

---

## Future Enhancements

1. **Plate-only settings**: Temperature overrides, retraction settings, custom G-code at height
2. **Visual preview**: Show height range boundaries in the 3D view
3. **Import from object**: Copy an object's height modifiers to plate level
