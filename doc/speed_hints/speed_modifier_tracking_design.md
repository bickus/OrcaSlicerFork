# Speed Modifier Tracking - Implementation Design

## Overview

**Goal:** Display a chain of speed modifiers in the Preview → Speed tab tooltip showing WHY the current speed is what it is.

**Core Principle:** Record each modifier at the EXACT point in code where it's applied. **ZERO guesswork.**

---

## Critical Requirements

1. **NO GUESSWORK** - Every modifier tracked at exact application point
2. **NO G-CODE CONTENT PARSING** - Only allowed: identifying G1 commands with extrusion (E+)
3. **PER-G1 GRANULARITY** - Each G1 extrusion command has its own modifier data
4. **MEMORY EFFICIENT** - G-code can have 2-15 million lines; use sparse storage
5. **DIRECT TRACKING** - CoolingBuffer records what it modifies, not detected by comparison

## Scope

**Supported:** Newly sliced prints (data passed in-memory during slicing pipeline)

**Not Supported:** Loaded G-code files - when opening a `.gcode` file directly, extended speed modifier explanations are not available (only standard feedrate display)

---

## Architecture Overview

### Key Infrastructure

- **`m_processor`**: GCodeProcessor is a member of GCode class (GCode.hpp:589)
- **Streaming architecture**: GCodeOutputStream::write() calls `m_processor.process_buffer()` after each write
- **TBB Pipeline**: GCode → CoolingBuffer → FanMover → GCodeOutputStream → processor

### Pipeline Order

```
GCode._extrude()
    └─ Generates G-code strings, stores modifier data in m_g1_modifier_data
         │
         ▼
CoolingBuffer (pipeline filter)
    └─ Modifies feedrates, stores cooling data in m_cooling_data
         │
         ▼
FanMover (pipeline filter)
         │
         ▼
GCodeOutputStream::write()  [GCode.cpp:5177-5185]
    ├─ fwrite() to disk
    └─ m_processor.process_buffer() - streaming parse
         │
         ▼
MoveVertex array (PERMANENT storage for Preview UI)
```

### Data Flow

```
GCode._extrude()
    ├─ Count G1 E+ commands: ++m_g1_counter
    ├─ For EACH G1 E+ emitted:
    │   └─ Store g1_modifier_data[g1_counter] = {base_speed, base_type, modifiers[]}
    └─ Emit G-code string

NOTE: Wipe moves have NEGATIVE E values, so is_g1_extrusion() excludes them automatically.
         │
         ▼
CoolingBuffer.process_layer()
    ├─ Count G1 E+ commands: ++m_g1_counter (deterministic - same logic as GCode)
    ├─ When modifying a G1 E+ line's feedrate:
    │   └─ Store cooling_data[g1_counter] = {original_F, new_F}  (SPARSE - only slowed lines)
    └─ Output modified G-code
         │
         ▼
Before GCodeProcessor processes layer:
    ├─ m_processor.set_modifier_data(&gcode.get_g1_modifier_data())
    └─ m_processor.set_cooling_data(&cooling_buffer.get_cooling_data())
         │
         ▼
GCodeProcessor.process_buffer()  [called per-write in streaming mode]
    ├─ Count G1 E+ commands: ++m_g1_counter (deterministic - same logic)
    ├─ Look up g1_modifier_data[g1_counter] → COPY to MoveVertex (PERMANENT storage)
    ├─ Look up cooling_data[g1_counter] → add LayerTimeCooling if present
    └─ MoveVertex array persists for Preview UI
         │
         ▼
After each layer: Clear TEMPORARY transfer maps (g1_modifier_data, cooling_data)
                  MoveVertex array retains all data for entire print
```

**Memory model:**
- `g1_modifier_data` and `cooling_data` are **temporary** transfer maps, cleared per-layer
- `MoveVertex` array is **permanent** storage used by Preview UI
- Per-layer clearing avoids holding 2 copies of data in memory

### Key Code Locations

| Component | File | Lines |
|-----------|------|-------|
| GCodeProcessor member | GCode.hpp | 589 |
| GCodeOutputStream::write() | GCode.cpp | 5177-5185 |
| Variable-speed emission loop | GCode.cpp | 5932-6053 |
| Non-variable-speed emission loop | GCode.cpp | 5766-5931 |
| CoolingLine struct | CoolingBuffer.cpp | 46-105 |
| CoolingBuffer feedrate parsing | CoolingBuffer.cpp | 398-407 |
| **Pipeline cooling filter (MODIFY)** | GCode.cpp | 2856-2861 |
| CoolingBuffer.process_layer() flush | CoolingBuffer.cpp | 314-332 |

### Key Synchronization Mechanism

**G1 Extrusion Counter** - All three components count G1 commands with positive extrusion using IDENTICAL logic:

```cpp
// In ALL three components (GCode, CoolingBuffer, GCodeProcessor):
bool is_g1_extrusion(const std::string& line) {
    // Must start with "G1 " (or "G1\t")
    if (line.size() < 3 || line[0] != 'G' || line[1] != '1' ||
        (line[2] != ' ' && line[2] != '\t'))
        return false;

    // Must have E parameter with positive value
    size_t e_pos = line.find('E');
    if (e_pos == std::string::npos) return false;

    // Check if E value is positive (has extrusion)
    double e_val = std::stod(line.substr(e_pos + 1));
    return e_val > 0;
}
```

**Why this works:**
- G1 E+ commands are deterministic - same input produces same output
- No parsing of comments, markers, or role tags
- Counter increments identically across all components

**Important:** This counter (`m_g1_extrusion_counter`) is NOT the same as the existing `m_g1_line_id` in GCodeProcessor. The existing counter tracks ALL G0/G1 commands, while this new counter specifically tracks only G1 commands with positive extrusion (E+). This design is intentional for memory efficiency and direct correlation with extrusion MoveVertex entries.

---

## Data Structures

### SpeedModifierEntry (in GCodeProcessor.hpp)

