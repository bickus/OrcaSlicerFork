#ifndef slic3r_SpeedModifier_hpp_
#define slic3r_SpeedModifier_hpp_

#include <cstdint>

namespace Slic3r {

// Speed modifier entry - records a single speed modification applied to a G1 command
struct SpeedModifierEntry {
    enum class Type : uint8_t {
        None = 0,
        FirstLayer,           // Initial layer speed limit
        SlowDownLayers,       // Gradual speed increase over first N layers
        Overhang,             // Overhang slowdown (variable per point)
        LayerTimeCooling,     // Minimum layer time cooling slowdown
        VolumetricCap,        // Filament max volumetric speed limit
        ResonanceAvoidance,   // Resonance avoidance speed limit
        SmallPerimeter,       // Small perimeter slowdown
        ScarfJoint,           // Scarf joint speed
        CurledEdge,           // Curled edge slowdown
        Bridge,               // Bridge speed
        GapFill               // Gap fill speed
    };

    Type type{ Type::None };
    float value{ 0.0f };        // Modifier-specific value (% for overhang, mm/s reduction for others)
    float speed_after{ 0.0f };  // Speed after this modifier was applied (mm/s)
};

// Base speed type - determines the label shown in UI ("Bridge:", "Support:", etc.)
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

// Speed modifier data for G1 extrusion tracking
// Stores base speed, type, and modifiers applied to a G1 command
struct G1ModifierData {
    float base_speed{ 0.0f };
    SpeedModifierEntry modifiers[5];  // Max 5 modifiers
    uint8_t count{ 0 };
    BaseSpeedType base_type{ BaseSpeedType::Normal };

    void add_modifier(SpeedModifierEntry::Type type, float value, float speed_after) {
        if (count < 5) {
            modifiers[count++] = { type, value, speed_after };
        }
    }

    void reset() {
        base_speed = 0.0f;
        count = 0;
        base_type = BaseSpeedType::Normal;
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

// Records the original and new feedrate for a G1 that was slowed by CoolingBuffer
struct CoolingModification {
    float original_feedrate;  // Feedrate before slowdown (mm/s)
    float new_feedrate;       // Feedrate after slowdown (mm/s)
};

} // namespace Slic3r

#endif // slic3r_SpeedModifier_hpp_
