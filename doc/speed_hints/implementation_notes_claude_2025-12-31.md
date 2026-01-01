# Speed Modifier Tracking - Implementation Notes

**Date:** 2025-12-31
**Implementer:** Claude (AI Assistant)
**Status:** Complete (pending compilation verification)

---

## Overview

Full implementation of the Speed Modifier Tracking feature as specified in `speed_modifier_tracking_design.md`. All 8 phases were implemented, followed by:
1. A major refactor to fix a TBB pipeline race condition
2. Creation of a centralized header to resolve circular dependencies

---

## Phase 1: Data Structures

### Implemented
- SpeedModifierEntry struct with Type enum (12 modifier types)
- BaseSpeedType enum (13 base speed types for role-specific labels)
- MoveVertex extension with base_speed, speed_modifiers array, speed_modifier_count, base_type, has_speed_modifiers() method
- G1ModifierData struct with add_modifier(), reset(), reset_variable_modifiers() methods
- CoolingModification struct for tracking cooling slowdowns

### Files Modified
- **`src/libslic3r/GCode/SpeedModifier.hpp`** (NEW) - Central header containing all speed modifier types:
  - SpeedModifierEntry struct with Type enum
  - BaseSpeedType enum
  - G1ModifierData struct
  - CoolingModification struct
- `src/libslic3r/GCode/GCodeProcessor.hpp` - Includes SpeedModifier.hpp, MoveVertex extensions, member variables
- `src/libslic3r/GCode.hpp` - Includes SpeedModifier.hpp
- `src/libslic3r/GCode/CoolingBuffer.hpp` - Includes SpeedModifier.hpp

### Architectural Decision: Centralized Type Header

**Problem:** The original design placed types in multiple headers:
- SpeedModifierEntry and BaseSpeedType in GCodeProcessor.hpp
- G1ModifierData in GCode.hpp
- CoolingModification in CoolingBuffer.hpp

This caused circular dependency issues when GCodeProcessor.hpp needed to use `std::unordered_map<uint64_t, G1ModifierData>` - it couldn't include GCode.hpp because GCode.hpp already includes GCodeProcessor.hpp.

**Solution:** Created `SpeedModifier.hpp` as a standalone header with no dependencies on other project headers (only `<cstdint>`). All three headers now include this central header, avoiding circular dependencies.

---

## Phase 2: G1 Counter Infrastructure

### Implemented
- G1 extrusion counter in GCode class
- Modifier data map (m_g1_modifier_data) and current modifier accumulator
- SmallPerimeterState struct for passing small perimeter info from extrude_loop to _extrude
- CoolingBuffer counter (mutable for const method compatibility)
- GCodeProcessor counter and per-layer data storage

### Files Modified
- `src/libslic3r/GCode.hpp` - Added m_g1_extrusion_counter, m_g1_modifier_data map, m_current_g1_modifiers, SmallPerimeterState
- `src/libslic3r/GCode/CoolingBuffer.hpp` - Added mutable m_g1_extrusion_counter
- `src/libslic3r/GCode/GCodeProcessor.hpp` - Added m_g1_extrusion_counter, m_layer_modifier_data, m_layer_cooling_data

### Decisions Made
- Made CoolingBuffer's counter mutable because parse_layer_gcode() is a const method but needs to modify the counter

---

## Phase 3: ExtrusionProcessor ProcessedPoint Extension

### Implemented
- Extended ProcessedPoint struct with has_curled_slowdown and curled_height_factor fields
- Modified estimate_extrusion_quality() to set these fields when curled edge slowdown is applied

### Files Modified
- `src/libslic3r/GCode/ExtrusionProcessor.hpp` - Added two fields to ProcessedPoint struct, added tracking logic in estimate_extrusion_quality()

---

## Phase 4: GCode.cpp Modifier Recording

### Implemented
- Reset modifiers at start of _extrude() and set base_type based on extrusion role
- Track ScarfJoint modifier in role-based speed lookup
- Track FirstLayer modifier when initial_layer_speed applied
- Track SlowDownLayers modifier for gradual speed increase
- Track VolumetricCap modifier when filament_max_volumetric_speed limits speed
- Track ResonanceAvoidance modifier for external perimeters
- Track SmallPerimeter modifier via state set in extrude_loop()
- G1 counter increment and modifier storage after each extrude_to_xy() call in both non-variable and variable speed loops
- Per-point Overhang and CurledEdge modifier tracking with reset_variable_modifiers() call between G1s

