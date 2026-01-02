# Bridging System Overview

This document provides a high-level overview of OrcaSlicer's bridging functionality. For detailed information on specific aspects, see the linked subdocuments.

## Quick Reference

| Topic | Document | When to Read |
|-------|----------|--------------|
| All bridge settings | [bridging-settings.md](bridging-settings.md) | Adding/modifying bridge settings, understanding config options |
| Detection algorithm | [bridging-detection.md](bridging-detection.md) | Understanding how bridges are identified, modifying detection logic |
| Fill patterns | [bridging-fill.md](bridging-fill.md) | Working on bridge infill, flow calculations, pattern selection |
| G-code generation | [bridging-gcode.md](bridging-gcode.md) | Speed/accel/fan handling, extrusion processing |

## Architecture Overview

### Surface Types

OrcaSlicer distinguishes between multiple bridge surface types:

| Surface Type | Description |
|--------------|-------------|
| `stBottomBridge` | External bridge - bottom unsupported surface |
| `stInternalBridge` | 1st layer of dense infill over sparse infill |
| `stSecondInternalBridge` | 2nd layer of dense infill over sparse infill |
| `stInternalAfterExternalBridge` | Surface above an external bridge |

### Extrusion Roles

Bridge extrusions use dedicated roles for independent control:

| Role | Description |
|------|-------------|
| `erBridgeInfill` | External bridge infill |
| `erInternalBridgeInfill` | Internal bridge infill |
| `erExtraBridgeInfill` | Extra layer after external bridge |
| `erExtraInternalBridgeInfill` | Extra layer after internal bridge |
| `erOverhangPerimeter` | Bridge/overhang perimeters |

### Key Files

| Purpose | File |
|---------|------|
| Surface types | `src/libslic3r/Surface.hpp` |
| Extrusion roles | `src/libslic3r/ExtrusionEntity.hpp` |
| Bridge detection | `src/libslic3r/BridgeDetector.cpp/.hpp` |
| Configuration | `src/libslic3r/PrintConfig.cpp/.hpp` |
| Fill processing | `src/libslic3r/Fill/Fill.cpp` |
| G-code generation | `src/libslic3r/GCode.cpp` |
| Cooling/fan control | `src/libslic3r/GCode/CoolingBuffer.cpp` |
| Layer processing | `src/libslic3r/LayerRegion.cpp` |
| GUI settings | `src/slic3r/GUI/Tab.cpp` |

## Processing Pipeline

```
1. Slicing & Surface Detection (PrintObject.cpp)
   └─> Surfaces marked as stBottomBridge, stInternalBridge, etc.

2. Bridge Detection (BridgeDetector.cpp, LayerRegion.cpp)
   └─> Optimal bridge angle calculated for each region
   └─> Anchor regions identified

3. Fill Generation (Fill/Fill.cpp)
   └─> Bridge-specific flow and spacing applied
   └─> Lines oriented to detected angle

4. G-code Generation (GCode.cpp)
   └─> Bridge speed/accel/jerk applied
   └─> Fan markers inserted
   └─> Optional: bridges printed last

5. Post-processing (CoolingBuffer.cpp)
   └─> Fan speed commands inserted based on markers
```

## Internal vs External Bridges

OrcaSlicer provides independent control for internal and external bridges:

| Parameter | External | Internal |
|-----------|----------|----------|
| Speed | `bridge_speed` | `internal_bridge_speed` |
| Flow | `bridge_flow` | `internal_bridge_flow` |
| Acceleration | `bridge_acceleration` | `internal_bridge_acceleration` |
| Jerk | `bridge_jerk` | `internal_bridge_jerk` |
| Fan speed | `overhang_fan_speed` | `internal_bridge_fan_speed` |
| Density | `bridge_density` | `internal_bridge_density` |
| Line width | `bridge_infill_line_width` | `internal_bridge_infill_line_width` |
| Angle | `bridge_angle` | `internal_bridge_angle` |

## Helper Functions

Key functions for identifying bridges in code:

```cpp
// From ExtrusionEntity.hpp
bool is_bridge(ExtrusionRole role);        // All bridge roles including overhang
bool is_bridge_infill(ExtrusionRole role); // Bridge infill only (not overhang perimeter)
```

## Common Modifications

### Adding a new bridge setting
See [bridging-settings.md](bridging-settings.md) for the full list and patterns.

### Modifying bridge detection
See [bridging-detection.md](bridging-detection.md) for algorithm details.

### Changing bridge fill behavior
See [bridging-fill.md](bridging-fill.md) for flow and pattern handling.

### Adjusting G-code output
See [bridging-gcode.md](bridging-gcode.md) for speed/accel/fan processing.
