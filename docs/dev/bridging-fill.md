# Bridge Fill Patterns

How bridge infill is generated, including flow calculations and pattern selection.

## Key Files

| File | Purpose |
|------|---------|
| `src/libslic3r/Fill/Fill.cpp` | Bridge fill surface processing |
| `src/libslic3r/Flow.hpp` / `Flow.cpp` | Bridge flow calculations |
| `src/libslic3r/LayerRegion.cpp` | Bridge flow selection |
| `src/libslic3r/Surface.hpp` | Bridge surface types |

## Surface Types

| Type | Description |
|------|-------------|
| `stBottomBridge` | External bridge - bottom unsupported surface |
| `stInternalBridge` | 1st layer dense infill over sparse |
| `stSecondInternalBridge` | 2nd layer dense infill over sparse |
| `stInternalAfterExternalBridge` | Surface above external bridge |

Each surface stores:
```cpp
SurfaceType surface_type;
ExPolygon expolygon;
double bridge_angle;  // Radians, CCW from East; negative = undefined
```

## Bridge Flow Calculation

### Flow Constants

```cpp
#define BRIDGE_EXTRA_SPACING 0.05  // 0.05mm extra for bridge threads
```

### Bridging Flow Creation

```cpp
// Flow.hpp
static Flow bridging_flow(float dmr, float nozzle_diameter) {
    return Flow { dmr, dmr, bridge_extrusion_spacing(dmr), nozzle_diameter, true };
}

static float bridge_extrusion_spacing(float dmr) {
    return dmr + BRIDGE_EXTRA_SPACING;  // dmr + 0.05mm
}
```

### Thick vs Thin Bridges

```cpp
// LayerRegion.cpp
Flow LayerRegion::bridging_flow(FlowRole role, bool thick_bridge) const {
    if (thick_bridge) {
        // Circular cross-section with proper spacing
        bridge_flow = Flow::bridging_flow(
            float(sqrt(region_config.bridge_flow)) * nozzle_diameter,
            nozzle_diameter
        );
    } else {
        // Apply bridge_flow ratio to maintain spacing
        bridge_flow = this->flow(role).with_flow_ratio(region_config.bridge_flow);
    }
    return bridge_flow;
}
```

## Fill.cpp Bridge Processing

### Bridge Flow Selection (~line 835-848)

```cpp
auto bridge_flow_for_role = [&](const LayerRegion &layerm, FlowRole extrusion_role,
                                bool is_thick_bridge, ExtrusionRole extrusion_role_id) -> Flow {
    if (!is_thick_bridge && (extrusion_role_id == erBridgeInfill ||
                             extrusion_role_id == erInternalBridgeInfill ||
                             extrusion_role_id == erExtraBridgeInfill ||
                             extrusion_role_id == erExtraInternalBridgeInfill)) {

        // Select width config based on role
        const ConfigOptionFloatOrPercent *width_opt =
            (extrusion_role_id == erBridgeInfill || extrusion_role_id == erExtraBridgeInfill) ?
                &region_config.bridge_infill_line_width :
                &region_config.internal_bridge_infill_line_width;

        Flow base_flow = Flow::new_from_config_width(extrusion_role, *width_opt, ...);
        return base_flow.with_flow_ratio(region_config.bridge_flow);
    }
    return layerm.bridging_flow(extrusion_role, is_thick_bridge);
};
```

### Bridge Detection (~line 938)

```cpp
params.bridge = is_bridge || Fill::use_bridge_flow(params.pattern);

params.flow = params.bridge ?
    bridge_flow_for_role(layerm, extrusion_role, is_thick_bridge, params.extrusion_role) :
    layerm.flow(extrusion_role, ...);
```

### Bridge Direction (~line 299-311)

```cpp
// _infill_direction method
if (surface->bridge_angle >= 0) {
    // Use bridge angle (detected or user-specified)
    out_angle = float(surface->bridge_angle);
} else if (this->layer_id != size_t(-1)) {
    // Alternate fill direction (if no bridge angle)
    if(!is_using_template_angle)
        out_angle += this->_layer_angle(this->layer_id / surface->thickness_layers);
}
```

### Bridge Wall Overlap (~line 1049-1102)

```cpp
for (SurfaceFill &fill : surface_fills) {
    if (fill.params.extrusion_role == erBridgeInfill ||
        fill.params.extrusion_role == erInternalBridgeInfill ||
        fill.params.extrusion_role == erExtraBridgeInfill ||
        fill.params.extrusion_role == erExtraInternalBridgeInfill) {

        double bridge_overlap_pct = region_config.bridge_infill_wall_overlap.value;

        if (bridge_overlap_pct != 0) {
            double bridge_line_width = region_config.bridge_infill_line_width.get_abs_value(...);
            if (bridge_line_width == 0)
                bridge_line_width = region_config.internal_solid_infill_line_width.get_abs_value(...);

            double bridge_overlap = bridge_line_width * bridge_overlap_pct / 100.0;
        }
    }
}
```

## Pattern Selection

### Which Patterns Use Bridge Flow

Most patterns return `false` for `use_bridge_flow()`:
- FillGyroid, FillTpmsD, FillTpmsFK, Fill3DHoneycomb: **false**
- Line, Rectilinear, Grid, etc.: **false** by default

**Key insight**: Bridge infill is determined by surface type (`is_bridge`), not pattern selection. Pattern only matters if it explicitly declares `use_bridge_flow() = true`.

## Regular vs Bridge Infill Comparison

| Aspect | Regular Infill | Bridge Infill |
|--------|----------------|---------------|
| Flow Shape | Rounded rectangle | Circular (thick) or modified (thin) |
| Spacing | Based on line width | dmr + 0.05mm extra |
| Angle | Rotates per layer | Fixed to detected/configured angle |
| Density | `sparse_infill_density` | `bridge_density` / `internal_bridge_density` |
| Line Width | `sparse_infill_line_width` | `bridge_infill_line_width` (separate) |
| Speed | `sparse_infill_speed` | `bridge_speed` / `internal_bridge_speed` |
| Overlap | `infill_wall_overlap` | `bridge_infill_wall_overlap` (separate) |

## Internal Bridge Detection

In `PrintObject.cpp` (~line 2963-2985):

```cpp
// stInternalSolid surfaces become stInternalBridge if conditions met
for (const Surface *surface : internal_solids) {
    if (cs.original_surface == surface) {
        Surface tmp{*surface, {}};
        tmp.surface_type = stInternalBridge;
        tmp.bridge_angle = cs.bridge_angle;
        new_surfaces.emplace_back(tmp, ep);
    }
}
```

## Configuration Impact

| Setting | Effect on Fill |
|---------|----------------|
| `bridge_flow` | Multiplier on flow rate |
| `internal_bridge_flow` | Separate multiplier for internal |
| `bridge_density` | Line spacing (100% = solid) |
| `thick_bridges` | Circular vs rectangular cross-section |
| `bridge_angle` | Override auto-detected direction |
| `bridge_infill_wall_overlap` | Expansion into wall region |