### Files Modified
- `src/libslic3r/GCode.cpp` - Multiple locations in _extrude() function, added tracking in extrude_loop() for small perimeter state, added handling in variable speed loop for per-point modifiers

### Issues Encountered
- When speed != -1 (passed from extrude_loop), the role-based speed lookup was skipped, so base_type wasn't set. Added an else branch to handle this case and properly set base_type and SmallPerimeter modifier.

---

## Phase 5: CoolingBuffer Direct Tracking

### Implemented
- Extended CoolingLine struct with g1_id_start, g1_count, and original_feedrate fields
- G1 counting in parse_layer_gcode() - increments counter when TYPE_ADJUSTABLE is set and for accumulated G1s
- Recording original feedrate for adjustable lines
- Recording cooling modifications in apply_layer_cooldown() when line->slowdown is true

### Files Modified
- `src/libslic3r/GCode/CoolingBuffer.cpp` - Extended CoolingLine struct, added G1 counting in parsing, added slowdown recording

### Decisions Made
- Used g1_id_start and g1_count instead of single g1_id because CoolingBuffer groups multiple G1s into one CoolingLine segment
- When recording slowdown, iterate through all G1s in the segment's range

---

## Phase 6: Data Passing Integration (REFACTORED)

### Original Implementation (Had Race Condition)
The original design set shared pointers to m_g1_modifier_data and m_cooling_data maps at export start. However, this caused a race condition with the TBB pipeline: even with `serial_in_order` filters, multiple tokens (layers) can be in-flight simultaneously across different stages. This meant the generator stage could modify the shared maps while the output stage was reading from them.

### New Implementation: Per-Layer Data Passing
Each layer's modifier data now travels **with** the layer through the TBB pipeline:

1. **LayerResult struct extended** with:
   - `std::unordered_map<uint64_t, G1ModifierData> modifier_data`
   - `std::unordered_map<uint64_t, CoolingModification> cooling_data`

2. **Generator stage** (process_layer):
   - At layer end, moves m_g1_modifier_data to result.modifier_data
   - Clears m_g1_modifier_data for next layer

3. **Cooling stage** (process_layer):
   - Now takes `cooling_data` reference parameter
   - Populates it with cooling modifications instead of storing to member variable

4. **Pipeline filters changed** to pass LayerResult through:
   - cooling: `LayerResult → LayerResult` (was `LayerResult → std::string`)
   - pa_processor: `LayerResult → LayerResult` (was `std::string → std::string`)
   - fan_mover: `LayerResult → LayerResult` (was `std::string → std::string`)
   - output: `LayerResult → void` (was `std::string → void`)

5. **GCodeOutputStream.write_layer()** method added:
   - Sets layer data on GCodeProcessor before writing gcode
   - Calls `m_processor.set_layer_data(std::move(modifier_data), std::move(cooling_data))`

6. **GCodeProcessor.set_layer_data()** method:
   - Takes ownership of the layer's modifier and cooling data
   - Stores in m_layer_modifier_data and m_layer_cooling_data

### Files Modified
- `src/libslic3r/GCode.hpp` - Extended LayerResult struct, added write_layer() to GCodeOutputStream
- `src/libslic3r/GCode.cpp` - Modified process_layer() to move modifier_data to result, modified both TBB pipelines to pass LayerResult through, added write_layer() implementation
- `src/libslic3r/GCode/CoolingBuffer.hpp` - Changed process_layer() and apply_layer_cooldown() signatures to accept cooling_data reference
- `src/libslic3r/GCode/CoolingBuffer.cpp` - Updated process_layer() and apply_layer_cooldown() implementations
- `src/libslic3r/GCode/GCodeProcessor.hpp` - Changed from pointer members to value members (m_layer_modifier_data, m_layer_cooling_data), added set_layer_data() method
- `src/libslic3r/GCode/GCodeProcessor.cpp` - Updated store_move_vertex() to use new member names

---

## Phase 7: GCodeProcessor Integration

### Implemented
- G1 extrusion counter increment in store_move_vertex() for Extrude moves
- Lookup of modifier data from m_layer_modifier_data and copying to MoveVertex
- Lookup of cooling data from m_layer_cooling_data and adding LayerTimeCooling modifier if present

### Files Modified
- `src/libslic3r/GCode/GCodeProcessor.cpp` - Added tracking code in store_move_vertex()

