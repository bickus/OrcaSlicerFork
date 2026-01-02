# Bridge G-code Generation

How bridge extrusions are processed during G-code generation.

## Key Files

| File | Purpose |
|------|---------|
| `src/libslic3r/GCode.cpp` | Main G-code generation, speed/accel/flow handling |
| `src/libslic3r/GCode/CoolingBuffer.cpp` | Fan speed marker processing |
| `src/libslic3r/ExtrusionEntity.hpp` | Bridge extrusion role definitions |

## Extrusion Roles

```cpp
// ExtrusionEntity.hpp
erBridgeInfill              // External bridge infill
erExtraBridgeInfill         // Extra bridge infill
erInternalBridgeInfill      // Internal bridge infill
erExtraInternalBridgeInfill // Extra internal bridge infill
erOverhangPerimeter         // Bridge/overhang perimeters
```

### Helper Functions

```cpp
inline bool is_bridge(ExtrusionRole role) {
    return role == erBridgeInfill
        || role == erInternalBridgeInfill
        || role == erExtraBridgeInfill
        || role == erExtraInternalBridgeInfill
        || role == erOverhangPerimeter;
}

inline bool is_bridge_infill(ExtrusionRole role); // Excludes overhang perimeter
```

## Speed Handling

### GCode.cpp (~line 5482-5490)

**Internal Bridges**:
```cpp
else if(path.role() == erInternalBridgeInfill || path.role() == erExtraInternalBridgeInfill) {
    speed = m_config.get_abs_value("internal_bridge_speed");
    m_current_g1_modifiers.base_type = BaseSpeedType::InternalBridge;
}
```

**External Bridges**:
```cpp
else if (path.role() == erSupportTransition || path.role() == erBridgeInfill || path.role() == erExtraBridgeInfill) {
    speed = m_config.get_abs_value("bridge_speed");
    m_current_g1_modifiers.base_type = BaseSpeedType::Bridge;
}
```

**Overhang Perimeters**:
```cpp
else if (path.role() == erOverhangPerimeter) {
    speed = m_config.get_abs_value("bridge_speed");
    m_current_g1_modifiers.base_type = BaseSpeedType::OverhangBridge;
}
```

## Flow Rate Handling

### GCode.cpp (~line 5443-5450)

```cpp
if (path.role() == erTopSolidInfill)
    _mm3_per_mm *= m_config.top_solid_infill_flow_ratio;
else if (path.role() == erBottomSurface)
    _mm3_per_mm *= m_config.bottom_solid_infill_flow_ratio;
else if (path.role() == erInternalBridgeInfill || path.role() == erExtraInternalBridgeInfill)
    _mm3_per_mm *= m_config.internal_bridge_flow;
```

## Acceleration Handling

### GCode.cpp (~line 5369-5381)

```cpp
double bridge_acc = 0.0;
if (is_bridge(path.role())) {
    const double external_bridge_acc = m_config.get_abs_value("bridge_acceleration");
    if (path.role() == erInternalBridgeInfill || path.role() == erExtraInternalBridgeInfill) {
        const double internal_bridge_acc = m_config.get_abs_value("internal_bridge_acceleration");
        bridge_acc = internal_bridge_acc > 0.0 ? internal_bridge_acc : external_bridge_acc;
    } else {
        bridge_acc = external_bridge_acc;
    }
}

if (bridge_acc > 0.0)
    acceleration = bridge_acc;
```

## Jerk Handling

### GCode.cpp (~line 5404-5414)

```cpp
double bridge_jerk = 0.0;
if (is_bridge(path.role())) {
    if (path.role() == erInternalBridgeInfill || path.role() == erExtraInternalBridgeInfill) {
        bridge_jerk = (m_config.internal_bridge_jerk.value > 0.0) ?
            m_config.internal_bridge_jerk.value : m_config.infill_jerk.value;
    } else {
        bridge_jerk = (m_config.bridge_jerk.value > 0.0) ?
            m_config.bridge_jerk.value : m_config.infill_jerk.value;
    }
}

if (bridge_jerk > 0.0)
    jerk = bridge_jerk;
```

## Fan/Cooling Control

### Marker Types (CoolingBuffer.cpp ~line 51-74)

| Marker | Purpose |
|--------|---------|
| `TYPE_OVERHANG_FAN_START` / `_END` | External bridge fan control |
| `TYPE_INTERNAL_BRIDGE_FAN_START` / `_END` | Internal bridge fan control (ORCA feature) |

