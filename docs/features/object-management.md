# Object Management Features

Object handling, plate management, and print order control.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Bed Exclude Area | small | Object/Placement | Parameter exposed for defining excluded areas on the print bed. | v1.4.0 | - | - |
| Layer Time Display | medium | Object/Visualization | Display layer time and layer time (log) for analyzing print duration per layer. | v1.4.0 | v1.8.0-beta | - |
| Plate Naming | medium | Plate/Organization | Name plates with `plate_name` variable for G-code formatting. | v1.6.0 | v2.0.0-beta | - |
| Boolean Operations | medium | Object/Editing | Boolean union, difference, and intersection operations between objects. | v1.6.4 | - | - |
| Fill Bed with Copies | medium | Object/Arrangement | Automatically fill available bed space with copies of selected object. | v1.6.4 | - | - |
| Klipper Exclude Objects | large | Object/Cancellation | Native Klipper "Exclude Objects" support without post-processing scripts. | v1.6.0 | v1.2.5.3, v1.3.1 | - |
| Object Exclusion for BambuLab | medium | Object/Cancellation | Cancel/exclude individual objects during printing on Bambu printers. | v1.6.4 | - | - |
| Cancel Objects (Prusa/Marlin/RRF) | large | Object/Cancellation | Cancel individual objects during printing for Prusa, Marlin, and RepRapFirmware. | v1.9.0-alpha | - | [#3164](https://github.com/SoftFever/OrcaSlicer/pull/3164) |
| Filament Usage Breakdown | medium | Object/Statistics | Shows used filament length and weight per line type in print statistics. | v1.9.0-alpha | - | [#3246](https://github.com/SoftFever/OrcaSlicer/pull/3246) |
| Print Order Control | medium | Object/Order | Adjust print order of objects in "by layer" mode through drag-and-drop in object list. | v2.0.0-beta | - | [#3685](https://github.com/SoftFever/OrcaSlicer/pull/3685) |
| Plate Name Editing | small | Plate/Editing | Edit build plate names directly in the interface. | v2.0.0-beta | - | - |
| Duplicate Current Plate | small | Plate/Management | Copy current plate with all objects for quick duplication. | v2.2.0-beta | - | [#6710](https://github.com/SoftFever/OrcaSlicer/pull/6710) |
| Move Plate to Front | small | Plate/Order | Move any plate to front position using Sokoban-style mechanics. | v2.2.0-beta2 | - | - |
| Per-Object Skirt and Draft Shield | medium | Object/Settings | Apply skirt and draft shield settings to individual objects. | v2.2.0-beta | - | [#6487](https://github.com/SoftFever/OrcaSlicer/pull/6487) |

## Object Cancellation Systems

### Klipper Exclude Objects
- Native support without post-processing (v1.6.0)
- Exclude object labels support (v1.2.5.3)
- Works with standard Klipper configuration

### BambuLab Object Exclusion
- Works with X1/X1C printers (v1.6.4)
- Integrated with printer firmware

### Prusa/Marlin/RRF Cancel Objects ([#3164](https://github.com/SoftFever/OrcaSlicer/pull/3164))
- Added in v1.9.0-alpha
- Video demonstration available in release notes
- Supports multiple firmware types

## Plate Management

### Plate Features
- Naming with G-code variable support
- Duplicate with objects ([#6710](https://github.com/SoftFever/OrcaSlicer/pull/6710))
- Reorder plates
- Per-plate statistics

### Visualization
- Layer time display
- Auto-arrange after cloning
- All plates statistics total cost (v1.8.0-beta)

## Notes

- Bed exclude area limitations removed in v1.4.3
- Object exclusion default status changed to disabled by default (v1.9.0-beta)
- Auto-arrange plate after cloning added in v2.2.0-beta
- Object spacing arrangement regression fixed ([#6741](https://github.com/SoftFever/OrcaSlicer/pull/6741))
