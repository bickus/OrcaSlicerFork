# Process (Print) Settings Reference

## Overview

Process settings control how objects are sliced and printed. Each setting belongs to exactly ONE config class, which determines its override granularity.

## Config Class Determines Override Level

**CRITICAL:** Each setting can only be in ONE config class. The class determines where overrides are available:

| Config Class | Override Available At | Examples |
|-------------|----------------------|----------|
| `PrintObjectConfig` | Global + Per-Object | layer_height, brim, supports, acceleration |
| `PrintRegionConfig` | Global + Per-Object + Per-Modifier | wall_loops, infill, speeds, fuzzy skin |
| `PrintConfig` | Global ONLY | initial_layer_line_width, nozzle_diameter |

### PrintObjectConfig (Per-Object Overrides)

Located in `PrintConfig.hpp` ~line 815. Settings here appear in:
- Process Settings tab (global)
- Per-object settings panel (right-click object → Edit Object Settings)

```cpp
PRINT_CONFIG_CLASS_DEFINE(
    PrintObjectConfig,
    ((ConfigOptionFloat, my_per_object_setting))
)
```

**Current settings include:** layer_height, line_width, brim settings, support settings, seam position, tree support params, accelerations, jerks, elephant foot compensation.

### PrintRegionConfig (Per-Modifier Overrides)

Located in `PrintConfig.hpp` ~line 970. Settings here appear in:
- Process Settings tab (global)
- Per-object settings panel
- Per-modifier settings (Edit Modifier Settings)

```cpp
PRINT_CONFIG_CLASS_DEFINE(
    PrintRegionConfig,
    ((ConfigOptionFloat, my_per_modifier_setting))
)
```

**Current settings include:** wall_loops, sparse_infill_density, sparse_infill_pattern, speeds (outer_wall_speed, inner_wall_speed, sparse_infill_speed), fuzzy skin, ironing, overhang speeds.

### PrintConfig (Global Only)

Located in `PrintConfig.hpp` ~line 1292. Settings here appear ONLY in Process Settings tab, no per-object or per-modifier overrides.

**Current settings include:** initial_layer_line_width, initial_layer_print_height, initial_layer_speed, nozzle_diameter, bed temperatures.

## Deciding Which Class to Use

Ask these questions:

1. **Does this setting make sense to vary per modifier volume?**
   - YES → Use `PrintRegionConfig`
   - NO → Continue to question 2

2. **Does this setting make sense to vary per object?**
   - YES → Use `PrintObjectConfig`
   - NO → Use `PrintConfig`

**Examples:**
- "Bridge flow ratio" → Varies by region → `PrintRegionConfig`
- "Support angle" → Same for whole object → `PrintObjectConfig`
- "Initial layer height" → Same for entire print → `PrintConfig`

## Preset Options Array

Add to `s_Preset_print_options` in `Preset.cpp` (~line 792):

```cpp
static std::vector<std::string> s_Preset_print_options {
    // ... existing options ...
    "my_new_setting",
};
```

## GUI Tab Structure

Located in `Tab.cpp`, `TabPrint::build()` (~line 2113).

### Main Pages

- Quality - Layer height, line width, seam, precision, ironing
- Strength - Walls, top/bottom shells, infill
- Speed - Speed settings, acceleration, jerk
- Support - Support settings, tree support
- Others - Skirt/Brim, prime tower, special modes

### Adding to GUI

```cpp
page = add_options_page(L("Quality"), "custom-gcode_quality");
optgroup = page->new_optgroup(L("My Group"), L"param_my_icon");
optgroup->append_single_option_line("my_setting", "docs_page#anchor");
```

## Defaults and Fallbacks

### Default Values
Always set a sensible default that produces valid slicing results:
```cpp
def->set_default_value(new ConfigOptionFloat(0.2));  // layer_height
def->set_default_value(new ConfigOptionInt(2));     // wall_loops
```

### Line Width Fallback Chain
Line width settings follow this fallback pattern when value is 0:
1. Specific line width (e.g., `bridge_infill_line_width`)
2. Parent line width (e.g., `internal_solid_infill_line_width`)
3. Base `line_width`
4. Auto-calculated from `nozzle_diameter`

### Speed Fallback
Speed settings with value 0 typically mean "use calculated default" or "no limit".

### Using `ratio_over` for Percentage Settings
```cpp
def->ratio_over = "nozzle_diameter";   // For line widths
def->ratio_over = "outer_wall_speed";  // For related speeds
def->ratio_over = "layer_height";      // For height-dependent settings
```

## Visibility Toggling

Add in `ConfigManipulation.cpp`, `toggle_print_fff_options()` (~line 534):

```cpp
bool has_feature = config->opt_bool("enable_my_feature");
toggle_line("my_feature_amount", has_feature);

// Based on enum
auto mode = config->opt_enum<MyMode>("my_mode");
toggle_line("mode_a_setting", mode == MyMode::ModeA);
```
