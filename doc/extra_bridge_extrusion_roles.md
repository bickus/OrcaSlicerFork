# Extra Bridge Extrusion Roles Implementation

## Overview

This document describes the implementation of `erExtraBridgeInfill` and `erExtraInternalBridgeInfill` extrusion roles to properly distinguish extra bridge layer extrusions from regular bridge extrusions.

## Background

The "Extra bridge layers" feature creates a second layer of bridge above detected bridges to improve bridging reliability. This is controlled by the `enable_extra_bridge_layer` setting.

### Existing Surface Types

Two surface types already exist for extra bridges:
- `stInternalAfterExternalBridge` - Second layer above external bridges
- `stSecondInternalBridge` - Second layer above internal bridges

### Previous Workaround

Previously, these surface types were converted back to their base types (`stBottomBridge` and `stInternalBridge`) before extrusion role assignment. This caused extra bridges to share the same extrusion roles as regular bridges, making them indistinguishable in the Preview pane.

## Implementation

### New Extrusion Roles

Added to `ExtrusionRole` enum:
- `erExtraBridgeInfill` - For extra external bridge layer extrusions
- `erExtraInternalBridgeInfill` - For extra internal bridge layer extrusions

### Files Modified

1. **ExtrusionEntity.hpp** - Added new enum values and updated helper functions (`is_infill()`, `is_solid_infill()`, `is_bridge()`)

2. **ExtrusionEntity.cpp** - Added string conversions for new roles:
   - `erExtraBridgeInfill` → "Extra Bridge"
   - `erExtraInternalBridgeInfill` → "Extra Internal Bridge"

3. **Surface.hpp** - Updated `is_bridge()` to include extra bridge surface types

4. **PrintObject.cpp** - Removed workarounds that converted extra bridge surfaces back to regular bridge surfaces

5. **Fill.cpp** - Updated role assignment to detect extra bridge surface types and assign appropriate roles

6. **GCodeViewer.cpp** - Added colors for new line types (same as Bridge/Internal Bridge for consistency)

7. **GCode.cpp** - Updated speed, acceleration, jerk, and fan handling to support new roles

8. **GCodeProcessor.cpp** - Added parsing support for new role types

9. **PressureEqualizer.cpp** - Updated bridge detection to include new roles

10. **SupportMaterial.cpp** - Updated bridge detection to include new roles

### Preview Pane

Two new line types appear in the Preview pane under "Line Type":
- **Extra Bridge** - Displays `erExtraBridgeInfill` extrusions
- **Extra Internal Bridge** - Displays `erExtraInternalBridgeInfill` extrusions

These are separate from the regular "Bridge" and "Internal Bridge" line types, allowing users to independently toggle visibility of extra bridge layers.

### Color Consistency

Extra bridge line types use the same colors as their non-extra counterparts:
- Extra Bridge: Same color as Bridge
- Extra Internal Bridge: Same color as Internal Bridge

## Behavior

Extra bridges inherit the same settings as regular bridges:
- Speed settings (bridge_speed, internal_bridge_speed)
- Acceleration settings (bridge_acceleration, internal_bridge_acceleration)
- Jerk settings (bridge_jerk, internal_bridge_jerk)
- Fan speed settings (overhang fan for external, internal_bridge_fan_speed for internal)
- Flow settings

This ensures consistent behavior while providing visibility control in the Preview pane.

## Bug Fixes

### Bridge Angle Preservation (LayerRegion.cpp)

The `slices_to_fill_surfaces_clipped()` function was losing the `bridge_angle` property when transferring surfaces from `slices` to `fill_surfaces`. This affected all bridge types.

**Fix**: Modified `slices_to_fill_surfaces_clipped()` to use the surface template (preserving `bridge_angle`) for bridge surface types instead of just passing the surface type.

### Extra External Bridge Created Too Early (PrintObject.cpp)

The `stInternalAfterExternalBridge` surfaces were originally created in `detect_surfaces_type()` which runs BEFORE `process_external_surfaces()`. This caused two problems:
1. The `stBottomBridge` surfaces did not have their `bridge_angle` set yet (it's detected in `process_external_surfaces()`)
2. The surfaces were later dropped by `process_external_surfaces()` which clears and rebuilds `fill_surfaces`

**Fix**: Moved the extra external bridge creation code from `detect_surfaces_type()` to `bridge_over_infill()`, which runs AFTER `process_external_surfaces()`. This ensures:
1. `stBottomBridge` surfaces have their `bridge_angle` properly detected before creating extra bridges
2. The extra bridge surfaces are added directly to `fill_surfaces` after all processing is complete
3. The pattern now matches how `stSecondInternalBridge` is created (also in `bridge_over_infill()`)

### Assertion in process_external_surfaces() (LayerRegion.cpp)

The second version of `process_external_surfaces()` (behind an `#else` preprocessor directive) had an assertion that only allowed `stInternal` and `stInternalSolid` for internal surfaces.

**Fix**: Updated the assertion to include `stInternalAfterExternalBridge` as a valid internal surface type (though with the new implementation, this surface type is created after this function runs).

### is_solid() Missing Extra Bridge Types (Surface.hpp)

The `is_solid()` helper function only included `stInternalBridge`, not `stInternalAfterExternalBridge` or `stSecondInternalBridge`. This caused Fill.cpp to treat extra bridge surfaces as sparse infill instead of solid/bridge infill, resulting in wrong fill patterns being generated.

**Fix**: Updated `is_solid()` to include `stInternalAfterExternalBridge` and `stSecondInternalBridge`.

### Bridge Line Width Not Applied to Extra Bridges (Fill.cpp)

The `bridge_flow_for_role` lambda function and the bridge infill/wall overlap logic only checked for `erBridgeInfill` and `erInternalBridgeInfill`, not for the new extra bridge roles. This caused extra bridges to not respect the Bridge/Internal Bridge line width settings.

**Fix**: Updated both locations in Fill.cpp to include `erExtraBridgeInfill` and `erExtraInternalBridgeInfill`:
- `erExtraBridgeInfill` uses `bridge_infill_line_width` (like `erBridgeInfill`)
- `erExtraInternalBridgeInfill` uses `internal_bridge_infill_line_width` (like `erInternalBridgeInfill`)
