# PHM-Only Settings (Plate Height Modifiers)

PHM-only settings are settings that appear **only** in Plate Height Modifiers (PHM), NOT in:
- General process settings
- Per-object process settings
- Object layer ranges

## Architecture Overview

PHM-only settings use a hybrid approach:
1. **Storage**: Setting is defined in `PrintRegionConfig` (for layer config range storage)
2. **UI**: Optgroup is added **only** in `TabPrintLayer::build()`, not in `build_print_tab()`
3. **Visibility**: Toggled based on context detection (PartPlate* vs ModelObject*)

## Key Files for PHM-Only Settings

| Purpose | File | Location |
|---------|------|----------|
| Setting definition | `PrintConfig.cpp` | `init_fff_params()` |
| Config class | `PrintConfig.hpp` | `PrintRegionConfig` class (~line 1120) |
| Preset options | `Preset.cpp` | `s_Preset_print_options` (~line 852) |
| PHM-only keys list | `Tab.cpp` | `phm_only_keys` static vector (~line 69) |
| UI optgroup creation | `Tab.cpp` | `TabPrintLayer::build()` (~line 3238) |
| Visibility toggling | `Tab.cpp` | `TabPrintLayer::update_phm_options_visibility()` |
| Context detection | `Tab.cpp` | `TabPrintLayer::set_model_config()` |
| G-code generation | `GCode.cpp` | `process_layer()` for temperature/speed overrides |

## Step-by-Step: Adding a PHM-Only Setting

### Step 1: Define in PrintConfig.cpp

```cpp
// Orca: PHM-only settings (Plate Height Modifiers)
def = this->add("my_phm_setting", coInt);
def->label = L("My PHM Setting");
def->category = L("Others");  // Or appropriate category
def->tooltip = L("This setting only applies to plate height modifiers...");
def->min = 0;
def->max = 100;
def->mode = comAdvanced;
def->set_default_value(new ConfigOptionInt(0));
```

### Step 2: Add to PrintRegionConfig (PrintConfig.hpp)

```cpp
// In PrintRegionConfig class definition (~line 1120)
// Orca: PHM-only settings (Plate Height Modifiers)
((ConfigOptionInt,                  my_phm_setting))
```

**Why PrintRegionConfig?** Layer config ranges store settings in `ModelConfig` which uses `PrintRegionConfig` keys.

### Step 3: Add to Preset Options (Preset.cpp)

```cpp
// In s_Preset_print_options (~line 852)
// PHM-only settings (Plate Height Modifiers)
"my_phm_setting",
```

### Step 4: Add to phm_only_keys (Tab.cpp)

```cpp
// Near line 69
static const std::vector<std::string> phm_only_keys = {
    "nozzle_temperature_override",
    "my_phm_setting",  // Add new PHM-only settings here
};
```

### Step 5: Add UI in TabPrintLayer::build() (Tab.cpp)

**IMPORTANT:** Do NOT add to `build_print_tab()` - that would make it appear in all tabs.

In `TabPrintLayer::build()` (~line 3238), add to the appropriate optgroup:

```cpp
void TabPrintLayer::build()
{
    TabPrintModel::build();

    // Find or create the "Others" page
    PageShp others_page;
    for (auto& page : m_pages) {
        if (page->title() == "Others") {
            others_page = page;
            break;
        }
    }
    if (!others_page) {
        others_page = add_options_page(L("Others"), "custom-gcode_other");
    }

    // Add PHM-only optgroup
    auto optgroup = others_page->new_optgroup(L("My Group"), "icon_name");
    optgroup->append_single_option_line("my_phm_setting");

    // Move to front if desired
    if (others_page->m_optgroups.size() > 1) {
        auto new_group = others_page->m_optgroups.back();
        others_page->m_optgroups.pop_back();
        others_page->m_optgroups.insert(others_page->m_optgroups.begin(), new_group);
    }
}
```

### Step 6: Add Visibility Toggling (Tab.cpp)

In `TabPrintLayer::update_phm_options_visibility()`, add the optgroup title check:

```cpp
if (optgroup->title == "My Group") {
    found_my_group = true;
    if (optgroup->is_activated()) {
        optgroup->Show(m_is_plate_context);
    }
}
```

**CRITICAL:** Always check `optgroup->is_activated()` before calling `Show()` - the sizer may not be ready during early initialization.

### Step 7: Add Legacy Handling (PrintConfig.cpp)

```cpp
// In handle_legacy_composite() (~line 7280)
if (!config.has("my_phm_setting")) {
    const ConfigOption *default_opt = FullPrintConfig::defaults().option("my_phm_setting");
    if (default_opt != nullptr)
        config.set_key_value("my_phm_setting", default_opt->clone());
}
```

### Step 8: Implement G-code Generation (if needed)

For settings that affect G-code (like temperature overrides), add logic in `GCode.cpp`:

```cpp
// In process_layer() (~line 3810)
// Access plate layer config ranges
const auto& plate_ranges = print.plate_layer_config_ranges();
for (const auto& range : plate_ranges) {
    if (print_z >= range.first.first - EPSILON && print_z < range.first.second) {
        if (range.second.has("my_phm_setting")) {
            int value = range.second.opt_int("my_phm_setting");
            // Use the value...
        }
        break;
    }
}
```

## Context Detection Mechanism

`TabPrintLayer` detects PHM vs object layer context by checking the key type in `set_model_config()`:

```cpp
m_is_plate_context = std::any_of(object_configs.begin(), object_configs.end(),
    [](const auto& pair) { return dynamic_cast<PartPlate*>(pair.first) != nullptr; });
```

- **PHM context**: Key is `PartPlate*` → `m_is_plate_context = true` → Show PHM-only options
- **Object layer context**: Key is `ModelObject*` → `m_is_plate_context = false` → Hide PHM-only options

## Key Implementation Details

1. **GUI_ObjectSettings.cpp** (~line 239): When building object configs for PHM, use `PartPlate*` as the key:
   ```cpp
   object_configs.emplace(plate, &plate_layer_ranges.at(height_range));
   ```

2. **Don't call `Show()` during `build()`**: The UI sizers aren't ready. Visibility is set when `set_model_config()` is called.

3. **Always update visibility in `set_model_config()`**: Don't just update on context change - always call `update_phm_options_visibility()` to handle first-time initialization.

## Checklist for PHM-Only Settings

1. [ ] Define in `PrintConfig.cpp` with default value
2. [ ] Add to `PrintRegionConfig` in `PrintConfig.hpp`
3. [ ] Add to `s_Preset_print_options` in `Preset.cpp`
4. [ ] Add to `phm_only_keys` in `Tab.cpp`
5. [ ] Add optgroup in `TabPrintLayer::build()` (NOT in `build_print_tab()`)
6. [ ] Add visibility check in `update_phm_options_visibility()` with `is_activated()` guard
7. [ ] Add to `handle_legacy_composite()` in `PrintConfig.cpp`
8. [ ] Implement G-code generation if setting affects output
9. [ ] Test: Setting appears only in PHM, not in general/per-object/object-layer tabs
