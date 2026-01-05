# Seam & Surface Quality Features

Features for controlling seam placement, visibility, and overall surface finish.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Fuzzy Skin | medium | Surface/Texture | Creates textured, fuzzy surface finish on outer walls for aesthetic or grip purposes. | v1.0 | v2.1.0 | - |
| Seam Gap Control | small | Seam/Placement | Fine-tuning gap at seam location for improved seam quality. | v1.4.3 | - | - |
| Staggered Inner Seams | medium | Seam/Placement | Ported from PrusaSlicer, distributes inner wall seams to reduce visible vertical lines. | v1.6.4-beta3 | - | - |
| Z-Seam Performance Optimization | small | Seam/Performance | Optimized seam calculations for faster slicing performance. | v1.6.4-beta3 | - | - |
| Smooth Spiral Vase Mode | large | Surface/Quality | Eliminates visible seams in vase mode by smoothly transitioning Z height throughout each layer. | v1.9.0-alpha | v2.3.1 | [#3091](https://github.com/SoftFever/OrcaSlicer/pull/3091) |
| Wipe Inside Before External | small | Seam/Quality | Wipe movement inside before extruding external perimeter to improve seam appearance. | v1.9.0-beta | - | - |
| Scarf Joint Seam | xlarge | Seam/Hiding | Innovative approach to hide seams using diagonal transitions, creating nearly invisible layer changes. | v2.0.0-beta | v2.0.0 | [#3839](https://github.com/SoftFever/OrcaSlicer/pull/3839), [#4317](https://github.com/SoftFever/OrcaSlicer/pull/4317) |
| Scarf Joint Overhang Threshold | small | Seam/Scarf | Prevents scarf joint seam application on steep overhangs to avoid quality issues. | v2.0.0 | - | - |
| Scarf Joint Slowdown | small | Seam/Scarf | Speed reduction specifically during scarf joint transitions for better quality. | v2.0.0-beta | - | - |
| Scarf Joint Flow Ratio | tiny | Seam/Scarf | Flow adjustment during scarf joint (developer mode only due to unclear utility). | v2.0.0-beta | v2.0.0 | - |
| Fuzzy Skin Modifier | small | Surface/Texture | Modifier support for fuzzy skin to prevent unwanted perimeters on certain areas. | v2.2.0-beta2 | - | [#6716](https://github.com/SoftFever/OrcaSlicer/pull/6716) |
| Seam Alignment: Aligned Back | medium | Seam/Placement | Positions seams away from front-facing surfaces, balancing between hidden placement and front avoidance. | v2.3.1-alpha | - | [#10255](https://github.com/SoftFever/OrcaSlicer/pull/10255) |
| Fuzzy Skin Extrusion Mode | medium | Surface/Texture | Novel approach using extrusion flow modification instead of jagged toolpaths for fuzzy effect. | v2.3.1-alpha | - | [#9878](https://github.com/SoftFever/OrcaSlicer/pull/9878) |
| Fuzzy Skin Painting | large | Surface/Texture | Paint fuzzy texture onto specific surfaces for precise control over where rough finish appears. | v2.3.1-alpha | - | [#9979](https://github.com/SoftFever/OrcaSlicer/pull/9979) |

## Seam Strategies

### Standard Seam Placement Options
- **Nearest**: Places seam at the nearest point
- **Aligned**: Aligns seams along a consistent line
- **Random**: Distributes seams randomly
- **Rear**: Places seams at the rear of the model
- **Aligned Back**: New option for front-facing models ([#10255](https://github.com/SoftFever/OrcaSlicer/pull/10255))

### Scarf Joint Seam

The scarf joint seam creates a diagonal transition that blends layers together, dramatically reducing seam visibility. Key considerations:
- Best for models where seam visibility is critical
- May not work well on steep overhangs (use threshold setting)
- Slight performance overhead during slicing

## Surface Texture Options

### Fuzzy Skin Modes
1. **Standard Fuzzy Skin**: Applies randomized displacement to outer walls
2. **Painted Fuzzy Skin**: Selective application using painting tools ([#9979](https://github.com/SoftFever/OrcaSlicer/pull/9979))
3. **Extrusion Mode**: Creates texture through flow variation rather than path displacement ([#9878](https://github.com/SoftFever/OrcaSlicer/pull/9878))

## Notes

- Enhanced fuzzy skin with truly uniform and random Z-direction patterns added in v2.1.0
- Arachne wall generator fuzzy skin randomness was fixed in v2.2.0-beta2
- Smooth spiral mode is disabled during calibrations to avoid measurement interference