### Issues Encountered
- Compilation error: MoveVertex is nested inside GCodeProcessorResult, so needed to use fully qualified name GCodeProcessorResult::MoveVertex::MaxSpeedModifiers

---

## Phase 8: UI Display

### Implemented
- get_modifier_name() helper function for modifier type names
- get_base_type_label() helper function for base speed type labels
- Enhanced Feedrate tooltip case to display modifier chain when has_speed_modifiers() returns true

### Files Modified
- `src/slic3r/GUI/GCodeViewer.cpp` - Added helper functions, enhanced Feedrate case in Marker::render()

### Decisions Made
- Display format: Base speed on first line with optional type label, then each modifier on subsequent lines with arrow prefix
- Overhang shows percentage, CurledEdge shows factor, other modifiers show reduction amount

---

## Summary of All Files Modified

| File | Changes |
|------|---------|
| `src/libslic3r/GCode/SpeedModifier.hpp` | **NEW** - Central header for all speed modifier types |
| `src/libslic3r/GCode/GCodeProcessor.hpp` | Includes SpeedModifier.hpp, MoveVertex extensions, per-layer data members, set_layer_data() |
| `src/libslic3r/GCode/GCodeProcessor.cpp` | Speed modifier tracking in store_move_vertex() |
| `src/libslic3r/GCode/ExtrusionProcessor.hpp` | ProcessedPoint curled edge fields |
| `src/libslic3r/GCode.hpp` | Includes SpeedModifier.hpp, LayerResult extension, GCodeOutputStream.write_layer() |
| `src/libslic3r/GCode.cpp` | Modifier recording, pipeline changes, write_layer() implementation |
| `src/libslic3r/GCode/CoolingBuffer.hpp` | Includes SpeedModifier.hpp, updated function signatures |
| `src/libslic3r/GCode/CoolingBuffer.cpp` | G1 counting, cooling modification recording |
| `src/slic3r/GUI/GCodeViewer.cpp` | UI display helpers and tooltip enhancement |

---

## Key Architectural Decisions

### 1. Centralized Type Header (SpeedModifier.hpp)
**Problem:** Circular dependencies when using `std::unordered_map<uint64_t, G1ModifierData>` in GCodeProcessor.hpp
**Solution:** Move all speed modifier types to a standalone header with no project dependencies

### 2. Per-Layer Data Passing vs Shared Pointers
| Aspect | Original Design | Final Implementation |
|--------|-----------------|---------------------|
| Storage | Global shared maps with pointers | Per-layer maps in LayerResult |
| Ownership | Single owner (GCode/CoolingBuffer class) | Moves with layer token through pipeline |
| Access pattern | Concurrent (caused race condition) | Sequential per-layer (thread-safe) |
| Pointer passing | Once at export start | Per-layer via write_layer() |
| Memory | Maps grow throughout export | Per-layer data, cleared after use |

---

## Assumptions

1. G1 counter synchronization relies on all three components counting extrusions identically
2. The TYPE_ADJUSTABLE flag in CoolingBuffer accurately identifies extrusion G1s
3. MoveVertex storage in GCodeProcessor persists for Preview UI access
4. Variable-speed paths call extrude_to_xy() once per processed point

---

## Testing Recommendations

1. Verify G1 counter matches across GCode, CoolingBuffer, and GCodeProcessor
2. Test all modifier types appear correctly in tooltip:
   - FirstLayer on first layer
   - SlowDownLayers on early layers
   - SmallPerimeter on small loops
   - Overhang with varying percentages
   - CurledEdge near curled regions
   - LayerTimeCooling on layers with slow_down_for_layer_cooling active
3. Verify base type labels display correctly for all roles
4. Check memory usage on large prints
5. Verify no tooltip crashes with 0-5 modifiers
6. **Verify no race conditions** - test with parallel pipeline execution

---

## Bug Fixes (Counter Synchronization Issues)

After initial testing showed incorrect modifier data being displayed (wrong modifiers, missing data, wrong speeds), a detailed review of the G1 counter synchronization revealed several bugs:

### Bug 1: CoolingBuffer counted F-only set_speed line

**Problem:** When CoolingBuffer saw the `;_EXTRUDE_SET_SPEED` comment on a `G1 Fxxxx` line (feedrate-only, no extrusion), it incremented its counter and set `g1_count = 1`. But GCode.cpp and GCodeProcessor don't count F-only lines as extrusions.

