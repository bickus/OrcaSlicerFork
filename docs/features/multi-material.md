# Multi-Material Features

Multi-tool, MMU, and multi-color printing features.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Multi-Color Pressure Advance | medium | MultiMaterial/Tuning | Pressure advance support for multi-material prints with filament changes. | v1.3.3 | - | - |
| Wipe Tower Improvements | medium | MultiMaterial/Tower | Various improvements including RIB wall for stability, max speed limits, and margin reduction. | v1.4.0 | v1.8.0, v2.1.0-beta, v2.3.1-alpha | [#9881](https://github.com/SoftFever/OrcaSlicer/pull/9881) |
| SEMM Support | xlarge | MultiMaterial/System | Single Extruder Multi-Material support with purge tower/ramming for ERCF, TradRack, MMU compatibility. | v1.7.0-beta | v1.7.0 | - |
| Adaptive Pressure Advance | large | MultiMaterial/Tuning | Automatic PA adjustment based on real-time flow rate and print acceleration. | v2.2.0-beta | - | [#5609](https://github.com/SoftFever/OrcaSlicer/pull/5609) |
| Automatic Tool Preheat | medium | MultiMaterial/ToolChange | Automatic preheat of next tool based on time estimation, supports all firmwares. | v2.2.0-beta | - | - |
| Filament Shrinkage Compensation | medium | MultiMaterial/Quality | Shrinkage compensation per filament for XY and independent Z dimensions. | v2.2.0-beta | - | [#6507](https://github.com/SoftFever/OrcaSlicer/pull/6507) |
| Mixed Line Width Support | medium | MultiMaterial/ToolChange | Support for mixing different line widths using different nozzle diameters in same print. | v2.2.0-beta | - | - |
| MMU Filament Cutter Support | medium | MultiMaterial/Hardware | Support for filament cutters (ERCF v2, Filametrix) with correct flush_length calculation. | v2.2.0-beta | - | [#6241](https://github.com/SoftFever/OrcaSlicer/pull/6241) |
| Multi-Material Interlocking | large | MultiMaterial/Quality | Ported from Cura 5.3, creates interlocking structures at material boundaries for stronger multi-material prints. | v2.2.0-beta | - | [#5775](https://github.com/SoftFever/OrcaSlicer/pull/5775) |
| Multi-Tool Printer Support | xlarge | MultiMaterial/Hardware | Support for tool changers and IDEX systems with extruder assignment, ooze prevention, and automatic preheat. | v2.2.0-beta | - | [#6087](https://github.com/SoftFever/OrcaSlicer/pull/6087) |
| Ooze Prevention | medium | MultiMaterial/Quality | Temperature-based ooze prevention using idle temperature and temperature variation approaches. | v2.2.0-beta | - | - |
| Pellet Printer Support | large | MultiMaterial/Hardware | Support for pellet-based extruders for industrial applications. | v2.2.0-beta | - | [#4836](https://github.com/SoftFever/OrcaSlicer/pull/4836) |
| Filament Color Remapping | medium | MultiMaterial/Workflow | Reassign filament colors for pre-colored models without physically rearranging filaments. | v2.3.1-alpha | - | [#10303](https://github.com/SoftFever/OrcaSlicer/pull/10303) |

## Multi-Tool Systems

### SEMM (Single Extruder Multi-Material)
Supports various MMU-style systems:
- Prusa MMU
- ERCF (Enraged Rabbit Carrot Feeder)
- Annex TradRack
- N-in-1-out extruders

### Tool Changers ([#6087](https://github.com/SoftFever/OrcaSlicer/pull/6087))
Support for tool-changing printers:
- Generic ToolChanger (Klipper-based)
- Prusa XL 5T
- Custom IDEX configurations

## Wipe Tower

### Features
- RIB wall reinforcement for stability ([#9881](https://github.com/SoftFever/OrcaSlicer/pull/9881))
- Maximum purge speed limits (v1.8.0)
- Purge amount optimization for Bambu printers (v1.8.1)
- Preview hiding when no tool changes occur (v1.8.0-beta)

### Improvements
- Wipe tower position fixed for multiple plates (v1.8.0-beta)
- Purge tower display in gcode preview fixed for SEMM (v1.7.0)
- Margin reduction for efficient plate usage (v2.1.0-beta)

## Notes

- SEMM invalid speed generation fixed when cooling tube position is 0 (v1.7.0)
- Filament cutter support requires proper flush_length configuration
- Multi-material interlocking enhances mechanical bonding at color boundaries ([#5775](https://github.com/SoftFever/OrcaSlicer/pull/5775))
- Concentric infill seam staggering improved for multi-material ([#6184](https://github.com/SoftFever/OrcaSlicer/pull/6184))
