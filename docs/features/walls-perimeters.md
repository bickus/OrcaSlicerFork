# Walls & Perimeters Features

Wall generation, ordering, and perimeter control features.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Arachne Wall Generator | xlarge | Walls/Generator | Advanced variable-width wall generation using skeletal trapezoidation for improved wall quality. | v1.0 (inherited) | v1.4.5 | - |
| Single Wall First Layer | small | Walls/FirstLayer | Option to use single wall on first layer for better bed adhesion and edge quality. | v1.1 | - | - |
| Small Perimeters | medium | Walls/Speed | Speed adjustment for small perimeters with configurable threshold for detecting small features. | v1.3.2 | v1.4.5 | - |
| Sandwich Mode (IOI) | large | Walls/Order | Inner-Outer-Inner wall ordering for better dimensional accuracy without post-travel wall artifacts. | v1.3.4 | v1.4.0, v1.7.0 | - |
| Precise Wall | large | Walls/Accuracy | Experimental feature improving dimensional accuracy and layer consistency in wall generation. | v1.4.5 | v1.8.1 | - |
| One Wall Threshold | small | Walls/TopSurface | Parameter for controlling when single wall is used on top surfaces. | v1.6.4 | - | - |
| Single Wall on Top (Arachne) | medium | Walls/TopSurface | Single wall mode for top surfaces when using Arachne wall generator. | v1.6.4-beta | v1.6.4-beta2 | - |
| Optimized Sandwich Mode (Arachne) | medium | Walls/Arachne | Optimized sandwich wall mode specifically for Arachne wall generator. | v1.6.4-beta3 | - | - |
| First Layer Min Wall Width | small | Walls/FirstLayer | Enforcement of minimum wall width on first layer for consistent extrusion. | v1.7.0-beta | - | - |
| Overhang Extra Perimeters | medium | Walls/Overhang | Additional perimeters on overhanging areas for improved structural support. | v1.7.0-beta | - | - |
| Reverse Perimeter on Odd Layers | medium | Walls/Warping | Change direction for perimeter extrusion at odd layers for warping reduction. | v1.8.0-beta | - | - |
| Vertical Shell Thickness Toggle | tiny | Walls/Detection | Toggle for vertical shell thickness detection algorithm. | v1.8.0-rc | - | - |
| Thin Wall Scar Prevention | small | Walls/Quality | Prevents thin wall scars when "avoid crossing walls" is enabled. | v1.8.0-rc | - | - |
| Reverse Only Internal Perimeters | medium | Walls/Warping | Option to reverse only internal perimeters for warping reduction while maintaining external wall quality. | v1.9.0-alpha | - | [#2722](https://github.com/SoftFever/OrcaSlicer/pull/2722) |
| Wall Direction Specification | small | Walls/Control | User control over wall printing directions (clockwise/counter-clockwise). | v2.0.0-beta | - | [#4156](https://github.com/SoftFever/OrcaSlicer/pull/4156) |
| Short Wall/Perimeter Cleaning | small | Walls/Arachne | Configurable cleaning of very short wall segments in Arachne generator. | v2.0.0-beta | - | [#2790](https://github.com/SoftFever/OrcaSlicer/pull/2790) |
| Inner/Outer Wall Ordering | medium | Walls/Order | Enhanced logic for determining wall print order with improved edge case handling. | v1.0 | v2.2.0-beta | [#6138](https://github.com/SoftFever/OrcaSlicer/pull/6138) |

## Wall Generation Algorithms

### Arachne Wall Generator
The Arachne wall generator uses skeletal trapezoidation to create variable-width walls that:
- Better fill thin features
- Reduce gaps between walls
- Improve dimensional accuracy
- Set as default engine since v1.4.5

### Classical Wall Generator
Traditional fixed-width wall generation, useful for:
- Predictable extrusion widths
- Simpler toolpath planning
- Compatibility with older slicers

## Wall Ordering Options

### Standard Order
- **Inner-Outer**: Print inner walls first, then outer
- **Outer-Inner**: Print outer walls first (better overhangs)
- **Inner-Outer-Inner (Sandwich)**: Hybrid approach for dimensional accuracy

### Sandwich Mode Benefits
- Better dimensional accuracy
- Reduces visible wall artifacts from travel moves
- Available for both classical and Arachne generators
- Algorithm improved in v1.8.0-beta for multi-island models

## Notes

- Default wall loop changed from 2 to 3 in v1.4.0
- Inner-outer wall reordering failures in specific Arachne edge cases fixed in v2.2.0 ([#6138](https://github.com/SoftFever/OrcaSlicer/pull/6138))
- Small perimeters feature defaults to off (v1.4.5)
- ExtrusionLine simplification improved to eliminate very-short segments ([#3750](https://github.com/SoftFever/OrcaSlicer/pull/3750))
