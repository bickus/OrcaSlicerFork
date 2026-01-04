---
name: orcaslicer-settings
description: |
  Guide for adding new or editing existing settings in OrcaSlicer process/printer/filament profiles.
  IMPORTANT: This skill researches similar existing settings first and provides recommendations based
  on how they are implemented (types, defaults, config classes, fallbacks) before asking clarification
  questions. Use when asked to: (1) Add a new setting/parameter/option to print/process profiles,
  (2) Add a new setting to printer profiles, (3) Add a new setting to filament profiles, (4) Edit
  existing setting properties (defaults, limits, visibility), (5) Add enum types for dropdown settings,
  (6) Handle legacy/migration for renamed or removed settings, (7) Control setting visibility based on
  other settings, (8) Make settings available for per-object or per-modifier overrides.
---

# OrcaSlicer Settings System

## CRITICAL: Research First, Then Clarify

Before implementing a new setting, ALWAYS:

1. **Search for similar existing settings** in `PrintConfig.cpp` to understand patterns
2. **Provide recommendations** based on how similar settings are implemented
3. **Show examples** from the codebase when asking clarification questions

### Research Process

Use Grep to find similar settings. For example, if adding an acceleration setting:
```
Grep pattern: "_acceleration" in PrintConfig.cpp
```

Then read the definitions to understand:
- What type they use (coFloat vs coFloatOrPercent)
- What their defaults are
- What config class they belong to
- How they handle fallbacks

### Clarification Questions with Recommendations

Use `AskUserQuestion` to clarify each of these, providing **recommendations based on similar settings**:

1. **Setting type** - Show what similar settings use
   - Example: "Similar acceleration settings like `outer_wall_acceleration` use `coFloat`. Should this also be `coFloat`, or `coFloatOrPercent` like `bridge_acceleration` (which allows percentages)?"

2. **Default value** - Show what similar settings default to
   - Example: "Similar settings use these defaults: `outer_wall_acceleration=500`, `bridge_acceleration=50%`, `top_surface_acceleration=500`. Recommendation: 0 means 'use default logic'. What default would you prefer?"

3. **Profile type** - Infer from context if possible
   - Example: "This appears to be a process setting like other acceleration settings. Confirm: Process, Printer, or Filament?"

4. **Override level** - Show what similar settings use (for process settings)
   - Example: "Similar acceleration settings are in `PrintObjectConfig` (global + per-object). Options: Global only (`PrintConfig`), Per-object (`PrintObjectConfig`), or Per-modifier (`PrintRegionConfig`)?"

5. **Tooltip** - Offer to generate based on similar settings' patterns
   - Example: "Similar settings use tooltips like 'Acceleration for X regions. Set to 0 to use default acceleration.' Should I generate a similar tooltip, or do you have specific text?"

6. **Fallback behavior** - Show if similar settings use fallbacks
   - Example: "`bridge_acceleration` uses `ratio_over='outer_wall_acceleration'` for percentage mode. Should this setting have any fallback behavior?"

7. **Visibility dependencies** - Check if similar settings have visibility toggling
   - Example: "No visibility toggling found for similar acceleration settings. Should this setting be conditionally hidden?"

### Example Research Output

When adding "overhang_acceleration", research might show:
```
Found similar acceleration settings:
- outer_wall_acceleration: coFloat, default=500, PrintObjectConfig
- bridge_acceleration: coFloatOrPercent, default=50%, ratio_over=outer_wall_acceleration
- top_surface_acceleration: coFloat, default=500, PrintObjectConfig
- travel_acceleration: coFloat, default=0, PrintObjectConfig

Recommendation: Use coFloat with default=0, in PrintObjectConfig (like travel_acceleration)
```

## Profile Type References

**IMPORTANT:** Choose the appropriate reference based on the setting type:

- **Process settings** - See [references/process-settings.md](references/process-settings.md)
- **Filament settings** - See [references/filament-settings.md](references/filament-settings.md)
- **Printer settings** - See [references/printer-settings.md](references/printer-settings.md)

Each reference contains profile-specific guidance on config classes, preset arrays, GUI tabs, and visibility toggling.

## Quick Reference - Key Files (All Types)

| Purpose | File |
|---------|------|
| Setting definition | `src/libslic3r/PrintConfig.cpp` |
| Config class declarations | `src/libslic3r/PrintConfig.hpp` |
| Preset option lists | `src/libslic3r/Preset.cpp` |
| GUI tabs | `src/slic3r/GUI/Tab.cpp` |
| Visibility toggling | `src/slic3r/GUI/ConfigManipulation.cpp` |
| Legacy project loading | `src/libslic3r/Format/3mf.cpp`, `bbs_3mf.cpp` |

## Setting Types (All Profile Types)

```cpp
coFloat, coFloats           // Single/vector floats
coInt, coInts               // Single/vector integers
coBool, coBools             // Single/vector booleans
coString, coStrings         // Single/vector strings
coPercent, coPercents       // Float as percentage (0-100)
coFloatOrPercent            // Can be "5.0" or "50%"
coPoint, coPoints           // Vec2d (X,Y coordinates)
coEnum                      // Dropdown with string->int mapping
```

**Note:** Use vector types (`coFloats`, `coInts`, etc.) for per-extruder settings in filament/printer profiles.

