# OrcaSlicer Features Documentation

This documentation provides a comprehensive overview of all features available in OrcaSlicer, compiled from release notes spanning v1.0 through v2.3.1.

## Feature Categories

| Category | Description | Feature Count |
|----------|-------------|---------------|
| [Bed & Build Surface](bed-surface.md) | Build plate settings and bed mesh features | 8 |
| [Bridging](bridging.md) | Bridge detection, flow, and enhancement features | 10 |
| [Brim & Skirt](brim-skirt.md) | Adhesion helpers and outline features | 8 |
| [Calibration](calibration.md) | Built-in calibration tools for tuning print parameters | 12 |
| [Connectivity](connectivity.md) | Network printing and cloud integrations | 10 |
| [Flow & Extrusion](flow-extrusion.md) | Extrusion control, retraction, and flow compensation | 14 |
| [Gizmos & Tools](gizmos-tools.md) | Model editing and manipulation tools | 10 |
| [Import & Export](import-export.md) | File format support and export options | 12 |
| [Infill](infill.md) | Infill patterns, density controls, and internal structures | 15 |
| [Motion & Speed](motion-speed.md) | Speed control, acceleration, and motion optimization | 18 |
| [Multi-Material](multi-material.md) | Multi-tool, MMU, and color printing features | 12 |
| [Object Management](object-management.md) | Object handling, plate management, and print order | 12 |
| [Printer Profiles](printer-profiles.md) | Profile management and printer support | 8 |
| [Seam & Surface Quality](seam-quality.md) | Seam control, fuzzy skin, and surface finish features | 14 |
| [Support Structures](support.md) | Support generation algorithms and settings | 8 |
| [UI & Visualization](ui-visualization.md) | User interface features and G-code preview | 18 |
| [Walls & Perimeters](walls-perimeters.md) | Wall generation, ordering, and perimeter features | 16 |

## Reading the Feature Tables

Each category page contains a table with the following columns:

| Column | Description |
|--------|-------------|
| **Feature** | Short name of the feature |
| **Scale** | Complexity/impact indicator: `tiny`, `small`, `medium`, `large`, `xlarge` |
| **Category** | Category and subcategory path |
| **Description** | Brief 1-2 sentence explanation |
| **Added** | Version where the feature was first introduced |
| **Modified** | Versions where significant changes were made (if any) |
| **PRs** | Link(s) to relevant Pull Requests (where available) |

### Scale Definitions

- **tiny**: Minor UI tweak or small parameter addition
- **small**: Single-purpose feature with limited scope
- **medium**: Feature with moderate complexity affecting one area
- **large**: Significant feature affecting multiple areas or requiring substantial implementation
- **xlarge**: Major feature representing a fundamental capability or algorithm

## Version History Overview

| Version | Release Date | Major Highlights |
|---------|--------------|------------------|
| v2.3.1 | Oct 2024 | Extra solid infill insertion, infill rotation templates, input shaping calibration |
| v2.3.0 | Mar 2024 | User profile backup, brim for holes, crosshatch default infill |
| v2.2.0 | Sep 2024 | Multi-tool printer support, YOLO flow calibration, adaptive PA, multi-material interlocking |
| v2.1.0 | Jun 2024 | Enhanced fuzzy skin, icon redesign, spacemouse support |
| v2.0.0 | Feb 2024 | Scarf joint seam, small area flow compensation, OTA profiles, adaptive bed mesh |
| v1.9.0 | Jan 2024 | Smooth spiral vase mode, Measure/Cut/Emboss gizmos, Cancel Objects |
| v1.8.0 | Nov 2023 | Thick internal bridges, arc fitting improvements |
| v1.7.0 | Sep 2023 | SEMM support, organic tree supports, Marlin 2 flavor |
| v1.6.0 | Apr 2023 | Plate naming, Klipper Exclude Objects, handy calibration models |
| v1.5.0 | Mar 2023 | Rebranding from BambuStudio-SoftFever, G-code window |
| v1.4.0 | Dec 2022 | Layer time display, sandwich mode, flow rate calibration |

## Quick Links

- [OrcaSlicer GitHub Repository](https://github.com/SoftFever/OrcaSlicer)
- [Official Release Notes](https://github.com/SoftFever/OrcaSlicer/releases)
- [Wiki Documentation](https://github.com/SoftFever/OrcaSlicer/wiki)

---

*This documentation was compiled from all OrcaSlicer release notes. For the most up-to-date information, refer to the official release notes.*
