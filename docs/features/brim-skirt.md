# Brim & Skirt Features

Adhesion helpers and outline features for first layer quality.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Skirt Height Support | small | Skirt/Height | Configurable skirt height beyond single layer for draft shielding. | v1.4.1 | - | - |
| Inner Brim Only | small | Brim/Placement | Option to generate brim only on inner edges of model holes. | v1.4.3 | - | - |
| Outer and Inner Brim | small | Brim/Placement | Option to generate brim on both outer perimeter and inner holes. | v1.4.3 | - | - |
| Mouse Ear Brims | medium | Brim/Special | Ported from SuperSlicer, adds small circular brims at corners for corner adhesion without full brim. | v1.6.4-beta3 | v1.6.4 | - |
| Skirt Speed Override | small | Skirt/Speed | Option to override speed specifically for skirt printing. | v1.6.4-beta3 | - | - |
| Brim for Holes (Objects Inside) | medium | Brim/Special | Generate brims for objects positioned inside other objects' holes. | v2.3.0 | - | - |
| Per-Object Skirt | medium | Skirt/PerObject | Apply skirt settings to individual objects rather than globally. | v2.2.0-beta | - | - |
| Per-Object Draft Shield | medium | Skirt/PerObject | Apply draft shield to individual objects for thermal isolation. | v2.2.0-beta | - | - |

## Brim Types

### Standard Brim
- Surrounds entire object perimeter
- Width configurable in mm
- Single or multiple layers

### Mouse Ear Brims
Small circular brims at model corners:
- Improves corner adhesion
- Uses less material than full brim
- Ported from SuperSlicer (v1.6.4-beta3)

### Selective Brim Options
- **Inner Brim Only**: Just inside holes
- **Outer and Inner**: Complete brim coverage
- **Brim for Holes**: When objects are nested (v2.3.0)

## Skirt Options

### Basic Skirt
- Primes nozzle before print
- Configurable distance from object
- Single or multi-layer height

### Draft Shield
- Full-height skirt for thermal isolation
- Useful for warping-prone materials
- Per-object application available (v2.2.0-beta)

## Notes

- Per-object skirt and draft shield allow different settings for each object in print
- Mouse ear brims are particularly effective for large flat models with corner lifting issues
- Skirt height support allows draft shielding without full enclosure
