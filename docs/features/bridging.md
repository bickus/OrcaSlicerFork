# Bridging Features

Bridge detection, flow control, and enhancement features for spanning gaps.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Bridge Infill Direction | small | Bridging/Fill | Specification of bridge infill direction for consistent results. | v1.3.1 | - | - |
| Bridge Acceleration | small | Bridging/Speed | Separate acceleration control for bridge movements to improve quality. | v1.4.5 | - | - |
| Bridge Infill Density | medium | Bridging/Fill | Adjustable density for sparse bridge fills, allowing material optimization on long spans. | v1.4.5 | - | - |
| Independent Internal Bridge Speed | small | Bridging/Speed | Separate speed setting for internal bridges independent of external bridge speed. | v1.6.4 | - | - |
| Internal Bridge Flow Rate | small | Bridging/Internal | Separate flow rate parameter for internal bridge layers. | v1.8.1 | - | - |
| Thick Internal Bridges | medium | Bridging/Internal | Toggle option for thicker internal bridge layers with separate flow rate parameter. | v1.8.1 | - | - |
| Counterbore Hole Bridging | medium | Bridging/Geometry | Ported from SuperSlicer, improves bridging in counterbore holes and similar geometries. | v2.0.0-beta | - | [#3189](https://github.com/SoftFever/OrcaSlicer/pull/3189) |
| Internal Bridging Control | medium | Bridging/Internal | Option to control the amount of internal bridging, fixing missing bridges on sloped surfaces. | v2.0.0-beta | - | [#3319](https://github.com/SoftFever/OrcaSlicer/pull/3319) |
| Bridge Anchor Fix | small | Bridging/Anchoring | Fixed bridge anchoring when bridge is also a top surface. | v2.2.0-beta | - | [#6654](https://github.com/SoftFever/OrcaSlicer/pull/6654) |
| Auto Internal Bridge Infill Direction | small | Bridging/Fill | Automatic determination of optimal bridge infill direction based on geometry. | v2.3.1-alpha | v2.3.1-beta | - |

## Bridge Detection

OrcaSlicer automatically detects areas that span gaps between supports, identifying:
- External bridges (visible surfaces)
- Internal bridges (hidden layers)
- Sparse bridges (partially filled areas)

## Bridge Flow Control

### External Bridges
- Standard bridge flow ratio applies
- Fan speed can be set separately for bridges
- Speed reduction for better quality

### Internal Bridges ([#3319](https://github.com/SoftFever/OrcaSlicer/pull/3319))
- Thick internal bridges option (v1.8.1+)
- Separate flow rate parameter
- Can improve layer adhesion over large spans
- Controls amount of internal bridging

## Bridging Algorithm Improvements

### Classical Wall Generator
- Bridging enhancement for single top wall scenarios (v1.6.3-beta)

### Sloped Surface Handling
- Internal bridging control fixes missing bridges on angled surfaces

### Counterbore Holes ([#3189](https://github.com/SoftFever/OrcaSlicer/pull/3189))
- Ported from SuperSlicer
- Improved bridging performance in counterbore holes
- Better handling of similar geometries

## Notes

- 3D Honeycomb infill was optimized to avoid bridge flow for consistency (v2.3.1-beta)
- Bridge anchoring issues with low density infill were fixed in v1.6.3
- Bridge expansion crashes were fixed in v2.3.1-alpha
- Bridge and perimeter flow ratios fixed ([#9210](https://github.com/SoftFever/OrcaSlicer/pull/9210))
