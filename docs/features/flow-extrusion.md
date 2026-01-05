# Flow & Extrusion Features

Extrusion control, retraction, and flow compensation features.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Top Surface Flow Rate | small | Flow/Surface | Adjust flow rate specifically for top surfaces. | v1.2 | - | - |
| Bottom Surface Flow Rate | small | Flow/Surface | Adjust flow rate specifically for bottom surfaces. | v1.2.4 | - | - |
| Extra Length on Restart | small | Retraction/Settings | Additional extrusion length when resuming after retraction to compensate for ooze. | v1.4.2 | v1.4.3 | - |
| Firmware Retraction | medium | Retraction/Firmware | Ported from PrusaSlicer, uses firmware-controlled retraction for consistent results. | v1.4.2 | - | - |
| Absolute/Relative Extrusion | small | Extrusion/Mode | Option for absolute or relative extrusion for non-BBL printers. | v1.4.3 | - | - |
| Cura-Style Outer Wall Wipe | small | Retraction/Wipe | Outer wall wipe option similar to Cura's implementation. | v1.4.3 | - | - |
| Role-Based Wipe Speed | small | Retraction/Wipe | Automatic wipe speed adjustment based on current extrusion type. | v1.4.3 | - | - |
| Seam Gap Control | small | Retraction/Quality | Fine-tuning gap at seam location for improved seam quality. | v1.4.3 | - | - |
| Wipe on Loop | small | Retraction/Wipe | Inward wipe movement at the end of loops for cleaner seams. | v1.4.3 | - | - |
| Wipe Speed Adjustment | small | Retraction/Wipe | Manual control over wipe movement speed. | v1.4.3 | - | - |
| Filament Shrinkage Compensation | medium | Flow/Dimensional | Compensates for material shrinkage per filament in XY and Z dimensions. | v1.5.0 | v2.2.0-beta | [#6507](https://github.com/SoftFever/OrcaSlicer/pull/6507) |
| Line Width Percentage | small | Extrusion/Width | Specify line width as percentage of nozzle diameter. | v1.6.4-beta | v1.6.4-beta3 | - |
| Extended Line Width Limit | small | Extrusion/Width | Line width limit increased to 5x nozzle diameter for special applications. | v1.6.4 | - | - |
| Small Area Flow Compensation | large | Flow/Quality | Adjusts extrusion flow inversely proportional to line length in small areas to prevent over-extrusion. | v2.0.0-beta | v2.1.1, v2.2.0-beta | [#3334](https://github.com/SoftFever/OrcaSlicer/pull/3334) |

## Flow Compensation

### Small Area Flow Compensation ([#3334](https://github.com/SoftFever/OrcaSlicer/pull/3334))
- Addresses over-extrusion in small solid infill areas
- Uses color-coding visualization: blue for reduced flow, red for increased
- Fixed regression issues in v2.1.1 and v2.2.0-beta

### Surface-Specific Flow
- **Top Surface**: Separate flow rate for visible top layers
- **Bottom Surface**: Separate flow rate for first layers and bottom surfaces
- Both help achieve optimal surface quality

## Retraction Settings

### Standard Retraction
- Retraction distance
- Retraction speed
- Deretraction speed (can differ from retraction)

### Firmware Retraction
- Uses G10/G11 commands
- Consistent across moves
- Firmware handles timing

### Wipe Options
- Role-based automatic speed
- Manual speed override
- Loop wipe for cleaner seams
- Outer wall specific wipe

## Notes

- Extra length on restart was being ignored when wipe enabled (fixed in v1.9.0-alpha, [#3094](https://github.com/SoftFever/OrcaSlicer/pull/3094))
- Wipe logic enhanced to respect retraction speed (v1.9.0-alpha)
- Small area flow compensation on first layer fixed in v2.2.0-beta
- Filament shrinkage now supports independent XY and Z compensation ([#6507](https://github.com/SoftFever/OrcaSlicer/pull/6507))
- Avoid crossing walls no longer removes retraction wipes ([#6518](https://github.com/SoftFever/OrcaSlicer/pull/6518))