```cpp
struct SpeedModifierEntry {
    enum class Type : uint8_t {
        None = 0,
        FirstLayer,         // initial_layer_speed applied
        SlowDownLayers,     // interpolated speed for slow_down_layers
        Overhang,           // overhang speed reduction (value = percentage 0-100)
        LayerTimeCooling,   // slow_down_for_layer_cooling (from CoolingBuffer)
        VolumetricCap,      // filament_max_volumetric_speed limit
        ResonanceAvoidance, // resonance_avoidance clamping
        SmallPerimeter,     // small_perimeter_speed applied
        ScarfJoint,         // scarf_joint_speed limit
        CurledEdge,         // slowdown_for_curled_perimeters (separate from Overhang)
        Bridge,             // bridge_speed applied (external or internal)
        GapFill             // gap_fill_speed applied
    };

    Type type{ Type::None };
    float value{ 0.0f };        // For Overhang: percentage (0-100)
                                // For CurledEdge: curled height factor
                                // For others: speed reduction amount
    float speed_after{ 0.0f };  // Speed after this modifier applied (mm/s)
};
```

### BaseSpeedType Enum (in GCodeProcessor.hpp)

```cpp
enum class BaseSpeedType : uint8_t {
    Normal = 0,         // Regular role speed - shows "Base: XX mm/s"
    Bridge,             // External bridge - shows "Bridge: XX mm/s"
    InternalBridge,     // Internal bridge - shows "Internal Bridge: XX mm/s"
    OverhangBridge,     // Overhang perimeter bridge - shows "Overhang Bridge: XX mm/s"
    Support,            // Support material - shows "Support: XX mm/s"
    SupportInterface,   // Support interface - shows "Support Interface: XX mm/s"
    TopSurface,         // Top solid infill - shows "Top Surface: XX mm/s"
    BottomSurface,      // Bottom solid infill - shows "Bottom Surface: XX mm/s"
    GapFill,            // Gap fill - shows "Gap Fill: XX mm/s"
    Ironing,            // Ironing pass - shows "Ironing: XX mm/s"
    ThinWall,           // Thin wall - shows "Thin Wall: XX mm/s"
    Skirt,              // Skirt - shows "Skirt: XX mm/s"
    Brim                // Brim - shows "Brim: XX mm/s"
};
```

### G1ModifierData (in GCode.hpp)

```cpp
struct G1ModifierData {
    float base_speed{ 0.0f };
    SpeedModifierEntry modifiers[5];  // Max 5 modifiers (increased from 4)
    uint8_t count{ 0 };
    BaseSpeedType base_type{ BaseSpeedType::Normal };

    void add_modifier(SpeedModifierEntry::Type type, float value, float speed_after) {
        if (count < 5) {
            modifiers[count++] = { type, value, speed_after };
        }
    }

    void reset_variable_modifiers() {
        // Remove Overhang and CurledEdge modifiers (keep others like FirstLayer, SmallPerimeter)
        uint8_t write_idx = 0;
        for (uint8_t i = 0; i < count; ++i) {
            if (modifiers[i].type != SpeedModifierEntry::Type::Overhang &&
                modifiers[i].type != SpeedModifierEntry::Type::CurledEdge) {
                if (write_idx != i) {
                    modifiers[write_idx] = modifiers[i];
                }
                ++write_idx;
            }
        }
        count = write_idx;
    }
};
```

### CoolingModification (in CoolingBuffer.hpp)

```cpp
struct CoolingModification {
    float original_feedrate;
    float new_feedrate;
};
```

### CoolingLine Struct (EXISTING - in CoolingBuffer.cpp lines 46-105)

```cpp
// Current fields:
struct CoolingLine {
    size_t  type;           // Bitfield for line type flags
    size_t  line_start;     // Start position in G-code string
    size_t  line_end;       // End position in G-code string
    float   length;         // XY Euclidean length
    float   feedrate;       // Current feedrate (mm/sec, NOT mm/min)
    float   time;           // Current duration of segment
    float   time_max;       // Maximum duration of segment
    bool    slowdown;       // If line has been slowed down

    // NEW fields to add:
    uint64_t g1_id{ 0 };           // G1 extrusion counter ID
    float   original_feedrate{ 0.f }; // Feedrate before slowdown
};
```

