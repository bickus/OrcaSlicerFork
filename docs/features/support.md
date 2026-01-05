# Support Structures Features

Support generation algorithms and settings for overhanging geometry.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Organic Tree Support | xlarge | Support/Algorithm | Advanced tree support algorithm ported from PrusaSlicer, creating organic branch-like structures for efficient support. | v1.7.0-beta | v1.7.0 | - |
| Classic Tree Support | large | Support/Algorithm | Traditional tree support generation with straight branches and configurable angles. | v1.0 (inherited) | - | - |
| Support Angle Specification | small | Support/Settings | Improved support generation with customizable overhang angle threshold. | v1.5.0 | - | - |
| Brim Width for Tree Supports | small | Support/Adhesion | Adjustable brim width specifically for tree support bases for better adhesion. | v1.5.0 | - | - |
| Adaptive Layer Height for Tree Supports | medium | Support/Optimization | Toggle for adaptive layer height when using tree supports for faster printing. | v1.5.0 | - | - |
| Per-Object Skirt and Draft Shield | medium | Support/PerObject | Apply skirt and draft shield settings to individual objects rather than globally. | v2.2.0-beta | - | - |
| Raft Sizing with Tree Supports | small | Support/Raft | Fixed raft sizing when used with non-organic tree support enabled. | v1.7.0-beta | - | - |
| Support Ironing | small | Support/Interface | Support for ironing on support interface layers for smoother contact surfaces. | v2.3.1-alpha | - | - |

## Tree Support Types

### Organic Tree Support
- Creates natural-looking branch structures
- Better material efficiency than traditional supports
- Smoother surfaces where support contacts model
- Ported from PrusaSlicer by @Noisyfox

### Classic Tree Support
- More predictable support placement
- Easier to remove in some cases
- Better for models with many small overhangs

## Support Settings

### Key Parameters
- **Support Threshold Angle**: Minimum overhang angle requiring support
- **Support Density**: How solid support structures are
- **Support Interface Layers**: Number of transition layers between support and model
- **Support Z Distance**: Gap between support and model for easier removal

## Notes

- Tree support calculations were improved in v2.0.0-beta to fix various edge cases
- Raft sizing with tree supports was corrected in v1.7.0-beta
- Pressure equalizer crashes with supports enabled were fixed in v2.0.0-beta
