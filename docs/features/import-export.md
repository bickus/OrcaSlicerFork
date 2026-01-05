# Import & Export Features

File format support, import options, and export capabilities.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| G-code Export for BBL Printers | small | Export/Gcode | G-code export enabled for Bambu Lab printers. | v1.4.2 | - | - |
| PrusaSlicer Variables Support | small | Export/Variables | Support for PrusaSlicer-compatible variables in G-code and filenames. | v1.4.3 | - | - |
| Post-Process Scripts | medium | Export/Processing | Enable post-processing scripts for G-code modification. | v1.4.3 | - | - |
| Customizable Output Filename | medium | Export/Naming | Customizable output filename formatting with variables. | v1.2.5.3 | v2.1.0-beta | - |
| First Layer Height Placeholder | tiny | Export/Variables | Added `first_layer_height` placeholder for templates. | v1.6.3 | - | - |
| ColPic Thumbnail Encoding | medium | Export/Thumbnails | Preview image support for QIDI, Elegoo, and similar printer displays. | v1.9.0-alpha | - | - |
| STL Rotation on Import | small | Import/Transform | Models can be rotated during import for better initial positioning. | v1.9.0-alpha | - | [#3116](https://github.com/SoftFever/OrcaSlicer/pull/3116) |
| 3MF Import Options | small | Import/Settings | Prompt to choose whether to import settings from 3MF files. | v2.0.0-beta | - | - |
| Direct Model Import from Platforms | large | Import/Online | Import models directly from Printables, Thingiverse, and MakerWorld (Windows). | v2.1.0-beta | - | - |
| G-code 3MF Extension | small | Export/Format | G-code 3MF files now use `.gcode.3mf` extension for clarity. | v2.3.0 | - | - |
| STEP Import Dialog | medium | Import/CAD | Enhanced STEP file import with granular control over import details. | v2.3.1-alpha | - | [#9102](https://github.com/SoftFever/OrcaSlicer/pull/9102) |
| STL/STEP in Recent Files | small | Import/UI | STL and STEP files now appear in recent file lists. | v2.3.1-beta | - | - |
| Sensitive Data Removal from 3MF | small | Export/Security | Removal of sensitive data from 3MF files for privacy. | v2.3.1-alpha | - | [#9825](https://github.com/SoftFever/OrcaSlicer/pull/9825) |

## File Format Support

### Import Formats
- **STL**: Standard tessellation language
- **OBJ**: Wavefront OBJ with materials
- **3MF**: 3D Manufacturing Format with settings
- **STEP**: CAD format with enhanced dialog ([#9102](https://github.com/SoftFever/OrcaSlicer/pull/9102))
- **AMF**: Additive Manufacturing Format

### Export Formats
- **G-code**: Standard G-code for all printers
- **G-code 3MF**: Combined format with `.gcode.3mf` extension
- **3MF Project**: Full project with settings

## Online Import

### Supported Platforms (Windows, v2.1.0-beta)
- Printables.com
- Thingiverse.com
- MakerWorld.com

Toggle via Preferences to enable/disable direct downloads.

## Filename Variables

### Available Placeholders
- `{input_filename}`: Original model name
- `{layer_height}`: Layer height value
- `{first_layer_height}`: First layer height
- `{filament_type}`: Filament material type
- `{print_time}`: Estimated print time
- `{initial_tool}`: Starting tool number (v1.8.0)
- `{model_name}`: Model name (v2.1.0-beta)
- `{plate_name}`: Plate name (v1.6.0)
- `{plate_number}`: Plate index (v2.1.0-beta)

## Notes

- STEP file importing fixed for files with specific shell types ([#4247](https://github.com/SoftFever/OrcaSlicer/pull/4247))
- 3MF file case-insensitive handling added in v2.2.0-beta2
- Reduced 3MF file sizes for BBL printer transmission ([#9114](https://github.com/SoftFever/OrcaSlicer/pull/9114))
- Large G-code file upload to PrusaLink fixed in v2.1.1
- Filament profile export fix for non-BBL printers ([#4217](https://github.com/SoftFever/OrcaSlicer/pull/4217))
