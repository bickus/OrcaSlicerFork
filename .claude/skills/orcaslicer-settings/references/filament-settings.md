# Filament Settings Reference

## Overview

Filament settings define material-specific properties. They are per-filament (multi-extruder aware) with no per-object/modifier overrides.

## Config Classes

Filament settings are defined in:
- **GCodeConfig** (~line 1160 in PrintConfig.hpp): Flow ratios, retraction, pressure advance, firmware settings
- **PrintConfig** (~line 1292): Temperatures, fan speeds, cooling parameters

Add settings to the appropriate class based on category.

## Key Difference: Vector Types

Most filament settings use **vector types** (`coFloats`, `coInts`, `coBools`) to support multi-extruder setups:

```cpp
def = this->add("my_filament_setting", coFloats);  // NOT coFloat
def->set_default_value(new ConfigOptionFloats{ 200.0 });  // NOT ConfigOptionFloat
```

## Defaults and Fallbacks

### Default Values
Filament settings typically default to reasonable values for common materials:
```cpp
def->set_default_value(new ConfigOptionInts{ 200 });  // Temperature default
def->set_default_value(new ConfigOptionFloats{ 0 });  // 0 often means "use printer default"
```

### Fallback Pattern (0 = Use Other Setting)
Many filament settings use 0 to indicate "use the printer/process setting":
- `filament_retraction_length = 0` → use printer's `retraction_length`
- Temperature settings of 0 → filament doesn't support that bed type

This is handled in code when the setting is used, not in config definition.

## Dependencies

Filament settings often depend on printer capabilities:
- Pressure advance requires firmware support
- Chamber temperature requires `support_chamber_temp_control` on printer
- Air filtration requires `support_air_filtration` on printer

Visibility toggling in `toggle_filament_options()` handles these.

## Preset Options Array

Add to `s_Preset_filament_options` in `Preset.cpp` (~line 854).

## GUI Tab

Located in `Tab.cpp`, `TabFilament::build()` (~line 3431).

**Pages:** Filament, Cooling, Setting Overrides

## Visibility Toggling

Add in `ConfigManipulation.cpp`, `toggle_filament_options()`.
