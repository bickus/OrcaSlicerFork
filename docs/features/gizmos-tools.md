# Gizmos & Tools Features

Model editing, manipulation tools, and specialized gizmos.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Color Paint Gizmo | medium | Tools/Painting | Paint colors onto model surfaces for multi-color printing. | v1.0 (inherited) | v2.0.0-beta | - |
| Thumbview Preview | small | Tools/Preview | Thumbnail preview support for printer displays. | v1.1 | - | - |
| Elephant Foot Compensation Multi-Layer | medium | Tools/Quality | Elephant foot compensation applied over multiple layers. | v1.8.0-beta | - | - |
| Automatic Hole to Polyhole | medium | Tools/Geometry | Ported from SuperSlicer, converts holes to polygon-based holes for better printing. | v1.8.0-beta | - | - |
| Straight Line Painting | small | Tools/Painting | Vertical and horizontal straight line mode for painting tools. | v1.8.0-beta | - | - |
| Cut Gizmo | medium | Tools/Editing | Ported from PrusaSlicer, provides model cutting operations. | v1.9.0-alpha | v2.0.0-beta | [#2603](https://github.com/SoftFever/OrcaSlicer/pull/2603) |
| Emboss Gizmo | medium | Tools/Text | Apply embossing effects to model surfaces. | v1.9.0-alpha | v1.9.0 | [#2819](https://github.com/SoftFever/OrcaSlicer/pull/2819) |
| Measure Gizmo | medium | Tools/Measurement | Ported from PrusaSlicer, enables precise model measurements. | v1.9.0-alpha | - | [#2603](https://github.com/SoftFever/OrcaSlicer/pull/2603) |
| SVG Gizmo | medium | Tools/Vector | Work with SVG graphics directly in the workspace. | v1.9.0-alpha | - | [#2819](https://github.com/SoftFever/OrcaSlicer/pull/2819) |
| Project Info Editor | small | Tools/Metadata | Edit and manage project information within the application. | v2.0.0-beta | - | - |

## Measurement & Cutting

### Measure Gizmo ([#2603](https://github.com/SoftFever/OrcaSlicer/pull/2603))
- Point-to-point measurements
- Edge and surface measurements
- Coordinate display
- Ported from PrusaSlicer

### Cut Gizmo ([#2603](https://github.com/SoftFever/OrcaSlicer/pull/2603))
- Planar cuts
- Freeform cutting
- Part separation
- UI improvements in v2.0.0-beta

## Text & Graphics

### Emboss Gizmo ([#2819](https://github.com/SoftFever/OrcaSlicer/pull/2819))
- Text embossing on surfaces
- Adjustable depth
- Font selection
- macOS stability improvements (v1.9.0)
- Numbers 290-305 fixed (v2.0.0)

### SVG Gizmo ([#2819](https://github.com/SoftFever/OrcaSlicer/pull/2819))
- Import SVG files
- Scale and position
- Convert to 3D geometry

## Painting Tools

### Color Paint Gizmo
- Paint multi-material colors
- Brush size adjustment
- Wireframe display in MMU painter mode ([#2808](https://github.com/SoftFever/OrcaSlicer/pull/2808))
- Fixed crashes with part selection (v2.0.0-beta)

### Fuzzy Skin Painting ([#9979](https://github.com/SoftFever/OrcaSlicer/pull/9979))
- Paint textured areas
- Selective fuzzy skin application
- See [Seam & Surface Quality](seam-quality.md) for details

## Geometry Tools

### Automatic Polyhole Conversion
- Converts circular holes to polygonal
- Better dimensional accuracy
- Ported from SuperSlicer

### Elephant Foot Compensation
- Standard single-layer compensation
- Multi-layer compensation (v1.8.0-beta)
- Layer path order fixed (v1.8.0-beta)

## Notes

- Gizmo shortcuts improved in v2.3.1-alpha
- Coordinate system selection for transformations added (v2.3.1-alpha)
- Emboss numbers for 290-305 range fixed (v2.0.0)
- Text addition crash on Mac Intel fixed (v1.4.1)
- Variable layer height gizmo auto-closure when other toolbar buttons activate (v2.1.0-beta)
