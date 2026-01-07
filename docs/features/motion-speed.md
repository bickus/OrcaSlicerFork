# Motion & Speed Features

Speed control, acceleration, motion optimization, and performance tuning.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Line-Type-Specific Jerk | medium | Motion/Jerk | Different jerk values for different line types (walls, infill, travel). | v1.2.5 | - | - |
| Manual PA Override | small | Motion/PA | Manual pressure/linear advance override per filament type. | v1.2.5 | - | - |
| Travel Acceleration | small | Motion/Acceleration | Separate acceleration setting for travel moves. | v1.2.5 | - | - |
| Overhang Slowdown Algorithm | large | Motion/Overhang | PrusaSlicer 2.6 slowdown algorithm for both classic and Arachne engines. | v1.4.5 | v2.2.0-beta | - |
| First Layer Travel Speed | small | Motion/FirstLayer | Separate travel speed control for first layer movements. | v1.5.0 | - | - |
| Sparse/Internal Solid Infill Acceleration | small | Motion/Acceleration | Separate acceleration controls for sparse and internal solid infill. | v1.5.0 | - | - |
| Skirt Speed Override | small | Motion/Speed | Option to override speed specifically for skirt printing. | v1.6.4-beta3 | - | - |
| Curled Edge Detection | medium | Motion/Overhang | Detection of curled edges to enhance slowdown for overhangs algorithm with fan speed control. | v1.8.0-beta | v1.9.0-alpha | [#3034](https://github.com/SoftFever/OrcaSlicer/pull/3034) |
| Pressure Equalizer | medium | Motion/Flow | Ported from PrusaSlicer, balances extrusion pressure for consistent flow. Made less aggressive in v2.0.0. | v1.8.0-beta | v2.0.0-beta | [#4264](https://github.com/SoftFever/OrcaSlicer/pull/4264) |
| Machine Limits G-code Emission | medium | Motion/Firmware | Option to emit machine limits to G-code for firmware validation. | v1.9.0-alpha | - | [#3236](https://github.com/SoftFever/OrcaSlicer/pull/3236) |
| Motion Limit Warnings | medium | Motion/Safety | System checks and warns when speed, acceleration, or jerk exceed machine limits. | v2.0.0-beta | - | [#4403](https://github.com/SoftFever/OrcaSlicer/pull/4403) |
| Outer Wall Slowdown Opt-out | small | Motion/Cooling | Option to opt-out of outer wall slowdown for layer cooling consistency. | v2.1.0-rc | - | - |
| Adaptive Pressure Advance | large | Motion/PA | Automatic PA adjustment based on real-time flow rate and print acceleration. | v2.2.0-beta | - | [#5609](https://github.com/SoftFever/OrcaSlicer/pull/5609) |
| Extrusion Rate Smoothing | medium | Motion/Extrusion | Prevents trivial speed fluctuations for smoother extrusion dynamics. | v2.2.0-beta | - | [#5911](https://github.com/SoftFever/OrcaSlicer/pull/5911) |
| Jerk Per-Axis Maximum | small | Motion/Jerk | Maximum jerk limit per axis for fine-tuned motion control. | v2.2.0-beta | - | [#6252](https://github.com/SoftFever/OrcaSlicer/pull/6252) |
| Slowdown for Curled Perimeters | medium | Motion/Overhang | Enabled by default, slows down on perimeters prone to curling. | v2.2.0-beta | - | - |
| Travel Slope Option | small | Motion/Travel | Option for angled travel moves to reduce stringing and oozing. | v2.2.0-beta | - | [#5873](https://github.com/SoftFever/OrcaSlicer/pull/5873) |
| Junction Deviation Machine Limit | medium | Motion/Marlin | Configure maximum Junction Deviation for Marlin firmware, replacing older jerk parameter. | v2.3.1-alpha | - | [#9234](https://github.com/SoftFever/OrcaSlicer/pull/9234) |
| Resonance Avoidance | medium | Motion/Vibration | Sets speed ranges to skip problematic resonance frequencies, minimizing VFA artifacts. | v2.3.1-alpha | - | [#9403](https://github.com/SoftFever/OrcaSlicer/pull/9403) |
| Per-Modifier Acceleration | medium | Motion/Acceleration | All acceleration settings (outer/inner wall, bridge, travel, infill, etc.) can be edited per-modifier (height range modifier). Allows different accelerations for different Z heights or model regions. | Швидкосрал | - | - |

## Pressure Advance

### Standard PA
- Configured per-filament
- Values override AMS slot values (v1.4.3+)
- Linear enforcement for Bambu machines with manual override option

### Adaptive PA ([#5609](https://github.com/SoftFever/OrcaSlicer/pull/5609))
- Automatically adjusts based on flow rate
- Compensates for acceleration changes
- Reduces blobs and gaps from varying dynamics

## Overhang Speed Control

### Algorithm Options
- Classic overhang slowdown (deprecated in v2.2.0)
- New PrusaSlicer 2.6 algorithm (default)
- Slowdown for curled perimeters (default enabled)

### Settings
- Overhang threshold configuration
- Respects volumetric speed limits (v1.7.0-beta)
- Fan speed integration for cooling

## Klipper Optimization

- Combined acceleration and jerk commands into single SET_VELOCITY_LIMIT calls (v1.8.0-beta)
- Per-object acceleration and jerk adjustment support
- Reduced separate command overhead

## Notes

- Filament minimum print speed decoupled from overhang slowdown ([#3859](https://github.com/SoftFever/OrcaSlicer/pull/3859))
- Overhang slowdown bug causing excessive speed reduction was fixed in v2.1.0-beta
- Maximum Z acceleration fixed for Artillery Sidewinder X1 (v2.0.0)
- Overhang slowdown granularity improved ([#5996](https://github.com/SoftFever/OrcaSlicer/pull/5996))