**Result:** CoolingBuffer counter was ahead by 1 per extrusion segment, causing cooling modification data to be recorded for wrong G1 IDs.

**Fix:** In `CoolingBuffer.cpp` at the `;_EXTRUDE_SET_SPEED` handling:
- Removed `++m_g1_extrusion_counter`
- Changed `line.g1_id_start = m_g1_extrusion_counter + 1` (anticipate first G1)
- Changed `line.g1_count = 0` (no G1s yet)

### Bug 2: CoolingBuffer counted non-extrusion G1s

**Problem:** CoolingBuffer counted ALL G1/G2/G3 lines inside `;_EXTRUDE_SET_SPEED` blocks, including those without E movement (force_no_extrusion paths). GCode.cpp and GCodeProcessor only count G1s with actual extrusion (E > 0).

**Result:** Counter mismatch when paths contained force_no_extrusion G1s.

**Fix:** In `CoolingBuffer.cpp` at the G1 accumulation loop:
- Added check `if (dif[3] > 0.f)` before incrementing counter and g1_count
- This ensures only actual extrusions are counted

### Bug 3: Variable speed path missing force_no_extrusion check

**Problem:** The variable speed path at line 6210 unconditionally incremented the counter without checking `!path.is_force_no_extrusion()`. Non-variable paths all had this check.

**Result:** Potential counter mismatch for force_no_extrusion variable speed paths (rare case).

**Fix:** In `GCode.cpp` variable speed loop:
- Added `if (!path.is_force_no_extrusion())` around counter increment
- Also passed `path.is_force_no_extrusion()` to `extrude_to_xy()` and `extrude_to_xyz()` calls for consistency

### Bug 4: CoolingBuffer didn't count non-adjustable extrusions

**Problem:** CoolingBuffer only counted G1 E+ lines inside TYPE_ADJUSTABLE segments (controlled by `active_speed_modifier`). When `dont_slow_down_outer_wall` is enabled for external perimeters, those segments are NOT marked as adjustable, so their G1s were never counted. GCode.cpp counts all extrusions regardless of adjustable status.

**Result:** Counter mismatch for external perimeters with "don't slow down outer wall" setting enabled.

**Fix:** In `CoolingBuffer.cpp`:
- Added `in_extrusion_block` flag to track when we're between `;_EXTRUDE_SET_SPEED` and `;_EXTRUDE_END`
- Added else clause after the adjustable segment accumulation to count G1/G2/G3 E+ lines that are inside an extrusion block but not in an adjustable segment

### Files Modified for Bug Fixes

| File | Changes |
|------|---------|
| `src/libslic3r/GCode/CoolingBuffer.cpp` | Fixed F-only line counting, added E > 0 check, added in_extrusion_block tracking, count non-adjustable extrusions |
| `src/libslic3r/GCode.cpp` | Added force_no_extrusion check to variable speed path |

### Counter Synchronization Summary (Final Implementation)

**Key insight**: Use per-layer counter (reset at layer start) to avoid TBB parallel pipeline drift.

All three components count G1/G2/G3 with E > 0 (actual extrusions):
- **GCode.cpp**: Reset counter at layer start in `process_layer()`, count after each `extrude_to_xy()`/`extrude_to_xyz()`/`extrude_arc_to_xy()` call only when `!path.is_force_no_extrusion()`
- **CoolingBuffer**: Reset counter at layer start when `flush == true`, count G1/G2/G3 when `dif[3] > 0.f` (E > 0)
- **GCodeProcessor**: Reset counter in `set_layer_data()` (called per-layer), count when `type == EMoveType::Extrude` (requires E > 0)

---

## Bug Fix 5: Per-Layer Counter Reset (Final Fix)

### Problem
Even with all the previous fixes, counters were still drifting apart across layers due to TBB parallel pipeline. With `serial_in_order` filters, each layer is processed sequentially within a stage, but cumulative counters across layers could drift if any mismatch occurred.

### Solution: Per-Layer Counter
Reset counter to 0 at the start of each layer in all three components:

1. **GCode.cpp** (`process_layer()`):
   ```cpp
   // Speed modifier tracking: reset counter at start of each layer
   m_g1_extrusion_counter = 0;
   m_g1_modifier_data.clear();
   ```

2. **CoolingBuffer.cpp** (`process_layer()`):
   ```cpp
   if (flush) {
       // Speed modifier tracking: reset counter at start of each layer
       m_g1_extrusion_counter = 0;
       ...
   }
   ```

