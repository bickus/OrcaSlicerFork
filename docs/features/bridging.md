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
| Extra Bridge Settings | medium | Bridging/Extra | Adds 7 separate settings for extra bridge layers (second layer over bridges): speed, acceleration, jerk, flow, density, line width, and wall overlap. When set to 0, each setting falls back to context-appropriate regular bridge setting. | Швидкосрал | - | - |

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

## Extra Bridge Settings (Швидкосрал)

Extra bridge layers are the second layer printed over bridges, providing additional reinforcement. These settings allow fine-tuning extra bridges independently:

| Setting | Location | Fallback Behavior |
|---------|----------|-------------------|
| Extra Bridge Speed | Speed -> Bridge | External extra bridges use bridge speed, internal use internal bridge speed |
| Extra Bridge Acceleration | Speed -> Acceleration | External extra bridges use bridge acceleration, internal use internal bridge acceleration |
| Extra Bridge Jerk | Speed -> Jerk(XY) | External extra bridges use bridge jerk, internal use internal bridge jerk |
| Extra Bridge Flow | Quality -> Bridging | External extra bridges use bridge flow, internal use internal bridge flow |
| Extra Bridge Density | Quality -> Bridging | External extra bridges use bridge density, internal use internal bridge density |
| Extra Bridge Line Width | Quality -> Line Width | External extra bridges use bridge line width, internal use internal bridge line width |
| Extra Bridge Wall Overlap | Strength -> Infill | Falls back to bridge infill/wall overlap |

All settings default to 0, which triggers the fallback behavior.

## Notes

- 3D Honeycomb infill was optimized to avoid bridge flow for consistency (v2.3.1-beta)
- Bridge anchoring issues with low density infill were fixed in v1.6.3
- Bridge expansion crashes were fixed in v2.3.1-alpha
- Bridge and perimeter flow ratios fixed ([#9210](https://github.com/SoftFever/OrcaSlicer/pull/9210))
