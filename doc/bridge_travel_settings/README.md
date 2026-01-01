# Bridge Travel Settings

## Overview

This feature adds two new settings to control acceleration and jerk for travel moves that occur immediately before or after bridge and overhang extrusions:

- **Travel around bridges Acceleration** (`bridge_travel_acceleration`)
- **Travel around bridges Jerk** (`bridge_travel_jerk`)

## Purpose

When the print head travels to or from a bridge/overhang area, sudden accelerations and decelerations can cause vibrations that affect print quality. By using lower acceleration and jerk values for these specific travel moves, you can reduce vibrations and improve the quality of bridges and overhangs.

## Settings

### Travel around bridges Acceleration

- **Location**: Print Settings > Speed > Acceleration
- **Default**: 0 (uses regular Travel acceleration)
- **Unit**: mm/s²

When set to a value greater than 0, this acceleration will be used for travel moves that:
- Move TO a bridge or overhang extrusion (travel before bridge)
- Move FROM a bridge or overhang extrusion (travel after bridge)

### Travel around bridges Jerk

- **Location**: Print Settings > Speed > Jerk(XY)
- **Default**: 0 (uses regular Travel jerk)
- **Unit**: mm/s

When set to a value greater than 0, this jerk will be used for travel moves that:
- Move TO a bridge or overhang extrusion (travel before bridge)
- Move FROM a bridge or overhang extrusion (travel after bridge)

## Fallback Behavior

- If the value is set to 0 (default), the regular Travel acceleration/jerk settings are used
- This ensures backward compatibility with existing profiles and projects
- Legacy projects without these settings will automatically use regular travel settings

## Affected Extrusion Types

These settings apply to travel moves adjacent to any of the following extrusion types:

| Extrusion Type | Description |
|----------------|-------------|
| Bridge Infill | External/bottom bridge infill |
| Internal Bridge Infill | Bridge over internal sparse infill |
| Extra Bridge Infill | Second layer after external bridge |
| Extra Internal Bridge Infill | Second layer after internal bridge |
| Overhang Perimeter | Walls/perimeters that overhang |

## Firmware Support

The settings work with all supported firmware types:

- **Klipper**: Uses `SET_VELOCITY_LIMIT ACCEL=... SQUARE_CORNER_VELOCITY=...`
- **Marlin**: Uses `M204 T...` for acceleration and `M205 X... Y...` for jerk
- **RepRap Firmware**: Uses `M204 T...` for acceleration and `M205 X... Y...` for jerk
- **Repetier**: Uses `M202 X... Y...` for acceleration and `M205 X... Y...` for jerk

## Implementation Details

The detection of "travel around bridges" is based on:
1. The extrusion role of the path that will be printed AFTER the travel move
2. The extrusion role of the path that was printed BEFORE the travel move

If either the previous or next extrusion is a bridge or overhang type, the bridge travel settings are used (if they are non-zero).

## Recommended Usage

1. Start with values slightly lower than your regular travel acceleration/jerk
2. Test on a model with bridges or overhangs
3. Reduce values further if you see improvements in bridge quality
4. Note that lower values will increase print time for models with many bridges

## Files Modified

- `src/libslic3r/PrintConfig.cpp` - Setting definitions
- `src/libslic3r/PrintConfig.hpp` - Setting declarations
- `src/libslic3r/Preset.cpp` - Backward compatibility
- `src/libslic3r/GCode.cpp` - Logic in `travel_to()` function
- `src/slic3r/GUI/Tab.cpp` - UI integration
