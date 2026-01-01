# Steep Slope Infill Issue Research

## Problem Description

When printing tapered surfaces (e.g., a tube with walls at 60 degrees from vertical), inner walls can end up being printed over sparse infill instead of solid infill. This causes:

1. Inner walls printed at full speed over nearly-empty sparse infill
2. Poor adhesion and tearing of the wall material
3. Visible defects in the final print

The expected behavior: The slicer should detect that inner walls on steep slopes need solid foundation and generate internal bridge/solid infill layers progressively, similar to how it handles normal top surfaces.

## Root Cause Analysis

### Issue #1: Solid Infill Detection (PRIMARY ISSUE)

**Location**: `src/libslic3r/PrintObject.cpp:detect_surfaces_type()` (lines 1274-1512)

Top/bottom surfaces are detected by calculating geometric DIFFERENCE between adjacent layers:
- **Top surface** = `current_layer_slices - upper_layer_slices` (what's exposed above)
- **Bottom surface** = `current_layer_slices - lower_layer_slices` (what's exposed below)

**The Problem**: On a 60-degree tapered tube, the inner wall area is technically "covered" by the layer above (just shifted inward), so it's NOT marked as a top surface. It stays as `stInternal` (sparse infill) instead of `stInternalSolid`.

**Key Code** (PrintObject.cpp:1343-1346):
```cpp
// Find top surfaces
Polygons upper_slices = next_layer->lslices;
surfaces_append(top,
    offset2_ex(diff(layerm_slices_surfaces, upper_slices, ApplySafetyOffset::Yes), ...),
    stPosTop | stDensSolid);
```

This only marks surfaces as "top" if they extend beyond the upper layer boundary.

### Issue #2: Ensure Vertical Thickness Limitation

**Location**: `src/libslic3r/PrintObject.cpp:discover_vertical_shells()` (lines 1596-2034)

- Projects shells **vertically** downward from detected top surfaces
- On steep slopes, vertical projection doesn't align with actual inner wall location
- Regularization filtering removes narrow regions (< 0.65× infill spacing)
- No slope angle detection - 30° and 75° slopes treated identically

### Issue #3: Overhang Wall Detection Limitation

**Location**: `src/libslic3r/PerimeterGenerator.cpp` (lines 154-204)

The algorithm only checks if a wall extends beyond the geometric boundary of the layer below. It does NOT check whether there's solid material underneath - a wall on 10% sparse infill is treated identically to one on 100% solid infill.

## Focus Area: Issue #1 Fix

The recommended approach is to fix Issue #1 (solid infill detection) because:
- It addresses the root cause (lack of solid foundation)
- Issue #2 (ensure vertical thickness) is considered unreliable
- Issue #3 (overhang detection) wouldn't fully solve the problem - even with bridge speeds, printing over sparse infill is problematic

### Requirements for Fix

1. **Feature-switched**: New checkbox in Process settings to enable/disable
2. **Backward compatible**: Old approach continues to work where it worked well
3. **Slope-aware**: Detect when inner walls are on steep inward-tapering surfaces
4. **Solid foundation**: Generate solid infill layers under walls on steep slopes

## Key Files

- `/src/libslic3r/PrintObject.cpp` - Main surface type detection and shell discovery
- `/src/libslic3r/PrintConfig.cpp` - Setting definitions
- `/src/libslic3r/PrintConfig.hpp` - Setting declarations
- `/src/libslic3r/LayerRegion.cpp` - Surface processing
- `/src/libslic3r/Surface.hpp` - Surface type definitions

## Surface Types (from Surface.hpp)

```cpp
enum SurfaceType {
    stTop,            // External top surface
    stBottom,         // External bottom surface
    stBottomBridge,   // Bottom surface over void (bridge)
    stInternal,       // Internal sparse infill
    stInternalSolid,  // Internal solid infill
    stInternalBridge, // Internal bridge
    stInternalVoid,   // Internal void
    // ... additional types
};
```

## Related Settings

- `top_shell_layers` - Number of solid layers at top
- `bottom_shell_layers` - Number of solid layers at bottom
- `ensure_vertical_shell_thickness` - Existing vertical shell enforcement
- `sparse_infill_density` - Infill percentage
- `dont_filter_internal_bridges` - Controls internal bridge filtering (similar problem domain)

## Existing Similar Feature: `dont_filter_internal_bridges`

**Location**: `PrintConfig.cpp:1554-1580`, `PrintObject.cpp:2056-2256`

This setting addresses a similar but different problem:
- It handles `stInternalSolid` surfaces that sit over sparse infill
- The `bridge_over_infill()` function creates internal bridges for these cases

**Key difference from our issue**: The user's problem occurs because the steep slope region is never marked as `stInternalSolid` in the first place. The geometric detection in `detect_surfaces_type()` doesn't catch it.

## Implementation Approach

### Core Idea: Detect Steep Inward Slopes During Surface Type Detection

In `detect_surfaces_type()`, after calculating the standard geometric difference for top surfaces:

1. **Calculate slope regions**: Compare perimeter boundaries between current and upper layer
2. **Identify steep inward slopes**: Where the upper layer boundary has shrunk significantly
3. **Mark for solid treatment**: These regions should be treated as "top-like" and get solid infill

### Algorithm Concept

```
steep_slope_regions = current_layer_boundary - offset(upper_layer_boundary, threshold_distance)
```

Where `threshold_distance` is calculated from:
- Layer height
- Configured slope angle threshold (e.g., 50 degrees from horizontal = 40 from vertical)

For a 60-degree slope from vertical with 0.2mm layer height:
- Horizontal shift per layer = 0.2 / tan(60°) ≈ 0.35mm inward
- This creates regions where inner walls lose their foundation rapidly

### New Settings Required

Two settings will be added:

1. **`solid_infill_under_steep_slopes`** (enum: disabled/enabled)
   - Main toggle for the feature
   - Default: disabled (backward compatibility)
   - Location: Strength section in Process settings

2. **`steep_slope_angle_threshold`** (float: 30-85 degrees)
   - Configurable angle from vertical
   - Default: 60 degrees
   - Only visible when main setting is enabled

### Implementation Location

The detection will be added to `detect_surfaces_type()` in PrintObject.cpp, after the existing top surface detection (line ~1353). Detected steep slope regions will be marked as `stTop` surfaces, which will automatically:
- Get solid infill treatment
- Propagate according to `top_shell_layers` setting
- Work with existing bridge detection

## Implementation Complete

The feature has been implemented in the following files:

### Files Modified

1. **`src/libslic3r/PrintConfig.hpp`**
   - Added `SteepSlopeInfillMode` enum (line ~210)
   - Added config options to `PrintObjectConfig` (lines ~883-884)

2. **`src/libslic3r/PrintConfig.cpp`**
   - Added enum value map for `SteepSlopeInfillMode` (lines ~314-319)
   - Added setting definitions for `solid_infill_under_steep_slopes` and `steep_slope_angle_threshold` (lines ~1582-1614)

3. **`src/libslic3r/Preset.cpp`**
   - Added settings to preset serialization list (line ~815)

4. **`src/libslic3r/PrintObject.cpp`**
   - Core detection algorithm in `detect_surfaces_type()` (lines ~1355-1393)
   - Step invalidation triggers (lines ~1085-1086)

5. **`src/slic3r/GUI/Tab.cpp`**
   - GUI controls in Strength > Top/bottom shells section (lines ~2250-2251)

6. **`src/slic3r/GUI/ConfigManipulation.cpp`**
   - Visibility condition for angle threshold (lines ~855-857)

### Algorithm Summary

The steep slope detection is now integrated into the shell discovery functions, NOT in `detect_surfaces_type()`. This ensures we don't mark the steep slope layer itself as `stTop`, but instead create solid shells on the layers BELOW.

**Detection locations:**
1. `discover_vertical_shells()` - both multi-material and single-material branches
2. `discover_horizontal_shells()` - for when `ensure_vertical_shell_thickness != evstAll`

**Algorithm:**
1. Calculates threshold offset: `offset = layer_height * tan(angle)`
2. Expands upper layer boundary by threshold offset
3. Finds regions of current layer NOT covered by expanded upper boundary
4. Expands these regions inward by perimeter band width to cover inner wall areas
5. Adds these regions to the shell projection surfaces (NOT marking current layer as stTop)
6. Shell discovery projects these surfaces downward, creating `stInternalSolid` on layers below
7. `bridge_over_infill()` then adds internal bridges where those solid layers sit over sparse infill

**Key insight**: The steep slope region on layer N triggers solid shell creation on layers N-1, N-2, etc. WITHOUT marking layer N itself as `stTop`. This means:
- Layer N keeps its original surface types (inner/outer walls print normally)
- Layer N-1 becomes `stInternalSolid` to support layer N's inner walls
- `bridge_over_infill()` adds internal bridges to layer N-1 if layer N-2 is sparse

**Important**: The threshold angle should be set LOWER than the model's actual slope angle to trigger detection. For example, if a model has 60° slopes, set the threshold to 50° or less.

### Default Values

- `solid_infill_under_steep_slopes`: Disabled (for backward compatibility)
- `steep_slope_angle_threshold`: 50° (catches slopes of ~55-60° and steeper)

### Files Modified

1. `discover_vertical_shells()` in PrintObject.cpp (~lines 1659-1685, 1728-1764)
   - Adds steep slope regions to shell projection for solid infill creation
2. `discover_horizontal_shells()` in PrintObject.cpp (~lines 3615-3641)
   - Same as above, for when ensure_vertical_shell_thickness != evstAll
3. `bridge_over_infill()` in PrintObject.cpp (~lines 2245-2291)
   - Direct bridge creation on layer N-1 to support layer N's steep slope inner walls
   - Creates bridges from stInternal surfaces that are over sparse infill
   - Expands for anchor points (4x spacing) like regular bridging

### Two-Pronged Approach

The implementation uses two complementary mechanisms:

1. **Shell propagation** (discover_vertical/horizontal_shells):
   - Creates stInternalSolid on layers below steep slopes
   - Works with top_shell_layers to create multiple solid layers

2. **Direct bridging** (bridge_over_infill):
   - Creates bridges on layer N-1 directly under steep slope inner walls
   - Works even without shell propagation
   - Finds stInternal surfaces over sparse infill and bridges them