### Overhang Detection (GCode.cpp ~line 5865-5929)

```cpp
auto check_overhang_fan = [&overhang_fan_threshold](float overlap, ExtrusionRole role) {
    if (role == erBridgeInfill || role == erOverhangPerimeter) {
        return true;  // Always enable for bridges
    }
    // Check overlap thresholds for overhang detection
    switch (overhang_fan_threshold) {
    case Overhang_threshold_1_4: return overlap <= 0.9f;
    case Overhang_threshold_2_4: return overlap <= 0.75f;
    case Overhang_threshold_3_4: return overlap <= 0.5f;
    case Overhang_threshold_4_4: return overlap <= 0.25f;
    case Overhang_threshold_bridge: return overlap <= 0.05f;
    }
};
```

### Marker Insertion (GCode.cpp ~line 5901-5919)

```cpp
auto append_role_based_fan_marker = [this, &gcode](const ExtrusionRole role,
                                                    const std::string_view& marker_prefix,
                                                    const bool fan_on) {
    if (fan_on) {
        if (!m_is_role_based_fan_on[role]) {
            gcode += ";" + marker_prefix + "_FAN_START\n";
            m_is_role_based_fan_on[role] = true;
        }
    } else {
        if (m_is_role_based_fan_on[role]) {
            gcode += ";" + marker_prefix + "_FAN_END\n";
            m_is_role_based_fan_on[role] = false;
        }
    }
};
```

### CoolingBuffer Processing (~line 749-826)

Two separate fan configurations:
- `overhang_fan_control` / `overhang_fan_speed` - External bridges
- `internal_bridge_fan_control` / `internal_bridge_fan_speed` - Internal bridges

Internal bridge fan defaults to `-1` (falls back to overhang settings).

## Bridge Infill Ordering

### GCode.cpp (~line 5121-5151)

```cpp
if (m_config.bridge_infill_at_end && !ironing) {
    ExtrusionEntitiesPtr non_bridge_extrusions;
    ExtrusionEntitiesPtr bridge_extrusions;

    for (ExtrusionEntity *ee : extrusions) {
        if (is_bridge_infill(ee->role()))
            bridge_extrusions.emplace_back(ee);
        else
            non_bridge_extrusions.emplace_back(ee);
    }

    // Print non-bridge first
    if (!non_bridge_extrusions.empty()) {
        chain_and_reorder_extrusion_entities(non_bridge_extrusions, &m_last_pos);
        extrude_entities(non_bridge_extrusions);
    }

    // Print bridge last
    if (!bridge_extrusions.empty()) {
        chain_and_reorder_extrusion_entities(bridge_extrusions, &m_last_pos);
        extrude_entities(bridge_extrusions);
    }
}
```

## Pressure Advance for Bridges

### GCode.cpp (~line 5949-5973)

Bridges are flagged in PA change tags:

```cpp
sprintf(buf, ";%sT%u MM3MM:%g ACCEL:%u BR:%d RC:%d OV:%d\n",
        GCodeProcessor::reserved_tag(GCodeProcessor::ETags::PA_Change).c_str(),
        m_writer.extruder()->id(),
        _mm3_per_mm,
        acceleration_i,
        ((path.role() == erBridgeInfill) || (path.role() == erOverhangPerimeter)),  // BR flag
        1,
        1);
```

The `BR:1` flag allows adaptive PA to treat bridges differently.

Related setting: `adaptive_pressure_advance_bridges` - Fixed PA value for bridges.

## Speed Modifier Tracking

Bridges are tagged with speed modifiers for G-code preview:

| Modifier | Role |
|----------|------|
| `BaseSpeedType::Bridge` | External bridges |
| `BaseSpeedType::InternalBridge` | Internal bridges |
| `BaseSpeedType::OverhangBridge` | Overhang perimeters |

Stored in `m_current_g1_modifiers` for visualization.

## Processing Summary

1. **Identification** - Separate by ExtrusionRole
2. **Speed** - Dedicated `bridge_speed` / `internal_bridge_speed`
3. **Flow** - Role-specific flow ratios applied
4. **Acceleration/Jerk** - Separate settings for motion control
5. **Cooling** - Fan markers processed by CoolingBuffer
6. **Ordering** - Optional: print bridges last in layer
7. **Tracking** - Speed modifiers for preview
8. **PA** - Flagged for adaptive pressure advance