**Note:** Feedrate stored in mm/sec (converted from mm/min at line 404: `new_pos[4] /= 60.f`). Both `feedrate` and `original_feedrate` should be stored in mm/sec (matching CoolingBuffer's internal representation). When recording `original_feedrate`, capture the already-converted mm/sec value.

### MoveVertex Extension (in GCodeProcessor.hpp)

```cpp
struct MoveVertex {
    // ... existing fields ...

    // Speed modifier tracking
    static constexpr uint8_t MaxSpeedModifiers = 5;  // Increased from 4
    float base_speed{ 0.0f };
    SpeedModifierEntry speed_modifiers[MaxSpeedModifiers];
    uint8_t speed_modifier_count{ 0 };
    BaseSpeedType base_type{ BaseSpeedType::Normal };  // Single byte enum instead of flags

    bool has_speed_modifiers() const {
        return base_speed > 0.0f || speed_modifier_count > 0;
    }
};
```

---

## Implementation Steps

### Step 1: G1 Counter Infrastructure

**In GCode.hpp:**
```cpp
class GCode {
    // G1 extrusion counter - increments for each G1 E+ emitted
    uint64_t m_g1_extrusion_counter{ 0 };

    // Per-G1 modifier storage (cleared per layer)
    std::unordered_map<uint64_t, G1ModifierData> m_g1_modifier_data;

    // Small perimeter state (passed from extrude_loop to _extrude)
    struct SmallPerimeterState {
        bool applied{ false };
        float original_speed{ 0.0f };
        float small_peri_speed{ 0.0f };
    };
    SmallPerimeterState m_small_perimeter_state;

    // Current G1 modifier accumulator (reset per G1)
    G1ModifierData m_current_g1_modifiers;

public:
    const auto& get_g1_modifier_data() const { return m_g1_modifier_data; }
    void clear_layer_modifier_data() { m_g1_modifier_data.clear(); }
};
```

**In CoolingBuffer.hpp:**
```cpp
class CoolingBuffer {
    uint64_t m_g1_extrusion_counter{ 0 };
    std::unordered_map<uint64_t, CoolingModification> m_cooling_data;

public:
    const auto& get_cooling_data() const { return m_cooling_data; }
    void clear_layer_cooling_data() { m_cooling_data.clear(); }
};
```

**In GCodeProcessor.hpp:**
```cpp
class GCodeProcessor {
    uint64_t m_g1_extrusion_counter{ 0 };

    // Data received from GCode and CoolingBuffer
    const std::unordered_map<uint64_t, G1ModifierData>* m_g1_modifier_data{ nullptr };
    const std::unordered_map<uint64_t, CoolingModification>* m_cooling_data{ nullptr };

public:
    void set_g1_modifier_data(const std::unordered_map<uint64_t, G1ModifierData>* data) {
        m_g1_modifier_data = data;
    }
    void set_cooling_data(const std::unordered_map<uint64_t, CoolingModification>* data) {
        m_cooling_data = data;
    }
};
```

---

### Step 2: GCode.cpp - Record Modifiers Per G1

**Location:** `GCode::_extrude()` - Two emission loops exist:

1. **Non-variable-speed loop** (lines 5766-5931): Standard path emission
2. **Variable-speed loop** (lines 5932-6053): Per-point speed variation for overhang

**Variable-speed loop structure (lines 5954-6053):**

```cpp
for (size_t i = 1; i < new_points.size(); i++) {
    const ProcessedPoint &processed_point = new_points[i];
    const ProcessedPoint &pre_processed_point = new_points[i-1];
    Vec2d p = this->point_to_gcode_quantized(processed_point.p);

    // ... speed handling, PA adjustment ...

    auto dE = e_per_mm * line_length;  // Extrusion amount

    // G1 emission via GCodeWriter
    gcode += m_writer.extrude_to_xy(p, dE, ...);  // Line 6102-6110

    // INSERT G1 TRACKING HERE:
    ++m_g1_extrusion_counter;
    update_modifiers_for_point(pre_processed_point);  // Use pre_processed_point for THIS segment
    m_g1_modifier_data[m_g1_extrusion_counter] = m_current_g1_modifiers;
}
```

**Non-variable-speed loop - similar pattern:**

```cpp
// Lines 5766-5931
for (const Line& line : path.polyline.lines()) {
    // ... generate G1 ...
    gcode += m_writer.extrude_to_xy(...);

    // INSERT G1 TRACKING HERE:
    ++m_g1_extrusion_counter;
    m_g1_modifier_data[m_g1_extrusion_counter] = m_current_g1_modifiers;
}
```

**Key insight:** G1 tracking must happen AFTER extrude_to_xy() call, as that's when the G1 is actually emitted.

---

### Step 3: Base Speed Type Assignment

**Location:** `GCode.cpp` `_extrude()` role-based speed lookup (~line 5408-5424)

Set `base_type` based on extrusion role. This determines the label shown in UI ("Bridge:", "Support:", etc.)

```cpp
// During role-based speed lookup in _extrude():
switch (path.role()) {
    case erBridgeInfill:
        speed = m_config.get_abs_value("bridge_speed");
        m_current_g1_modifiers.base_type = BaseSpeedType::Bridge;
        break;

    case erInternalBridgeInfill:
        speed = m_config.get_abs_value("internal_bridge_speed", speed);
        m_current_g1_modifiers.base_type = BaseSpeedType::InternalBridge;
        break;

    case erOverhangPerimeter:
        speed = m_config.get_abs_value("bridge_speed");
        m_current_g1_modifiers.base_type = BaseSpeedType::OverhangBridge;
        break;

    case erSupportMaterial:
    case erSupportTransition:
        speed = m_config.get_abs_value("support_speed");
        m_current_g1_modifiers.base_type = BaseSpeedType::Support;
        break;

    case erSupportMaterialInterface:
        speed = m_config.get_abs_value("support_interface_speed");
        m_current_g1_modifiers.base_type = BaseSpeedType::SupportInterface;
        break;

    case erTopSolidInfill:
        speed = m_config.get_abs_value("top_surface_speed");
        m_current_g1_modifiers.base_type = BaseSpeedType::TopSurface;
        break;

    case erBottomSurface:
        speed = m_config.get_abs_value("bottom_surface_speed", speed);
        m_current_g1_modifiers.base_type = BaseSpeedType::BottomSurface;
        break;

    case erGapFill:
        speed = m_config.get_abs_value("gap_fill_speed");
        m_current_g1_modifiers.base_type = BaseSpeedType::GapFill;
        break;

    case erIroning:
        speed = m_config.get_abs_value("ironing_speed");
        m_current_g1_modifiers.base_type = BaseSpeedType::Ironing;
        break;

    case erThinWall:
        // Uses outer_wall_speed typically
        m_current_g1_modifiers.base_type = BaseSpeedType::ThinWall;
        break;

    case erSkirt:
        // Skirt has dedicated skirt_speed config (GCode.cpp lines 5492-5496)
        // Speed override happens later in _extrude() if skirt_speed > 0
        m_current_g1_modifiers.base_type = BaseSpeedType::Skirt;
        break;

    case erBrim:
        // Brim uses support_speed (passed directly to extrude call, not via _extrude lookup)
        // Speed is set before _extrude() is called: m_config.support_speed.value
        m_current_g1_modifiers.base_type = BaseSpeedType::Brim;
        break;

    default:
        // Normal roles: erPerimeter, erExternalPerimeter, erInternalInfill, erSolidInfill, etc.
        m_current_g1_modifiers.base_type = BaseSpeedType::Normal;
        break;
}
```

**UI Display:** Shows role-specific label instead of generic "Base:"

---

### Step 4: Small Perimeter Modifier Tracking

**Location:** `GCode.cpp` `extrude_loop()` (~line 4760-4766) and `_extrude()`

**In extrude_loop():**
```cpp
if (loop.length() <= SMALL_PERIMETER_LENGTH(threshold)) {
    // Record state for _extrude to pick up
    m_small_perimeter_state.applied = true;
    m_small_perimeter_state.original_speed = get_wall_speed(path.role());
    m_small_perimeter_state.small_peri_speed = small_peri_speed;
}
```

**In _extrude():**
```cpp
if (m_small_perimeter_state.applied && is_perimeter(path.role())) {
    m_current_g1_modifiers.add_modifier(
        SpeedModifierEntry::Type::SmallPerimeter,
        m_small_perimeter_state.original_speed - speed,
        speed
    );
    m_small_perimeter_state.applied = false;  // Consume the state
}
```

---

### Step 5: First Layer Modifier Tracking

**Location:** `GCode.cpp` `_extrude()` (~line 5444)

```cpp
if (this->on_first_layer()) {
    double speed_before = speed;
    speed = m_config.get_abs_value("initial_layer_speed");

    if (speed < speed_before - 0.5) {
        m_current_g1_modifiers.add_modifier(
            SpeedModifierEntry::Type::FirstLayer,
            speed_before - speed,
            speed
        );
    }
}
```

---

### Step 5a: Slow Down Layers Modifier Tracking

**Location:** `GCode.cpp` `_extrude()` (lines 5441-5454)

```cpp
// slow_down_layers: gradual speed increase over first N layers
// Distinguishes between perimeters and infill for first layer speed
else if (m_config.slow_down_layers > 1) {
    const auto _layer = layer_id();
    if (_layer > 0 && _layer < m_config.slow_down_layers) {
        // Use different initial speed for perimeters vs infill
        const auto first_layer_speed =
            is_perimeter(path.role())
                ? m_config.get_abs_value("initial_layer_speed")
                : m_config.get_abs_value("initial_layer_infill_speed");

        if (first_layer_speed < speed) {
            double speed_before = speed;
            // Linear interpolation: lerp(first_layer_speed, speed, layer/slow_down_layers)
            speed = std::min(speed,
                Slic3r::lerp(first_layer_speed, speed,
                             (double)_layer / m_config.slow_down_layers));

            if (speed < speed_before - 0.5) {
                m_current_g1_modifiers.add_modifier(
                    SpeedModifierEntry::Type::SlowDownLayers,
                    speed_before - speed,
                    speed
                );
            }
        }
    }
}
```

---

### Step 5b: Volumetric Cap Modifier Tracking

**Location:** `GCode.cpp` `_extrude()` (lines 5516-5522)

```cpp
// filament_max_volumetric_speed: limit speed based on volumetric flow
// _mm3_per_mm is calculated earlier and includes flow ratios:
//   _mm3_per_mm = path.mm3_per_mm * print_flow_ratio * filament_flow_ratio * role_flow_ratios
if (EXTRUDER_CONFIG(filament_max_volumetric_speed) > 0) {
    double speed_before = speed;

    // Formula: max_speed = volumetric_limit / mm3_per_mm
    speed = std::min(speed, EXTRUDER_CONFIG(filament_max_volumetric_speed) / _mm3_per_mm);

    if (speed < speed_before - 0.5) {
        m_current_g1_modifiers.add_modifier(
            SpeedModifierEntry::Type::VolumetricCap,
            speed_before - speed,
            speed
        );
    }
}
```

**Note:** `_mm3_per_mm` is calculated at line 5379 and includes all flow ratio adjustments.

---

### Step 5c: Resonance Avoidance Modifier Tracking

**Location:** `GCode.cpp` `_extrude()` (lines 5476-5504)

**Member variable:** `m_resonance_avoidance` (GCode.hpp line 511)
- Declared as: `bool m_resonance_avoidance;`
- Initialized to: `true` (GCode.hpp line 168)
- This is an EXISTING member variable - no changes needed

**Config options:**
- `resonance_avoidance` (bool) - enable/disable the feature
- `min_resonance_avoidance_speed` (float) - speed to clamp DOWN to
- `max_resonance_avoidance_speed` (float) - threshold above which RA is disabled for the loop

**Logic explanation:**
1. Only applies to `erExternalPerimeter` role
2. If original speed > max threshold → disable RA for entire loop (m_resonance_avoidance = false)
3. If still in RA mode AND speed <= max → clamp to min threshold
4. Reset flag to true for next segment

```cpp
// resonance_avoidance: clamp external perimeter speed to avoid resonance
// Only applies to external perimeters
if (path.role() == erExternalPerimeter && m_config.resonance_avoidance.value) {
    double ref_speed = speed;  // stash pre-cap speed

    // If original speed was above "max", disable RA for this entire loop
    if (ref_speed > m_config.max_resonance_avoidance_speed.value) {
        m_resonance_avoidance = false;  // Existing member variable
    }

    // Re-apply volumetric cap (may have changed)
    if (EXTRUDER_CONFIG(filament_max_volumetric_speed) > 0) {
        speed = std::min(speed, EXTRUDER_CONFIG(filament_max_volumetric_speed) / _mm3_per_mm);
    }

    // If still in avoidance mode and speed under "max", clamp DOWN to "min"
    if (m_resonance_avoidance && speed <= m_config.max_resonance_avoidance_speed.value) {
        double speed_before_ra = speed;
        speed = std::min(speed, m_config.min_resonance_avoidance_speed.value);

        if (speed < speed_before_ra - 0.5) {
            m_current_g1_modifiers.add_modifier(
                SpeedModifierEntry::Type::ResonanceAvoidance,
                speed_before_ra - speed,
                speed
            );
        }
    }

    // Reset flag for next segment
    m_resonance_avoidance = true;
}
```

**Important:** The `m_resonance_avoidance` flag persists across _extrude() calls within a loop. If one segment's speed exceeds max, RA is disabled for all remaining segments of that loop.

---

### Step 5d: Scarf Joint Modifier Tracking

**Location:** `GCode.cpp` `_extrude()` (lines 5397-5420, 5452-5457)

**Detection:** Scarf joint is detected via the `sloped` flag, which is set when the path is an `ExtrusionPathSloped`:
```cpp
bool sloped = dynamic_cast<const ExtrusionPathSloped*>(&path) != nullptr;
```

**Config:** `scarf_joint_speed` (FloatOrPercent) - can be absolute or % of wall speed

```cpp
// Scarf joint speed limiting - integrated into role-based speed lookup
bool scarf_applied = false;
double scarf_base_speed = 0;

if (path.role() == erPerimeter) {
    speed = m_config.get_abs_value("inner_wall_speed");
    if (sloped) {
        double wall_speed = speed;
        speed = std::min(speed, m_config.scarf_joint_speed.get_abs_value(
            m_config.get_abs_value("inner_wall_speed")));
        if (speed < wall_speed - 0.5) {
            scarf_applied = true;
            scarf_base_speed = wall_speed;
        }
    }
} else if (path.role() == erExternalPerimeter) {
    speed = m_config.get_abs_value("outer_wall_speed");
    if (sloped) {
        double wall_speed = speed;
        speed = std::min(speed, m_config.scarf_joint_speed.get_abs_value(
            m_config.get_abs_value("outer_wall_speed")));
        if (speed < wall_speed - 0.5) {
            scarf_applied = true;
            scarf_base_speed = wall_speed;
        }
    }
}

// Later, after base_speed is set (line 5452-5457):
if (scarf_applied) {
    m_current_g1_modifiers.base_speed = static_cast<float>(scarf_base_speed);
    m_current_g1_modifiers.add_modifier(
        SpeedModifierEntry::Type::ScarfJoint,
        scarf_base_speed - speed,
        speed
    );
}
```

**Note:** Scarf joint affects base_speed - the wall speed becomes the base, and ScarfJoint becomes a modifier.

---

### Step 6: Overhang with Percentage (Per-G1)

**Location:** `GCode.cpp` after `estimate_extrusion_quality()` (~lines 5544-5567)

**Key change:** Store per-G1, not per-path. Each G1 in a variable-speed path can have different overhang/curled values.

```cpp
// For variable-speed overhang paths, each G1 gets its own overhang data
void GCode::update_modifiers_for_point(const ProcessedPoint& pt) {
    // CRITICAL: Clear previous Overhang/CurledEdge modifiers before adding new ones
    // This preserves base_type, base_speed, and other modifiers (FirstLayer, SmallPerimeter, etc.)
    m_current_g1_modifiers.reset_variable_modifiers();

    // Add Overhang modifier if overlap indicates overhang
    if (pt.overlap < 1.0f - 0.01f) {  // Has overhang (>1% threshold)
        float overhang_percent = (1.0f - pt.overlap) * 100.0f;
        m_current_g1_modifiers.add_modifier(
            SpeedModifierEntry::Type::Overhang,
            overhang_percent,
            pt.speed
        );
    }

    // Add CurledEdge modifier if curled slowdown was applied
    if (pt.has_curled_slowdown) {
        m_current_g1_modifiers.add_modifier(
            SpeedModifierEntry::Type::CurledEdge,
            pt.curled_height_factor,
            pt.speed
        );
    }
}
```

**How `reset_variable_modifiers()` works:** (defined in G1ModifierData struct)
- Iterates through existing modifiers
- Removes only Overhang and CurledEdge entries
- Preserves all other modifiers (FirstLayer, SmallPerimeter, VolumetricCap, etc.)
- Updates count to reflect remaining modifiers

This ensures that when processing multiple G1s in a variable-speed path, each G1 gets fresh overhang/curled data while retaining the common modifiers applied at path start.

---

### Step 7: Curled Edge as Separate Modifier

**Location:** `ExtrusionProcessor.hpp` lines 284-289 and 375-452

**Current ProcessedPoint struct (line 284-289):**
```cpp
struct ProcessedPoint {
    Point p;
    float speed = 1.0f;     // Speed factor (1.0 = normal)
    float overlap = 1.0f;   // Overlap factor (1.0 = full, <1.0 = overhang)
};
```

**Modified ProcessedPoint - ADD these fields:**
```cpp
struct ProcessedPoint {
    Point p;
    float speed = 1.0f;
    float overlap = 1.0f;
    // NEW fields for curled edge tracking:
    bool has_curled_slowdown{ false };
    float curled_height_factor{ 0.0f };  // The artificial_distance contribution from curled edges
};
```

**How Curled Edge Slowdown Works (lines 375-452):**

1. **Search for nearby curled lines** within `dist_limit = 10.0 * path.width`
2. **Calculate artificial distance** using formula:
   ```cpp
   artificial_distance = path.width *
       (1.0 - (distance_from_curled / dist_limit))^2 *
       (curled_height / (path.height * 10.0f));
   ```
3. **Apply slowdown** by taking minimum of curled_speed and overhang speed:
   ```cpp
   if (slowdown_for_curled_edges) {
       float curled_speed = calculate_speed(artificial_distance_to_curled_lines);
       extrusion_speed = std::min(curled_speed, extrusion_speed);
   }
   ```

**Implementation - Track curled edge in estimate_extrusion_quality():**

Find the section where `artificial_distance_to_curled_lines` is calculated (around line 420):
```cpp
// After calculating artificial_distance_to_curled_lines:
if (artificial_distance_to_curled_lines > 0.001f) {
    // Curled edge slowdown is being applied
    pt.has_curled_slowdown = true;
    pt.curled_height_factor = artificial_distance_to_curled_lines;
}
```

**In GCode.cpp `update_modifiers_for_point()`:**
```cpp
void GCode::update_modifiers_for_point(const ProcessedPoint& pt) {
    m_current_g1_modifiers.reset_variable_modifiers();

    // Add Overhang modifier if overlap indicates overhang
    if (pt.overlap < 1.0f - 0.01f) {
        float overhang_percent = (1.0f - pt.overlap) * 100.0f;
        m_current_g1_modifiers.add_modifier(
            SpeedModifierEntry::Type::Overhang,
            overhang_percent,
            pt.speed
        );
    }

    // Add CurledEdge modifier SEPARATELY if curled slowdown was applied
    if (pt.has_curled_slowdown) {
        m_current_g1_modifiers.add_modifier(
            SpeedModifierEntry::Type::CurledEdge,
            pt.curled_height_factor,  // Store the artificial_distance value
            pt.speed
        );
    }
}
```

**Key insight:** Curled edge and Overhang are COMBINED in the final speed calculation using `std::min()`, but we track them separately for display. Both can be active simultaneously on the same G1.

---

### Step 8: CoolingBuffer Direct Tracking

**Location:** `CoolingBuffer.cpp`

**In parsing phase - count G1 E+ and store original feedrate:**

```cpp
// When parsing G-code line:
if (is_g1_extrusion(line)) {
    ++m_g1_extrusion_counter;
    cooling_line.g1_id = m_g1_extrusion_counter;
    // NOTE: CoolingBuffer already parses F parameter to apply slowdown.
    // We just also store the original value before modification.
    cooling_line.original_feedrate = extract_feedrate(line);
}
```

**In apply_layer_cooldown() (~line 908) - record ONLY slowed lines:**

```cpp
if (line->slowdown) {
    // This line was actually slowed - record it
    m_cooling_data[line->g1_id] = {
        line->original_feedrate,
        static_cast<float>(new_feedrate)
    };
}
```

**Memory efficiency:** Only slowed lines are stored. Most lines are NOT slowed.

**Note on F parameter reading:** This is NOT additional G-code parsing. CoolingBuffer already parses F to apply slowdown - we're just capturing the original value before modification.

---

### Step 9: GCodeProcessor Integration

**Location:** `GCodeProcessor.cpp`

**When processing G1 E+ command:**

```cpp
if (is_g1_extrusion(line)) {
    ++m_g1_extrusion_counter;

    // Look up modifier data for this G1
    if (m_g1_modifier_data) {
        auto it = m_g1_modifier_data->find(m_g1_extrusion_counter);
        if (it != m_g1_modifier_data->end()) {
            const auto& data = it->second;
            move.base_speed = data.base_speed;
            move.base_type = data.base_type;  // Single enum field
            move.speed_modifier_count = data.count;
            for (uint8_t i = 0; i < data.count; ++i) {
                move.speed_modifiers[i] = data.modifiers[i];
            }
        }
    }

    // Look up cooling data - SPARSE, only slowed lines have entries
    if (m_cooling_data) {
        auto it = m_cooling_data->find(m_g1_extrusion_counter);
        if (it != m_cooling_data->end()) {
            // This G1 was slowed by CoolingBuffer
            if (move.speed_modifier_count < MoveVertex::MaxSpeedModifiers) {
                move.speed_modifiers[move.speed_modifier_count++] = {
                    SpeedModifierEntry::Type::LayerTimeCooling,
                    it->second.original_feedrate - it->second.new_feedrate,
                    it->second.new_feedrate
                };
            }
        }
    }
}
```

**Integration Point Details:**
- GCodeProcessor already has `m_g1_line_id` (line 865) which counts ALL G0/G1 commands
- The proposed `m_g1_extrusion_counter` is a SEPARATE counter for G1 E+ only
- Best location for data lookup is in `store_move_vertex()` (line 6130) after MoveVertex creation (lines 6147-6169)
- The counter `m_g1_line_id` is incremented at line 3651 (START of process_G1)
- `store_move_vertex()` is called at line 4189 (END of process_G1)

---

### Step 10: Data Passing and Per-Layer Clearing

**Integration point:** Modify the cooling filter lambda in process_layers() (GCode.cpp lines 2856-2861)

**Concrete implementation:**

```cpp
// GCode.cpp lines 2856-2861 - MODIFY to capture 'this'
const auto cooling = tbb::make_filter<LayerResult, std::string>(
    slic3r_tbb_filtermode::serial_in_order,
    [this, &cooling_buffer = *this->m_cooling_buffer.get()](LayerResult in) -> std::string {
        if (in.nop_layer_result)
            return in.gcode;

        std::string result = cooling_buffer.process_layer(
            std::move(in.gcode), in.layer_id, in.cooling_buffer_flush);

        // INJECTION POINT: After CoolingBuffer flush, set data on processor
        if (in.cooling_buffer_flush) {
            m_processor.set_g1_modifier_data(&get_g1_modifier_data());
            m_processor.set_cooling_data(&cooling_buffer.get_cooling_data());
        }

        return result;
    });
```

**After layer completes (after output filter runs):**

```cpp
// Clear temporary maps after layer is fully written/processed
// Add to end of process_layers() or in a cleanup filter
clear_layer_modifier_data();
m_cooling_buffer->clear_layer_cooling_data();
```

**Important:**
- Counter values are NOT reset between layers - they continue incrementing for the entire print
- MoveVertex array is permanent - Preview reads from it
- Temporary maps cleared per-layer to save memory (avoid 2 copies)
- Data pointers set AFTER CoolingBuffer flush but BEFORE GCodeOutputStream writes

---

### Step 11: UI Display

**Location:** `GCodeViewer.cpp` `Marker::render()` Feedrate case

```cpp
// Helper to get base speed type label
auto get_base_type_label = [](BaseSpeedType type) -> const char* {
    switch (type) {
        case BaseSpeedType::Bridge:           return "Bridge";
        case BaseSpeedType::InternalBridge:   return "Internal Bridge";
        case BaseSpeedType::OverhangBridge:   return "Overhang Bridge";
        case BaseSpeedType::Support:          return "Support";
        case BaseSpeedType::SupportInterface: return "Support Interface";
        case BaseSpeedType::TopSurface:       return "Top Surface";
        case BaseSpeedType::BottomSurface:    return "Bottom Surface";
        case BaseSpeedType::GapFill:          return "Gap Fill";
        case BaseSpeedType::Ironing:          return "Ironing";
        case BaseSpeedType::ThinWall:         return "Thin Wall";
        case BaseSpeedType::Skirt:            return "Skirt";
        case BaseSpeedType::Brim:             return "Brim";
        default:                              return "Base";
    }
};

case EViewType::Feedrate: {
    if (m_curr_move.type != EMoveType::Extrude) break;

    // Show requested speed
    ImGui::SameLine(startx2);
    sprintf(buf, "%s%.0f", speed.c_str(), m_curr_move.feedrate);
    imgui.text(buf);

    if (m_curr_move.has_speed_modifiers()) {
        // Base speed line with type-specific label
        const char* base_label = get_base_type_label(m_curr_move.base_type);
        sprintf(buf, "%s: %.0f mm/s", base_label, m_curr_move.base_speed);
        imgui.text(buf);

        // Modifier chain
        for (uint8_t i = 0; i < m_curr_move.speed_modifier_count; ++i) {
            const auto& mod = m_curr_move.speed_modifiers[i];
            std::string name = get_modifier_name(mod.type);

            if (mod.type == SpeedModifierEntry::Type::Overhang) {
                // Overhang shows percentage: "-> Overhang 45%: 150 mm/s"
                sprintf(buf, "-> %s %.0f%%: %.0f mm/s",
                    name.c_str(), mod.value, mod.speed_after);
            } else if (mod.type == SpeedModifierEntry::Type::CurledEdge) {
                // Curled edge shows height factor: "-> Curled Edge (1.25): 120 mm/s"
                sprintf(buf, "-> %s (%.2f): %.0f mm/s",
                    name.c_str(), mod.value, mod.speed_after);
            } else {
                // Other modifiers show reduction: "-> First Layer: 60 mm/s (-240)"
                sprintf(buf, "-> %s: %.0f mm/s (-%.0f)",
                    name.c_str(), mod.speed_after, mod.value);
            }
            imgui.text(buf);
        }

        text_line = 2 + 1 + m_curr_move.speed_modifier_count;
    }
    break;
}
```

---

## Memory Analysis

### Per-G1 Storage (worst case)

- `G1ModifierData`: ~40 bytes per G1
- Typical layer: 10K-100K G1 extrusion commands
- Memory per layer: 400 KB - 4 MB

### Mitigation: Deduplication (Optional Optimization)

If memory becomes an issue, consecutive G1s with identical modifiers can share data:

```cpp
// Instead of storing per G1:
std::unordered_map<uint64_t, G1ModifierData> m_g1_modifier_data;

// Store ranges:
struct G1Range {
    uint64_t start;
    uint64_t end;
    G1ModifierData data;
};
std::vector<G1Range> m_g1_modifier_ranges;  // Binary search by counter
```

This is optional - implement only if needed.

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/libslic3r/GCode/GCodeProcessor.hpp` | SpeedModifierEntry, MoveVertex extensions, data pointers |
| `src/libslic3r/GCode/ExtrusionProcessor.hpp` | has_curled_slowdown, curled_height_factor in ProcessedPoint |
| `src/libslic3r/GCode.hpp` | G1ModifierData, m_g1_extrusion_counter, m_g1_modifier_data, SmallPerimeterState |
| `src/libslic3r/GCode.cpp` | Record all modifiers per G1, G1 counting, clear per layer |
| `src/libslic3r/GCode/CoolingBuffer.hpp` | CoolingModification, m_g1_extrusion_counter, m_cooling_data |
| `src/libslic3r/GCode/CoolingBuffer.cpp` | G1 counting, store original feedrate, record slowed lines |
| `src/libslic3r/GCode/GCodeProcessor.cpp` | G1 counting, data lookups, copy to MoveVertex |
| `src/slic3r/GUI/GCodeViewer.cpp` | Bridge/Internal Bridge display, Overhang %, Curled Edge display |

---

## Base Speed Types (via BaseSpeedType enum)

These are role-based speeds that get special labels in UI instead of generic "Base:":

| Base Type | Role(s) | UI Label |
|-----------|---------|----------|
| Normal | erPerimeter, erExternalPerimeter, erInternalInfill, erSolidInfill, etc. | "Base" |
| Bridge | erBridgeInfill | "Bridge" |
| InternalBridge | erInternalBridgeInfill | "Internal Bridge" |
| OverhangBridge | erOverhangPerimeter | "Overhang Bridge" |
| Support | erSupportMaterial, erSupportTransition | "Support" |
| SupportInterface | erSupportMaterialInterface | "Support Interface" |
| TopSurface | erTopSolidInfill | "Top Surface" |
| BottomSurface | erBottomSurface | "Bottom Surface" |
| GapFill | erGapFill | "Gap Fill" |
| Ironing | erIroning | "Ironing" |
| ThinWall | erThinWall | "Thin Wall" |
| Skirt | erSkirt | "Skirt" |
| Brim | erBrim | "Brim" |

---

## Speed Modifiers Tracked

| Modifier | Where Tracked | Value Meaning |
|----------|--------------|---------------|
| FirstLayer | GCode._extrude() Step 5 | Speed reduction (mm/s) |
| SlowDownLayers | GCode._extrude() Step 5a | Speed reduction (mm/s) |
| VolumetricCap | GCode._extrude() Step 5b | Speed reduction (mm/s) |
| ResonanceAvoidance | GCode._extrude() Step 5c | Speed reduction (mm/s) |
| ScarfJoint | GCode._extrude() Step 5d | Speed reduction (mm/s) |
| SmallPerimeter | GCode.extrude_loop() + _extrude() Step 4 | Speed reduction (mm/s) |
| Overhang | GCode._extrude() Step 6 (per-G1) | Overhang percentage (0-100) |
| CurledEdge | GCode._extrude() Step 6 (per-G1) | Curled height factor |
| LayerTimeCooling | CoolingBuffer Step 8 (sparse) | Speed reduction (mm/s) |

---

## Key Design Decisions

1. **G1 Counter Sync** - Deterministic counting of G1 E+ commands across all components
2. **Per-G1 Storage** - Each G1 has its own modifier data (not per-path)
3. **Sparse Cooling** - Only G1s actually slowed by CoolingBuffer are stored
4. **Per-Layer Clear** - Data maps cleared after each layer to bound memory
5. **Direct Data Pass** - Pointers passed to GCodeProcessor, not through G-code content
6. **BaseSpeedType Enum** - Single byte enum replaces multiple flags for role-specific labels
7. **Separate Curled** - Curled Edge tracked separately from Overhang
8. **Variable Modifier Reset** - `reset_variable_modifiers()` clears only Overhang/CurledEdge between G1s

---

## Testing Checklist

### G1 Counter Synchronization
- [ ] G1 counter identical across GCode, CoolingBuffer, GCodeProcessor
- [ ] Counter continues across layers (not reset)
- [ ] Counter matches for all test prints

### Base Speed Types (UI Labels)
- [ ] Bridge roles show "Bridge:" instead of "Base:"
- [ ] Internal bridge shows "Internal Bridge:"
- [ ] Overhang perimeter shows "Overhang Bridge:"
- [ ] Support shows "Support:"
- [ ] Support interface shows "Support Interface:"
- [ ] Top surface shows "Top Surface:"
- [ ] Bottom surface shows "Bottom Surface:"
- [ ] Gap fill shows "Gap Fill:"
- [ ] Ironing shows "Ironing:"
- [ ] Skirt shows "Skirt:"
- [ ] Brim shows "Brim:"
- [ ] Normal roles show "Base:"

### Speed Modifiers
- [ ] FirstLayer modifier shows when on first layer
- [ ] SlowDownLayers modifier shows on early layers
- [ ] VolumetricCap modifier shows when volumetric limit hit
- [ ] ResonanceAvoidance modifier shows on external perimeters when enabled
- [ ] ScarfJoint modifier shows during scarf joint segments
- [ ] SmallPerimeter modifier shows on small loops
- [ ] Overhang shows percentage per G1 (varies within path)
- [ ] CurledEdge shows separately from Overhang
- [ ] LayerTimeCooling shows ONLY for lines actually slowed

### Quality
- [ ] No false positives (modifiers shown when not applied)
- [ ] No false negatives (modifiers missing when applied)
- [ ] Memory stays bounded (cleared per layer)
- [ ] Tooltip displays correctly with 0-5 modifiers

---

## Review Checklist

- [x] NO G-code content parsing (except G1 E+ identification)
- [x] NO guesswork or speed comparison detection
- [x] Per-G1 granularity for variable modifiers (Overhang, CurledEdge)
- [x] All modifiers tracked at exact application point
- [x] Data passing mechanism specified (pointer setters)
- [x] Per-layer clearing timing specified
- [x] Memory efficiency addressed (sparse cooling, per-layer clear)
- [x] BaseSpeedType enum for role labels (replaces flags)
- [x] Small Perimeter tracked via state variable
- [x] Curled Edge separated from Overhang
- [x] First Layer included
- [x] SlowDownLayers, VolumetricCap, ResonanceAvoidance, ScarfJoint included
- [x] Layer Cooling tracked directly (not by comparison)
- [x] Variable modifier clearing logic specified (reset_variable_modifiers)

---

## Developer Documentation Guidelines

**IMPORTANT: Instructions for developers implementing this feature**

### Documentation Requirements

Each developer working on this feature MUST create documentation files in `doc/speed_hints/` folder. **Do NOT modify the main design document** (`speed_modifier_tracking_design.md`).

### File Naming Convention

Create files named: `implementation_notes_[your_name]_[date].md` or `implementation_[component]_[date].md`

Example: `implementation_notes_john_2025-01-15.md` or `implementation_gcode_tracking_2025-01-15.md`

### Required Content

Your documentation MUST include:

1. **What was implemented** - Brief description of the component/step completed
2. **Where changes were made** - File paths and approximate line numbers (no full code blocks)
3. **Issues encountered** - Problems faced and how they were resolved
4. **Decisions made** - Any implementation choices and their reasoning
5. **Assumptions** - Any assumptions made during implementation
6. **Deviations** - Any deviations from the design document (see rules below)

### Documentation Format

```markdown
## [Date] - [Component/Step Name]

### Implemented
- Brief description of what was done

### Files Modified
- `path/to/file.cpp` - Added X at lines Y-Z
- `path/to/file.hpp` - Added struct definition

### Issues Encountered
- Issue description and resolution

### Decisions Made
- Decision and reasoning

### Assumptions
- Any assumptions made

### Deviations (if any)
- Deviation description with STRONG justification
```

### CRITICAL: Deviation Policy

**Developers are NOT allowed to deviate from the design document unless there is a critical technical reason.**

Unacceptable reasons for deviation:
- "Simplifies the code"
- "Introduces less complexity"
- "Easier to implement"
- "I think this is better"
- "Not sure why it was designed this way"

Acceptable reasons for deviation:
- The documented approach causes crashes/undefined behavior
- A required API/function does not exist as documented
- The approach violates thread safety in ways not anticipated
- Performance impact is catastrophic (measured, not assumed)

**If you must deviate:**
1. Document the exact deviation
2. Provide the critical technical reason
3. Explain why the original approach cannot work
4. Get approval before proceeding if possible

### No Code Blocks

Do NOT include full code blocks in your documentation. Instead, describe:
- What was added/modified
- The file and approximate location
- Key function/variable names involved

This keeps documentation concise and prevents it from becoming stale as the codebase evolves.
