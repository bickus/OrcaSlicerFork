# Calibration Features

Built-in calibration tools for tuning print parameters and optimizing print quality.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Flow Rate Calibration | large | Calibration/Flow | Calibration tool for fine-tuning extrusion multiplier using visual inspection of test patterns. | v1.4.1 | v2.3.1-alpha | [#8993](https://github.com/SoftFever/OrcaSlicer/pull/8993) |
| Pressure Advance Calibration | large | Calibration/PA | PA calibration with line and tower methods, configurable range settings for precise tuning. | v1.4.1 | v1.4.3, v2.0.0, v2.2.0 | - |
| Max Flowrate Test | medium | Calibration/Flow | Volumetric speed test to determine maximum flow rate before under-extrusion occurs. | v1.4.5 | v2.0.0 | - |
| Temperature Tower | medium | Calibration/Temperature | Built-in temperature tower calibration for finding optimal printing temperature for each filament. | v1.4.5 | - | - |
| VFA Test | medium | Calibration/Vibration | Vertical Fine Artifacts test for identifying and reducing surface vibration patterns. | v1.4.5 | v2.3.1-alpha | - |
| Orca Tolerance Test | medium | Calibration/Dimensional | Dimensional accuracy verification test for checking printer tolerance and calibration. | v1.6.3-beta | - | - |
| PA Pattern Calibration (Ellis) | medium | Calibration/PA | Ellis pattern pressure advance calibration method with minimum 100 mm/s speeds. | v1.6.4-beta | v2.2.0 | - |
| Retraction Testing | medium | Calibration/Retraction | Built-in retraction calibration for finding optimal retraction distance and speed. | v1.6.3-beta | - | - |
| Input Shaping Calibration | large | Calibration/Motion | Two-step calibration process for motion optimization, reducing ghosting and vibration artifacts. | v2.3.1-alpha | - | [#9160](https://github.com/SoftFever/OrcaSlicer/pull/9160) |
| Junction Deviation Calibration | medium | Calibration/Motion | Calibration for Marlin 2 and Klipper-based printers to optimize junction deviation settings. | v2.3.1-alpha | - | [#9160](https://github.com/SoftFever/OrcaSlicer/pull/9160) |
| YOLO Flow Rate Calibration | medium | Calibration/Flow | Single-pass flow rate testing covering 95% of use cases for quick calibration. | v2.2.0-beta | - | [#6479](https://github.com/SoftFever/OrcaSlicer/pull/6479) |
| Archimedean Chords Flow Calibration | small | Calibration/Flow | Enhanced flow rate calibration using ipArchimedeanChords pattern for improved accuracy. | v2.3.1-alpha | - | [#8993](https://github.com/SoftFever/OrcaSlicer/pull/8993) |

## Calibration Workflow

The recommended calibration order for new filaments:
1. Temperature Tower - Find optimal print temperature
2. Max Flowrate Test - Determine maximum volumetric speed
3. Pressure Advance Calibration - Tune PA for corner quality
4. Flow Rate Calibration - Fine-tune extrusion multiplier
5. Retraction Testing - Optimize retraction settings

## Handy Calibration Models

OrcaSlicer includes built-in calibration models (added in v1.6.0):
- Orca Cube v2
- #3DBenchy
- Voron Cube
- Autodesk FDM Test
- Stanford Bunny (v1.9.0-alpha)
- Orca String Hell Model ([#3892](https://github.com/SoftFever/OrcaSlicer/pull/3892))

## Notes

- PA values in filament profiles override AMS slot values (v1.4.3+)
- Smooth spiral mode is disabled during input shaping calibrations (v2.3.1)
- PA Pattern calibration prints max flow value and acceleration on test prints for reference (v2.2.0)
- PA pattern now includes Z-hop capability ([#2952](https://github.com/SoftFever/OrcaSlicer/pull/2952))
