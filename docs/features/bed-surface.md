# Bed & Build Surface Features

Build plate settings, bed mesh features, and surface configurations.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Bed Size Customization | small | Bed/Configuration | Customize bed size and dimensions per printer. | v1.3.1 | - | - |
| Zero Bed Temperature Support | tiny | Bed/Temperature | Support for printers that don't use heated beds. | v1.3.3 | - | - |
| Adaptive Bed Mesh (KAMP) | large | Bed/Leveling | Adaptive bed mesh support for Klipper (KAMP compatible). | v1.6.4 | - | - |
| Bed Type Memory | small | Bed/Settings | Bed type setting persists across sessions. | v1.8.0-rc | - | - |
| Multiple Bed Types Support | medium | Bed/Configuration | Support for multiple bed types across all printers with custom G-code variable. | v1.9.0 | - | - |
| Bed Texture Rendering Fix (AMD) | small | Bed/Display | Fixed PNG build plate texture rendering on AMD GPUs. | v1.9.0-alpha | - | [#2780](https://github.com/SoftFever/OrcaSlicer/pull/2780) |
| Adaptive Bed Mesh (Full) | large | Bed/Leveling | Comprehensive support for adaptive bed meshing across Marlin, Klipper, and RRF with safety limits. | v2.0.0-beta | - | [#4212](https://github.com/SoftFever/OrcaSlicer/pull/4212) |
| Maximum Bed Size Extension | small | Bed/Configuration | Maximum printer bed size increased from 2147mm to 214 meters for large format. | v2.1.0-beta | - | - |
| Textured Cool Plate Support | small | Bed/Surface | Support for textured cool plate and various coating types. | v2.2.0-beta2 | - | - |

## Adaptive Bed Mesh

### Overview ([#4212](https://github.com/SoftFever/OrcaSlicer/pull/4212))
Automatically adjusts bed mesh probing area to match print footprint:
- Faster leveling for small prints
- Safety limits to prevent bed crashes
- Works without firmware modifications

### Supported Firmwares
- Marlin
- Klipper (KAMP compatible, v1.6.4)
- RepRapFirmware (RRF)

### Benefits
- Reduced leveling time
- More accurate mesh for print area
- No manual configuration needed

## Bed Types

### Configuration Options
- Hot plate
- Cool plate
- Textured plate
- Engineering plate
- Custom types

### Per-Printer Support (v1.9.0)
- Multiple bed types per printer
- Custom G-code variables for bed type
- Automatic G-code adjustments

## Notes

- Bed exclude area limitations removed in v1.4.3
- Faster bed exclusion detection ([#8869](https://github.com/SoftFever/OrcaSlicer/pull/8869))
- Bed temperature configuration fixed for non-first layers (v1.2.4)
- Three-point bicubic bed mesh interpolation corrected (v2.1.0-beta)
