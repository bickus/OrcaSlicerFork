# Printer Settings Reference

## Overview

Printer settings define hardware capabilities and machine-specific configurations. They are global with no per-object/modifier overrides.

## Config Classes

Printer settings are defined in:
- **MachineEnvelopeConfig** (~line 1131 in PrintConfig.hpp): Machine limits (max speeds, accelerations, jerks)
- **GCodeConfig** (~line 1160): G-code templates, firmware settings, retraction defaults
- **PrintConfig** (~line 1292): Bed configuration, nozzle settings, clearances

Add settings to the appropriate class based on category.

## Key Characteristics

- Define physical machine properties (bed size, nozzle, clearances)
- Include G-code templates (start/end G-code)
- Machine limits (max speeds, accelerations)
- Per-extruder settings use vector types

## Preset Options Arrays

**Main printer options** - Add to `s_Preset_printer_options` in `Preset.cpp` (~line 898):
- printable_area, printable_height, gcode_flavor
- nozzle_type, nozzle_diameter, extruder settings
- G-code templates, host settings

**Machine limits** - Add to `s_Preset_machine_limits_options` (~line 886):
- machine_max_acceleration_*, machine_max_speed_*
- machine_max_jerk_*, machine_min_*_rate

## Defaults and Fallbacks

### Hardware Defaults
Printer settings typically have safe defaults that work for most machines:
```cpp
def->set_default_value(new ConfigOptionFloat(250.0));  // printable_height
def->set_default_value(new ConfigOptionFloats{ 0.4 }); // nozzle_diameter
```

### Per-Extruder Settings
Extruder-specific settings use vector types indexed by extruder:
```cpp
def = this->add("nozzle_diameter", coFloats);
def->set_default_value(new ConfigOptionFloats{ 0.4 });
```

### Capability Flags
Printer capabilities affect which settings are available elsewhere:
- `support_chamber_temp_control` → enables chamber temp in filament
- `support_air_filtration` → enables air filtration in filament
- `gcode_flavor` → affects which G-code features are available

## GUI Tab

Located in `Tab.cpp`, `TabPrinter::build_fff()` (~line 3916).

**Pages:** Basic information, Machine G-code, Motion ability, Extruder

## Visibility Toggling

Add in `ConfigManipulation.cpp`, `toggle_printer_fff_options()`.
