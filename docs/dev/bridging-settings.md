# Bridge Settings Reference

Complete reference of all bridge-related settings in OrcaSlicer.

## Flow & Quality Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `bridge_flow` | coFloat | 1.0 | External bridge flow ratio. Multiplied with filament and object flow. Range: 0-2.0 |
| `internal_bridge_flow` | coFloat | 1.0 | Internal bridge flow ratio. For first layer over sparse infill. Range: 0-2.0 |
| `bridge_density` | coPercent | 100% | External bridge line density. Lower values improve cooling. Range: 10-180% |
| `internal_bridge_density` | coPercent | 100% | Internal bridge line density. Range: 10-180% |
| `thick_bridges` | coBool | false | Thick external bridges - more reliable, longer spans, may look worse |
| `thick_internal_bridges` | coBool | true | Thick internal bridges - recommended enabled |
| `enable_extra_bridge_layer` | coEnum | disabled | Extra bridge layer options: disabled, external_bridge_only, internal_bridge_only, apply_to_all |
| `dont_filter_internal_bridges` | coEnum | disabled | Control internal bridge filtering: disabled, limited, nofilter |
| `counterbore_hole_bridging` | coEnum | none | Bridge counterbore holes: none, partiallybridge, sacrificiallayer |
| `bridge_infill_at_end` | coBool | false | Print all bridge infill last in layer |

## Angle & Direction Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `bridge_angle` | coFloat | 0.0 | External bridge angle override in degrees. 0 = auto-detect |
| `internal_bridge_angle` | coFloat | 0.0 | Internal bridge angle override. 0 = auto-detect |
| `bridge_infill_wall_overlap` | coPercent | 0% | Overlap with walls for better bonding. Range: 0-100% |

## Speed Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `bridge_speed` | coFloat | 25 mm/s | External bridge and overhang perimeter speed |
| `internal_bridge_speed` | coFloatOrPercent | 150% | Internal bridge speed. Percentage of bridge_speed or absolute mm/s |

## Acceleration Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `bridge_acceleration` | coFloatOrPercent | 50% | External bridge acceleration. Percentage of outer_wall_acceleration or mm/s² |
| `internal_bridge_acceleration` | coFloatOrPercent | 0 | Internal bridge acceleration. 0 = inherit from bridge_acceleration |
| `bridge_travel_acceleration` | coFloat | 0 | Travel acceleration before/after bridges. 0 = use normal travel accel |
| `overhang_acceleration` | coFloat | 0 | Overhang region acceleration. 0 = default logic |

## Jerk Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `bridge_jerk` | coFloat | 0 | External bridge jerk. 0 = use solid infill jerk |
| `internal_bridge_jerk` | coFloat | 0 | Internal bridge jerk. 0 = use solid infill jerk |
| `bridge_travel_jerk` | coFloat | 0 | Travel jerk before/after bridges. 0 = use normal travel jerk |

## Line Width Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `bridge_infill_line_width` | coFloatOrPercent | 0 | External bridge line width. 0 = use internal solid infill width |
| `internal_bridge_infill_line_width` | coFloatOrPercent | 0 | Internal bridge line width. 0 = use internal solid infill width |

## Support Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `bridge_no_support` | coBool | false | Don't generate support for bridge areas |
| `max_bridge_length` | coFloat | 10 mm | Max unsupported bridge length. 0 = support all, high = support none |

## Fan/Cooling Settings

| Setting Key | Type | Default | Scope | Description |
|-------------|------|---------|-------|-------------|
| `enable_overhang_bridge_fan` | coBools | true | Filament | Enable fan control for overhangs and bridges |
| `overhang_fan_speed` | coInts | 100% | Filament | Fan speed for external bridges. Range: 0-100% |
| `overhang_fan_threshold` | coEnums | 95% | Filament | Overhang threshold for fan activation |
| `internal_bridge_fan_speed` | coInts | -1 | Filament | Internal bridge fan speed. -1 = use overhang settings |

## Pressure Advance Settings

| Setting Key | Type | Default | Scope | Description |
|-------------|------|---------|-------|-------------|
| `adaptive_pressure_advance_bridges` | coFloats | 0.0 | Filament | PA value for bridges. 0 = disabled. Range: 0-2.0 |

## Related Overhang Settings

| Setting Key | Type | Default | Description |
|-------------|------|---------|-------------|
| `extra_perimeters_on_overhangs` | coBool | false | Extra perimeters over steep overhangs |
| `overhang_reverse` | coBool | false | Reverse extrusion direction on even layers |
| `overhang_reverse_internal_only` | coBool | false | Apply reversal only to internal walls |
| `enable_overhang_speed` | coBool | false | Enable variable speed for overhang degrees |
| `overhang_1_4_speed` | coFloatOrPercent | - | Speed for 10-25% overhang |
| `overhang_2_4_speed` | coFloatOrPercent | - | Speed for 25-50% overhang |
| `overhang_3_4_speed` | coFloatOrPercent | - | Speed for 50-75% overhang |
| `overhang_4_4_speed` | coFloatOrPercent | - | Speed for 75-100% overhang |

## GUI Organization (Tab.cpp)

### Quality Tab
- **Bridging section** (~line 2210): flow, density, thick bridges, extra layers, filtering
- **Line widths section** (~line 2132): bridge line widths
- **Strength section** (~line 2278): wall overlap, angles

### Speed Tab
- **Bridge Speed section** (~line 2321): speeds
- **Acceleration section** (~line 2333): accelerations
- **Jerk section** (~line 2350): jerks

### Support Tab
- **Advanced section** (~line 2412): bridge_no_support, max_bridge_length

### Filament Tab
- **Cooling section** (~line 3618): fan settings
- **Pressure Advance section** (~line 3483): adaptive_pressure_advance_bridges

## Config Classes

Most bridge settings belong to `PrintConfig` (process settings).

Fan and PA settings belong to `GCodeConfig` with filament scope.

## Adding a New Bridge Setting

1. Define in `PrintConfig.cpp` with bounds and defaults
2. Declare in `PrintConfig.hpp` in appropriate config class
3. Add to `Preset.cpp` in relevant options array
4. Add GUI control in `Tab.cpp` in appropriate section
5. Use in processing code (GCode.cpp, Fill.cpp, etc.)
