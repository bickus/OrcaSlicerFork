# Plan: Plate Height Range Modifiers - ObjectList Integration

## Goal
Replace the custom `PlateHeightRangesPanel` UI with proper ObjectList integration that mirrors how object height range modifiers work.

## Current State
- Core slicing integration is complete (PartPlate storage, Print transfer, PrintApply merge)
- 3MF serialization is complete
- Custom panel UI in PlateSettingsDialog (to be removed)

## Target UX (matching object height modifiers)
```
ObjectList Tree:
├─ Plate 1
│   ├─ itLayerRoot ("Layers")     ← NEW: under plate instead of object
│   │   ├─ itLayer "0.00-10.00"
│   │   │   └─ itSettings
│   │   └─ itLayer "15.00-25.00"
│   │       └─ itSettings
│   └─ Object 1
│       └─ ...
```

When plate layer is selected:
- Right panel shows ObjectLayers with Z range editors
- Settings tabs show the layer config (same as object layers)

## Files to Modify

### 1. ObjectDataViewModel.hpp/cpp
- Modify `AddLayersRoot()` to work with plate items (currently only objects)
- Modify `AddLayersChild()` to work with plate layer ranges
- May need to track plate vs object layer root differently

### 2. GUI_ObjectList.cpp/hpp
- Add `add_layer_range_after_current()` handling for plates
- Add `del_layer_range()` handling for plates
- Modify selection handling to recognize plate layers
- Add context menu item "Add Height Range" for plates

### 3. GUI_ObjectLayers.cpp/hpp
- Modify to handle plate layer selections (currently only handles object layers)
- When `itLayerRoot` parent is a plate: use `PartPlate::layer_config_ranges()`
- When `itLayer` parent's grandparent is a plate: same

### 4. GUI_ObjectSettings.cpp
- Modify `UpdateAndShow()` to handle plate layer selections
- Set `TabPrintModel` with plate layer config instead of object layer config

### 5. PlateSettingsDialog.cpp/hpp
- Remove `PlateHeightRangesPanel` integration
- Keep other plate settings (bed type, print sequence, etc.)

### 6. GUI_PlateHeightRanges.cpp/hpp
- DELETE these files (no longer needed)

### 7. CMakeLists.txt
- Remove GUI_PlateHeightRanges source files

## Key Implementation Details

