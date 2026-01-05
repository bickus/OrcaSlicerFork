# Infill Features

Infill patterns, density controls, and internal structure generation.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Lightning Infill | medium | Infill/Pattern | Sparse infill optimization for internal structures, using tree-like patterns for minimal material. | v1.0 (inherited) | v2.0.0 | - |
| Filter Out Tiny Gaps | small | Infill/Quality | Filters tiny gaps from infill areas, with option to exclude top/bottom visible areas. | v1.3.3 | v1.4.5 | - |
| Sparse/Internal Solid Infill Acceleration | small | Infill/Speed | Separate acceleration controls for sparse and internal solid infill movements. | v1.5.0 | - | - |
| Infill Anchor Parameter | small | Infill/Connection | Reintroduced infill_anchor parameter for controlling infill-to-wall connections. | v1.6.3 | - | - |
| Internal Solid Infill Pattern | medium | Infill/Pattern | Support for specific patterns on internal solid infill areas separate from visible surfaces. | v1.6.4 | - | - |
| Alternate Extra Wall for Infill | small | Infill/Anchoring | Adds alternating extra walls for better infill anchoring to perimeters. | v1.9.0-alpha | - | [#3196](https://github.com/SoftFever/OrcaSlicer/pull/3196) |
| Monotonic Infill Fixes | small | Infill/Quality | Corrected monotonic lines to properly respect user-defined infill and wall overlap settings. | v2.0.0-beta | - | [#3388](https://github.com/SoftFever/OrcaSlicer/pull/3388) |
| Combined Infill Max Layer Height | small | Infill/Optimization | Parameter for combining infill layers to reduce print time while maintaining quality. | v2.2.0-beta | - | [#6401](https://github.com/SoftFever/OrcaSlicer/pull/6401) |
| Crosshatch Default Infill | small | Infill/Pattern | Changed default infill pattern from grid to crosshatch for improved print quality. | v2.3.0 | - | - |
| Narrow Infill Detection Toggle | tiny | Infill/Detection | Toggle for narrow infill detection algorithm to handle thin areas appropriately. | v1.8.0-rc | - | - |
| Sparse Infill Rotation System | large | Infill/Pattern | Template-based pattern specification using metalanguage for defining any rotation pattern. | v2.3.1-alpha | v2.3.1-beta | [#9924](https://github.com/SoftFever/OrcaSlicer/pull/9924), [#9996](https://github.com/SoftFever/OrcaSlicer/pull/9996) |
| Infill Line Multiplier | medium | Infill/Density | Multiplies infill line count while preserving density, balancing strength and material distribution. | v2.3.1-alpha | - | [#9432](https://github.com/SoftFever/OrcaSlicer/pull/9432) |
| Extra Solid Infill Insertion | large | Infill/Strength | Insert extra solid infill layers at specific layers for added strength at critical points. | v2.3.1-beta | - | - |
| Fisher Koch Infill Precision | small | Infill/Pattern | Replaced lookup tables with direct trigonometric functions, eliminating grid artifacts. | v2.3.1-beta | - | - |
| 3D Honeycomb Optimization | small | Infill/Pattern | Eliminated bridge flow usage in 3D honeycomb for more consistent results. | v2.3.1-beta | - | - |
| Lateral Honeycomb Infill | medium | Infill/Pattern | New infill pattern optimized for aircraft wing applications. | v2.3.1-alpha | - | [#9483](https://github.com/SoftFever/OrcaSlicer/pull/9483) |
| TPMS-FK and TPMS-D Infill | medium | Infill/Pattern | Triply Periodic Minimal Surface variants for advanced infill geometry. | v2.3.1-alpha | - | [#10360](https://github.com/SoftFever/OrcaSlicer/pull/10360), [#9613](https://github.com/SoftFever/OrcaSlicer/pull/9613) |

## Infill Pattern Types

### Standard Patterns
- Grid
- Crosshatch (default since v2.3.0)
- Triangles
- Cubic
- Gyroid
- Honeycomb
- 3D Honeycomb
- Lightning

### Specialty Patterns (v2.3.1-alpha+)
- **Lateral Honeycomb**: For wing-like structures ([#9483](https://github.com/SoftFever/OrcaSlicer/pull/9483))
- **TPMS-FK**: Fischer-Koch minimal surface ([#10360](https://github.com/SoftFever/OrcaSlicer/pull/10360))
- **TPMS-D**: Diamond minimal surface ([#9613](https://github.com/SoftFever/OrcaSlicer/pull/9613))

## Infill Rotation Templates

The rotation template system ([#9924](https://github.com/SoftFever/OrcaSlicer/pull/9924)) allows complex rotation patterns:
- Use metalanguage syntax to define patterns
- Example: `5#2` means 2 consecutive layers every 5 layers
- Supports explicit layer lists with 1-based indexing
- Made mutually exclusive with "Sparse infill direction" setting

## Notes

- Lightning infill "divide by zero" error was fixed when sparse infill width set to 0 (v2.0.0-beta)
- Bridge anchoring issues with low density infill were fixed in v1.6.3
- Infill rotation and direction settings are mutually exclusive (v2.3.1-beta)
- Extend sparse infill ported from Prusa (v1.8.0-beta)
