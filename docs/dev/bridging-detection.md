# Bridge Detection Algorithm

How OrcaSlicer detects bridge regions and calculates optimal bridging direction.

## Key Files

| File | Purpose |
|------|---------|
| `src/libslic3r/BridgeDetector.hpp` | Main detector class definition |
| `src/libslic3r/BridgeDetector.cpp` | Detection algorithm implementation |
| `src/libslic3r/LayerRegion.cpp` | High-level bridge processing and expansion |
| `src/libslic3r/Surface.hpp` | Surface types and bridge_angle storage |

## BridgeDetector Class

### Constructor

```cpp
BridgeDetector(ExPolygon _expolygon, const ExPolygons &_lower_slices, coord_t _extrusion_width);
BridgeDetector(const ExPolygons &_expolygons, const ExPolygons &_lower_slices, coord_t _extrusion_width);
```

### Key Members

| Member | Type | Description |
|--------|------|-------------|
| `expolygons` | ExPolygons | Bridge region(s) to analyze |
| `lower_slices` | ExPolygons | Supporting geometry from layer below |
| `spacing` | coord_t | Scaled extrusion width |
| `resolution` | double | Angle search resolution (default: 5 degrees) |
| `angle` | double | Calculated optimal angle (radians, CCW from East) |
| `_edges` | Polylines | Supporting edges (intersection of grown bridge with lower contours) |
| `_anchor_regions` | ExPolygons | Areas where bridges can anchor |

### Main Method

```cpp
bool detect_angle(double bridge_direction_override = 0.);
```

Returns `true` if valid bridge direction found.

## Detection Algorithm

### Step 1: Initialization

```
1. Outset bridge region by extrusion width
   grown = offset(expolygons, spacing)

2. Find supporting edges
   _edges = intersection_pl(to_polylines(grown), lower_slice_contours)

3. Find anchor regions
   _anchor_regions = intersection_ex(grown, lower_slices)
```

### Step 2: Direction Candidate Generation

The algorithm tests multiple angles:

1. **Regular intervals**: 0°, 5°, 10°, ..., 175° (every π/36 radians)
2. **Contour angles**: Angles of bridge polygon perimeter edges
3. **Support edge angles**: Angles of open supporting edges (for C-shaped supports)
4. Duplicates within 1° tolerance removed

### Step 3: Coverage Evaluation

For each candidate angle:

```
1. Get oriented bounding box of anchor regions, rotated by -angle

2. Generate test lines:
   - Evenly spaced parallel lines (spacing = extrusion width)
   - Rotated to match candidate angle
   - Cover entire bounding box

3. Clip lines to bridge area (0.5 × spacing outset)

4. Check anchoring:
   - Both endpoints must be inside anchor_regions
   - Sum total length of anchored lines = coverage

5. Calculate metrics:
   - coverage: Total length of fully-anchored lines
   - max_length: Length of longest anchored line
   - anchored_percent: Ratio of anchored vs total clipped lines
```

### Step 4: Best Direction Selection

```
1. Sort candidates by coverage (highest first)

2. Tie-breaking: If within one extrusion width of coverage,
   prefer shorter max_length

3. Store final angle in BridgeDetector::angle
```

## Inline Direction Detection

For quick detection without explicit BridgeDetector:

```cpp
std::tuple<Vec2d, double> detect_bridging_direction(
    const Lines &floating_edges,
    const Polygons &overhang_area
);

std::tuple<Vec2d, double> detect_bridging_direction(
    const Polygons &to_cover,
    const Polygons &anchors_area
);
```

**Returns**: `{direction_vector, cost_metric}`

**Algorithm**:
- If no floating edges (fully surrounded): Use principal component analysis for shortest bridging direction
- Otherwise: Quantize edge normals, find direction perpendicular to edges that minimizes unsupported bridge ends

## Coverage Calculation

```cpp
Polygons coverage(double angle = -1, bool precise = true) const;
```

After angle detection, generates trapezoids showing bridgeable areas:

1. Rotate bridge region so test lines are vertical
2. Generate trapezoids spaced by extrusion width
3. Check each trapezoid's intersection with anchor regions
4. Return union of covered trapezoids

## Unsupported Edges Detection

```cpp
Polylines unsupported_edges(double angle = -1) const;
```

Identifies edges that aren't supported:

1. Offset lower slices by extrusion width
2. Find bridge edges NOT in grown lower slices
3. Filter out edges parallel to bridging angle (don't need support)
4. Return remaining unsupported edges

## Integration in LayerRegion

### Modern Bridge Expansion System

```cpp
// LayerRegion.cpp ~line 526
const double custom_angle = region.config().bridge_angle.value;
bridges.surfaces = custom_angle > 0 ?
    expand_merge_surfaces(..., Geometry::deg2rad(custom_angle)) :
    expand_bridges_detect_orientations(...);  // Auto-detect
```

**Key functions**:

| Function | Purpose |
|----------|---------|
| `get_grouped_bridges()` | Groups overlapping bridge regions |
| `detect_bridge_directions()` | Rapid angle detection using inline function |
| `expand_bridges_detect_orientations()` | Full pipeline: extract, expand, detect, merge |

### Legacy Detection

Still used in perimeter generation (`PerimeterGenerator.cpp` ~line 1732):

```cpp
BridgeDetector bd(region, lower_layer->lslices, bridging_flow.scaled_width());
if (bd.detect_angle(custom_angle)) {
    bridge_angle = bd.angle;
    unsupported_edges = bd.unsupported_edges();
}
```

## Configuration

| Setting | Effect |
|---------|--------|
| `bridge_angle` | Override auto-detection with fixed angle (0 = auto) |
| `internal_bridge_angle` | Override for internal bridges only |

## Algorithm Summary

**Goal**: Find the angle that maximizes fully-anchored bridge line length.

**Why this works**:
- Longer anchored lines = fewer unsupported spans = stronger bridges
- Lines perpendicular to main support edge are optimal
- Trade-off: coverage vs max span length when ambiguous
- PCA for enclosed regions favors shortest bridges (less sag)

**Key insight**: Uses line clipping and anchoring checks, not area-based methods. This matches how extrusion lines will actually be supported during printing.