### Distinguishing Plate vs Object Layers
The `ObjectDataViewModelNode` will need to know if its `itLayerRoot`/`itLayer` belongs to a plate or object. Options:
1. Add a flag `m_is_plate_layer` to the node
2. Check parent type at runtime (plate layer root's parent is `itPlate`)

### Data Source Mapping
| Selection | Data Source |
|-----------|-------------|
| Object's itLayerRoot | `model_object->layer_config_ranges` |
| Object's itLayer | `model_object->layer_config_ranges[range]` |
| **Plate's itLayerRoot** | `part_plate->layer_config_ranges()` |
| **Plate's itLayer** | `part_plate->layer_config_ranges()[range]` |

### Synchronization
When plate layer configs change via ObjectLayers panel:
1. Update `PartPlate::layer_config_ranges()`
2. Trigger re-slice (already handled by `Print::set_plate_layer_config_ranges()`)

## Implementation Order

1. **ObjectDataViewModel**: Add plate layer root/child support
2. **GUI_ObjectList**: Add plate layer add/delete operations + context menu
3. **GUI_ObjectLayers**: Handle plate layer selection
4. **GUI_ObjectSettings**: Show plate layer settings in tabs
5. **Cleanup**: Remove PlateHeightRangesPanel from PlateSettingsDialog
6. **Delete**: Remove GUI_PlateHeightRanges files

## User Decisions
- **Layers node visibility**: Only when ranges exist (cleaner tree)
- **Context menu wording**: "Add Height Range"

## Detailed Implementation Steps

### Step 1: ObjectDataViewModel - Add Plate Layer Support
**File**: `src/slic3r/GUI/ObjectDataViewModel.cpp`

1. Modify `AddLayersRoot(wxDataViewItem parent)`:
   - Currently assumes parent is an object
   - Add check: if parent type is `itPlate`, create layer root for plate
   - Store plate index in node (new member or use existing mechanism)

2. Modify `AddLayersChild(wxDataViewItem parent, layer_range, idx)`:
   - Add plate layer child support
   - Link to plate's layer_config_ranges instead of object's

3. Add helper methods:
   - `bool IsPlateLayerRoot(wxDataViewItem)`
   - `bool IsPlateLayer(wxDataViewItem)`
   - `PartPlate* GetPlateFromLayerItem(wxDataViewItem)`

### Step 2: GUI_ObjectList - Add/Delete Operations for Plate Layers
**File**: `src/slic3r/GUI/GUI_ObjectList.cpp`

1. Add context menu item for plates:
   - In plate context menu creation, add "Add Height Range" item
   - Triggers creation of first range (0-10mm default) + Layers root node

2. Modify `add_layer_range_after_current()`:
   - Check if current selection is plate layer vs object layer
   - For plate: modify `PartPlate::layer_config_ranges()`
   - For object: existing behavior

3. Modify `del_layer_range()`:
   - Same pattern - check plate vs object
   - When deleting last plate range, also remove LayerRoot node

4. Add `layers_editing()` support for plates

### Step 3: GUI_ObjectLayers - Handle Plate Layer UI
**File**: `src/slic3r/GUI/GUI_ObjectLayers.cpp`

1. Modify `UpdateAndShow()`:
   - Detect if selected layer belongs to plate or object
   - Get data from appropriate source

2. Modify `create_layer()` / `create_layers_list()`:
   - Use `PartPlate::layer_config_ranges()` for plate layers

3. Modify callbacks (add/delete buttons):
   - Route to plate-specific operations when applicable

### Step 4: GUI_ObjectSettings - Settings Tab for Plate Layers
**File**: `src/slic3r/GUI/GUI_ObjectSettings.cpp`

1. Modify `UpdateAndShow()`:
   - When plate layer selected, get config from `PartPlate::layer_config_ranges()[range]`
   - Pass to `TabPrintModel::set_model_config()`

### Step 5: Cleanup PlateSettingsDialog
**File**: `src/slic3r/GUI/PlateSettingsDialog.cpp`

1. Remove `#include "GUI_PlateHeightRanges.hpp"`
2. Remove `m_height_ranges_panel` member and all related code
3. Remove `sync_height_ranges()`, `get_height_ranges()`, `has_height_ranges()`

**File**: `src/slic3r/GUI/PlateSettingsDialog.hpp`
1. Remove `PlateHeightRangesPanel* m_height_ranges_panel`
2. Remove height range method declarations

### Step 6: Delete Custom UI Files
- Delete `src/slic3r/GUI/GUI_PlateHeightRanges.cpp`
- Delete `src/slic3r/GUI/GUI_PlateHeightRanges.hpp`

### Step 7: Update CMakeLists.txt
**File**: `src/slic3r/CMakeLists.txt`
- Remove `GUI_PlateHeightRanges.cpp` and `GUI_PlateHeightRanges.hpp`

### Step 8: Update Plater.cpp
**File**: `src/slic3r/GUI/Plater.cpp`
- Remove height range sync/save code from `open_platesettings_dialog()`

## Testing Checklist
- [ ] Right-click plate → "Add Height Range" creates Layers node + first range
- [ ] Select plate layer → ObjectLayers panel shows Z editors
- [ ] Select plate layer → Settings tabs show layer config
- [ ] Edit Z range in panel → updates plate data
- [ ] Delete last range → removes Layers node from tree
- [ ] Save/load 3MF preserves plate height ranges
- [ ] Slicing applies plate height modifiers correctly
- [ ] Object height modifiers still work (regression test)

## Implementation Status: COMPLETE

All steps have been implemented:

1. **ObjectDataViewModel** - Added plate layer support methods:
   - `IsPlateLayerRoot()`, `IsPlateLayer()`, `GetPlateFromLayerItem()`
   - `GetItemByPlateLayerRange()`, `AddPlateLayersRoot()`, `AddPlateLayersChild()`

2. **GUI_ObjectList** - Added plate layer operations:
   - `plate_layers_editing()`, `add_plate_layer_root_item()`
   - `del_plate_layer_range()`, `add_plate_layer_range_after_current()`
   - `can_add_new_plate_range_after_current()`, `edit_plate_layer_range()`
   - Context menu item "Add Height Range" for plates

3. **GUI_ObjectLayers** - Handle plate layer UI:
   - Modified `update_layers_list()` to detect plate vs object layers
   - Added `create_plate_layers_list()` and `create_plate_layer()`
   - Routes callbacks to plate-specific operations

4. **GUI_ObjectSettings** - Settings tabs for plate layers:
   - Modified `update_settings_list()` to get config from plate's layer_config_ranges

5. **PlateSettingsDialog** - Cleanup:
   - Removed `PlateHeightRangesPanel` integration
   - Removed height range sync/get methods

6. **Deleted files**:
   - `GUI_PlateHeightRanges.cpp`
   - `GUI_PlateHeightRanges.hpp`

7. **CMakeLists.txt** - Removed deleted source files

8. **Plater.cpp** - Removed height range sync code from dialog

## Bug Fixes Applied

The following bugs were identified and fixed during code review:

### 1. GetItemByPlateLayerRange - Wrong m_plates indexing (ObjectDataViewModel.cpp)
**Problem**: Used `m_plates[plate_idx]` assuming the vector is indexed by plate index, but `m_plates` is not indexed by plate_idx - it's a list of plate nodes in insertion order.
**Fix**: Use `GetItemByPlateId(plate_idx)` instead of direct array access.

### 2. GUI_ObjectSettings - Crash on plate layer selection (GUI_ObjectSettings.cpp)
**Problem**: `GetObjectIdByItem()` was called unconditionally for all non-plate items, including plate layers which have no associated object. This would crash due to null pointer dereference.
**Fix**: Check for plate layers and plate layer roots early in the loop, handle them separately with `continue`, before calling `GetObjectIdByItem()`.

### 3. GetObject - Null dereference for plate layer items (ObjectDataViewModel.cpp)
**Problem**: When traversing parent chain for a plate layer item, the code would eventually reach a plate node, then call `GetParent()` on it (returning nullptr), then dereference nullptr in the next iteration.
**Fix**: Added null check in while loop and early return if we reach a plate node (indicating this is a plate layer with no associated object).

### 4. get_item_config - Crash on plate layer selection (GUI_ObjectList.cpp)
**Problem**: When a plate layer item is selected in the ObjectList, `part_selection_changed()` calls `get_item_config(item)`. This function tried to get the object index via `GetObjectIdByItem()`, which returns -1 for plate layers, then accessed `(*m_objects)[-1]` causing an access violation crash.
**Fix**: Added early check for plate layers in `get_item_config()`. For plate layers, retrieve the config from `PartPlate::layer_config_ranges()` instead of trying to access the non-existent object.

### 5. update_settings_list - Null dereference of parent_object for plate layers (GUI_ObjectSettings.cpp)
**Problem**: When `is_layer_range_settings` is true, the code unconditionally accessed `parent_object->config`. For plate layers, `parent_object` is `nullptr` because plate layers have no associated ModelObject, causing a crash.
**Fix**: Added null check for `parent_object` before accessing its config. For plate layers, pass empty config to `tab_object->set_model_config()`.

### 6. TabPrintLayer::notify_changed - No re-slice triggered for plate layer config changes (Tab.cpp)
**Problem**: When the user changed settings (like `layer_height`) in the plate layer tab, the value was saved to `PartPlate::layer_config_ranges()`, but no re-slice was triggered. The slicing would only use the new values if manually triggered.
**Fix**: Added `wxGetApp().plater()->update()` and `schedule_background_process()` calls in `TabPrintLayer::notify_changed()` to trigger a re-slice when layer config values change.

### 7. PrintObject::slice - layer_height not applied from plate layer modifiers (PrintObjectSlice.cpp)
**Problem**: `layer_height` is part of `PrintObjectConfig`, NOT `PrintRegionConfig`. The existing implementation tried to apply plate layer `layer_height` via `PrintRegionConfig` in `region_config_from_model_volume()`, but since `layer_height` isn't a valid key in `PrintRegionConfig`, it was silently ignored. Object layer modifiers work because they use a separate code path that converts `layer_config_ranges` to `layer_height_profile` via `layer_height_profile_from_ranges()`.
**Fix**: Modified `PrintObject::slice()` to merge plate layer config ranges with object layer config ranges before generating the layer height profile. Plate ranges are applied first (lower priority), then object ranges (higher priority) can override them.

### 8. Plate layer changes don't invalidate slice preview (GUI_ObjectList.cpp, Tab.cpp)
**Problem**: When plate layer ranges are added, deleted, or modified (including editing Z heights or changing settings), the sliced preview was not invalidated. The user would have to manually trigger a re-slice. This happened because object layer changes call `changed_object()` which calls `schedule_background_process()`, but plate layer operations only called `plater()->update()` which doesn't trigger invalidation for FFF printing.
**Fix**: Added `schedule_background_process()` calls after `plater()->update()` in all plate layer modification functions:
- `del_plate_layer_range()`
- `add_plate_layer_range_after_current()`
- `edit_plate_layer_range()`
- `TabPrintLayer::notify_changed()`

### 9. PrintRegionConfig settings (e.g., wall_loops) not applied from plate layer modifiers (PrintApply.cpp)
**Problem**: Settings like `wall_loops`, `sparse_infill_density`, and other `PrintRegionConfig` options were not being applied from plate layer modifiers. This happened because `verify_update_print_object_regions()` was called without plate layer ranges, causing it to compute region configs without plate modifications and incorrectly conclude regions hadn't changed.
**Fix**: Modified `verify_update_print_object_regions()` to accept and use plate layer ranges:
- Added `plate_layer_ranges` parameter to the function signature
- Use `plate_layer_ranges.config_at_z()` when computing region configs for model parts
- Updated the call site in `Print::apply()` to pass `LayerRanges(m_plate_layer_config_ranges)`

### 10. UI buttons/preview don't update immediately when plate layer settings change (GUI_ObjectList.cpp, Tab.cpp)
**Problem**: When plate layer settings changed, `schedule_background_process()` was called which starts a 500ms timer. The UI (slice button, print button, preview) only updated when the timer fired, causing a delayed/inconsistent user experience. Users had to switch tabs to see the proper state.
**Fix**: Two-part fix:
1. Call `plate->update_slice_result_valid_state(false)` to immediately invalidate the slice result before updating UI
2. Call `mainframe->update_slice_print_status(MainFrame::eEventSliceUpdate, true, false)` after `schedule_background_process()` to immediately update button states

Applied in all plate layer modification functions:
- `del_plate_layer_range()`
- `add_plate_layer_range_after_current()`
- `edit_plate_layer_range()`
- `TabPrintLayer::notify_changed()`

### 11. PrintRegionConfig settings applied to ALL heights instead of just the modifier range (PrintApply.cpp)
**Problem**: Settings like `wall_loops` from a plate layer modifier at range (5mm, 10mm) were being applied to ALL heights, not just the specified range. This was due to a boundary condition bug in `LayerRanges::config_at_z()`. The condition `z < range.layer_height_range.second + EPSILON` caused z=10 to match the (5, 10) range because `10 < 10 + EPSILON` is true. Since ranges are checked in order, the modifier config was returned for z values at and beyond the end boundary.
**Fix**: Changed the upper bound check to use no EPSILON, so the half-open interval [start, end) is properly enforced:
- Before: `z < range.layer_height_range.second + EPSILON` (wrong - includes boundary)
- After: `z < range.layer_height_range.second` (correct - excludes boundary exactly)

### 12. Plate layer region boundaries not regenerated when plate modifiers change (PrintApply.cpp)
**Problem**: When a plate layer modifier was added (e.g., at range (5,10)), the `verify_update_print_object_regions()` function would iterate over the EXISTING layer boundaries (which were just (0, DBL_MAX) if no modifiers existed before). It would call `config_at_z(0)` which would return nullptr because 0 is not in the new range (5,10). Since the expected config matched the existing config (both had no plate modifier), the function returned true (regions valid) and `generate_print_object_regions()` was never called to create the proper split boundaries.
**Fix**: Added boundary validation at the start of `verify_update_print_object_regions()`:
- Collect all boundaries from existing layer ranges
- Check if plate layer ranges introduce any new boundaries not in the existing set
- If any new boundary is missing, return false to force region regeneration
This ensures that when plate layer modifiers are added/changed, the layer ranges are properly split at the modifier boundaries.