## Adding a New Setting (General Steps)

### Step 1: Define in PrintConfig.cpp

In `PrintConfigDef::init_fff_params()`:

```cpp
def = this->add("my_new_setting", coFloat);
def->label = L("My Setting Label");
def->category = L("Quality");
def->tooltip = L("Description shown on hover");
def->sidetext = "mm";
def->min = 0;
def->max = 100;
def->mode = comAdvanced;  // comSimple, comAdvanced, or comDevelop
def->set_default_value(new ConfigOptionFloat(5.0));
```

### Step 2: Add to Config Class (PrintConfig.hpp)

- **Process settings** → See [process-settings.md](references/process-settings.md) for config class
- **Filament settings** → See [filament-settings.md](references/filament-settings.md) for config class
- **Printer settings** → See [printer-settings.md](references/printer-settings.md) for config class

### Step 3: Add to Preset Options (Preset.cpp)

- **Process** → `s_Preset_print_options` (~line 792)
- **Filament** → `s_Preset_filament_options` (~line 854)
- **Printer** → `s_Preset_printer_options` (~line 898)

### Step 4: Add to GUI Tab (Tab.cpp)

- **Process** → `TabPrint::build()` (~line 2113)
- **Filament** → `TabFilament::build()` (~line 3431)
- **Printer** → `TabPrinter::build_fff()` (~line 3916)

### Step 5: Add Legacy Handling (CRITICAL)

Add to `handle_legacy_composite()` to prevent crashes when loading old projects.

## CRITICAL: Preventing Crashes on Legacy Projects

When adding a new setting, OLD projects without the setting WILL crash unless properly handled.

### Required Steps for ALL New Settings

1. **Always set a default value** in PrintConfig.cpp:
```cpp
def->set_default_value(new ConfigOptionFloat(5.0));
```

2. **Add to `handle_legacy_composite()`** (PrintConfig.cpp ~line 7119):
```cpp
void PrintConfigDef::handle_legacy_composite(DynamicPrintConfig &config)
{
    if (!config.has("my_new_setting")) {
        const ConfigOption *default_opt = FullPrintConfig::defaults().option("my_new_setting");
        if (default_opt != nullptr)
            config.set_key_value("my_new_setting", default_opt->clone());
    }
}
```

3. **For 3MF project loading**, add to `handle_legacy_project_loaded()` in `3mf.cpp` (~line 3253) and `bbs_3mf.cpp` (~line 8081) if special migration is needed:
```cpp
if (! config.has("my_new_setting")) {
    auto *opt = config.option<ConfigOptionFloat>("my_new_setting", true);
    opt->value = 5.0;
}
```

## Fallback Values Pattern

### Using `ratio_over` (Percentage of Another Setting)

```cpp
def = this->add("my_line_width", coFloatOrPercent);
def->ratio_over = "nozzle_diameter";  // 50% means 50% of nozzle_diameter
def->set_default_value(new ConfigOptionFloatOrPercent(0, false));
```

Common targets: `nozzle_diameter`, `layer_height`, `line_width`, `outer_wall_speed`

### Zero-Value Fallback

Handle in usage code where the setting is read:
```cpp
double width = config.opt_float("bridge_infill_line_width");
if (width == 0)
    width = config.opt_float("internal_solid_infill_line_width");
```

## Adding Enum Settings

### Step 1: Define Enum Type (PrintConfig.hpp)

```cpp
enum class MyEnumType { OptionA, OptionB, OptionC };
```

### Step 2: Create Key Map (PrintConfig.cpp, top of file)

```cpp
static t_config_enum_values s_keys_map_MyEnumType {
    { "option_a", int(MyEnumType::OptionA) },
    { "option_b", int(MyEnumType::OptionB) },
};
CONFIG_OPTION_ENUM_DEFINE_STATIC_MAPS(MyEnumType)
```

### Step 3: Define the Setting

```cpp
def = this->add("my_enum_setting", coEnum);
def->enum_keys_map = &ConfigOptionEnum<MyEnumType>::get_enum_values();
def->enum_values.push_back("option_a");
def->enum_labels.push_back(L("Option A"));
def->set_default_value(new ConfigOptionEnum<MyEnumType>(MyEnumType::OptionA));
```

## Handling Legacy/Migration

### Renaming Settings (PrintConfig.cpp ~line 6952)

```cpp
void PrintConfigDef::handle_legacy(t_config_option_key &opt_key, std::string &value)
{
    if (opt_key == "old_setting_name") {
        opt_key = "new_setting_name";
    }
}
```

### Ignoring Removed Settings

```cpp
static std::set<std::string> ignore = { "removed_setting" };
```

## Checklist for New Settings

1. [ ] Define in `PrintConfig.cpp` with default value
2. [ ] Add to config class in `PrintConfig.hpp` (see Steps 2 above)
3. [ ] Add to preset options in `Preset.cpp` (see Step 3 above)
4. [ ] Add to GUI tab in `Tab.cpp` (see Step 4 above)
5. [ ] **Add to `handle_legacy_composite()` to prevent crashes**
6. [ ] Add visibility toggling in `ConfigManipulation.cpp` if conditional
7. [ ] For enums: define type and key map
8. [ ] Test loading old project files