3. **GCodeProcessor.hpp** (`set_layer_data()`):
   ```cpp
   void set_layer_data(...) {
       // Reset counter at layer start - per-layer counter for synchronization
       m_g1_extrusion_counter = 0;
       ...
   }
   ```

### Simplified Counting Logic

Removed complex `in_extrusion_block` tracking in CoolingBuffer. Now all components use the same simple condition:

- **Count**: G1/G2/G3 with E > 0
- **Skip**: Travel moves, retracts, F-only lines, force_no_extrusion paths

---

## Bug Fix 6: CurledEdge Showing When It Didn't Limit Speed

### Problem
CurledEdge modifier was displayed whenever curled edge influence was considered, even if overhang had already slowed the speed more than curled edge would have.

### Solution
**File:** `src/libslic3r/GCode/ExtrusionProcessor.hpp` (lines 456-461)

Changed the condition from "curled edge influence exists" to "curled edge actually reduced speed":
```cpp
// Before: set true whenever there's curled edge influence
if (artificial_distance_to_curled_lines > 0.001f) {
    has_curled_slowdown = true;
}

// After: only set true when curled edge actually limits speed
if (curled_speed < extrusion_speed && artificial_distance_to_curled_lines > 0.001f) {
    has_curled_slowdown = true;
    curled_height_factor = artificial_distance_to_curled_lines;
}
extrusion_speed = std::min(curled_speed, extrusion_speed);
```

---

## UI Improvements

### File: `src/slic3r/GUI/GCodeViewer.cpp`

**Tooltip formatting for Feedrate view:**
- Requested Speed line centered in tooltip
- Single empty line between XYZ coordinates and Requested Speed
- No empty line between Requested Speed and modifier chain
- Base/modifier labels use highlight color (same as "Requested Speed:") instead of bold
- Arrow changed from "→" to ">" for compatibility
- Special base types (Bridge, Gap Fill, etc.) display as "Bridge: 600" instead of "Base: 600 (Bridge)"

**Example output:**
```
X: 100.000    Y: 200.000    Z: 0.200

           Requested Speed: 219
Base: 600
> Volumetric (-333): 267
> Overhang (6%): 219
```

---

## Bug Fix 7: GCodeViewer Crash on Preview Open

### Problem
Application crashed with "Access violation" when opening preview after slicing. The crash occurred at `GCodeViewer::update_moves_slider` line 2197 when accessing `view.gcode_ids[i]`.

### Root Cause
The `gcode_ids` vector is populated by iterating through moves and filtering out Seam types (size = moves - seams). However, `endpoints.last` (a segment ID) could occasionally exceed `gcode_ids.size()`, causing out-of-bounds access.

### Solution
**File:** `src/slic3r/GUI/GCodeViewer.cpp`

Added bounds checks at two locations:

1. **Line 834** (SequentialView::render):
   ```cpp
   // Before:
   if (has_render_path)
       gcode_window.render(..., gcode_ids[current.last]);

   // After:
   if (has_render_path && current.last < gcode_ids.size())
       gcode_window.render(..., gcode_ids[current.last]);
   ```

2. **Line 2197-2199** (update_moves_slider):
   ```cpp
   // Before:
   if (view.gcode_ids[i] > 0) alternate_values[count] = ...;

   // After:
   if (i < view.gcode_ids.size() && view.gcode_ids[i] > 0)
       alternate_values[count] = ...;
   ```

Note: Line 2306 already had proper bounds checking for this vector.

---

## Summary

All 8 implementation phases completed, plus:
1. Major refactor to fix TBB pipeline race condition (per-layer data passing)
2. Creation of SpeedModifier.hpp to resolve circular dependencies
3. Bug fixes for G1 counter synchronization issues (5 bugs fixed):
   - Bug 1: CoolingBuffer counted F-only set_speed line
   - Bug 2: CoolingBuffer counted non-extrusion G1s (E <= 0)
   - Bug 3: Variable speed path missing force_no_extrusion check
   - Bug 4: CoolingBuffer didn't count non-adjustable extrusions (dont_slow_down_outer_wall)
   - Bug 5: Counter drift across layers (fixed with per-layer reset)
4. Bug fix 6: CurledEdge modifier only shown when it actually limits speed
5. UI improvements: centered speed display, proper color formatting, cleaner layout
6. Bug fix 7: GCodeViewer crash due to out-of-bounds gcode_ids access

Feature is ready for testing after successful compilation.
