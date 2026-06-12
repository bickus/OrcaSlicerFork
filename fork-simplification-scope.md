# Fork Simplification Scope

Context: this fork is intended to become a focused single-nozzle, single-filament Voron/Klipper FFF slicer, not a general-purpose OrcaSlicer-compatible distribution.

## Product Target

- Single-nozzle, single-filament Voron/Klipper FFF slicing.
- Preserve local G-code export.
- Preserve or add Moonraker upload/integration, especially newer upstream Orca Moonraker support.
- Keep useful model-prep tools.
- No requirement to preserve full Orca project/profile compatibility.

## Keep For Now

- STEP/OpenCASCADE import.
- Multi-plate workflow.
- Cut and advanced cut tools.
- Emboss, SVG, and text tools.
- General FFF slicing/model-prep features.
- Voron/Klipper-relevant calibration generators:
  - Pressure advance.
  - Flow rate.
  - Temperature tower.
  - Volumetric speed.
  - Retraction.
  - Input shaping, if it emits useful local test G-code.

## Remove Or Consider Removing

### 1. SLA

- Strong candidate for removal.
- Mostly structurally separate from FFF.
- Not relevant to Voron FFF.
- Main areas:
  - `src/libslic3r/SLAPrint.*`
  - `src/libslic3r/SLA/`
  - `src/libslic3r/Format/SL1.*`
  - `src/slic3r/GUI/Gizmos/GLGizmoSlaSupports.*`
  - `src/slic3r/GUI/Gizmos/GLGizmoHollow.*`
  - `tests/sla_print/`

### 2. Bambu Cloud / Device / Account / Monitoring

- Strong candidate for removal.
- Large GUI/network subsystem not needed for Voron/Moonraker.
- Likely one of the biggest simplification opportunities.
- Needs care because `DeviceManager` is referenced broadly from GUI code.
- Main areas:
  - `src/slic3r/GUI/DeviceManager.*`
  - `src/slic3r/Utils/NetworkAgent.*`
  - `src/slic3r/GUI/BindDialog.*`
  - `src/slic3r/GUI/Monitor*.cpp`
  - `src/slic3r/GUI/MediaFilePanel.*`
  - `src/slic3r/GUI/CameraPopup.*`
  - `src/slic3r/GUI/BBL*.cpp`
  - `src/slic3r/GUI/HMSPanel.*`
  - `src/slic3r/GUI/Printer/BambuTunnel.*`
  - `src/slic3r/GUI/BambuPlayer/`

### 3. Online Content / Model Mall / Login / Web Flows

- Good candidate for removal.
- Mostly unrelated to focused local Voron slicing.
- Helps privacy/offline behavior and reduces web-resource maintenance.
- Main areas:
  - `src/slic3r/GUI/WebViewDialog.*`
  - `src/slic3r/GUI/WebGuideDialog.*`
  - `src/slic3r/GUI/WebDownPluginDlg.*`
  - `src/slic3r/GUI/DailyTips.*`
  - `src/slic3r/GUI/ReleaseNote.*`
  - `src/slic3r/GUI/UpgradePanel.*`
  - `src/slic3r/GUI/PrivacyUpdateDialog.*`
  - `src/slic3r/GUI/ModelMall.*`
  - `resources/web/`

### 4. AMS UI

- Remove together with Bambu/MMU work.
- Not relevant to single-filament Voron slicing.
- Main areas:
  - `src/slic3r/GUI/AMSSetting.*`
  - `src/slic3r/GUI/AMSMaterialsSetting.*`
  - `src/slic3r/GUI/AmsWidgets.*`
  - `src/slic3r/GUI/AmsMappingPopup.*`
  - AMS/device references in `DeviceManager`, `SelectMachine`, and calibration UI.

### 5. Non-Moonraker / Non-OctoPrint Host Backends

- Good low-risk cleanup candidate because the print-host layer is factory-based.
- Keep or add:
  - Moonraker.
  - Local export.
  - Possibly OctoPrint as fallback.
- Remove or disable if irrelevant:
  - FlashAir.
  - AstroBox.
  - Repetier.
  - MKS.
  - ESP3D.
  - CrealityPrint.
  - Obico.
  - Flashforge.
  - SimplyPrint.
  - ElegooLink.
  - PrusaLink/PrusaConnect, if not useful.
- Main areas:
  - `src/slic3r/Utils/PrintHost.*`
  - `src/slic3r/Utils/OctoPrint.*`
  - `src/slic3r/Utils/Duet.*`
  - `src/slic3r/Utils/Repetier.*`
  - Other host implementation files under `src/slic3r/Utils/`.
  - `src/slic3r/GUI/PrintHostDialogs.*`

### 6. MMU / Multi-Material / Wipe Tower

- Strong candidate conceptually, but technically high-risk.
- It is deeply mixed into FFF G-code generation.
- Best handled as staged deactivation first, then deletion.
- Target behavior: enforce single nozzle, single filament, one active extruder/material.
- Main areas:
  - `src/libslic3r/GCode/WipeTower.*`
  - `src/libslic3r/GCode/WipeTower2.*`
  - `src/libslic3r/GCode/ToolOrdering.*`
  - `src/libslic3r/FlushVolCalc.*`
  - `src/libslic3r/MultiMaterialSegmentation.*`
  - `src/slic3r/GUI/Gizmos/GLGizmoMmuSegmentation.*`
  - MMU/wipe/flush config keys in `src/libslic3r/PrintConfig.*`
  - MMU serialization in `src/libslic3r/Format/3mf.cpp` and `src/libslic3r/Format/bbs_3mf.cpp`

### 7. Bambu-Specific Calibration Upload / Results

- Remove cloud/device upload and Bambu result plumbing.
- Keep local calibration G-code generators that are useful for Voron/Klipper.
- Main areas:
  - Bambu-specific pieces in `src/slic3r/GUI/Calibration*`
  - Device/cloud interactions from calibration workflows.
  - `PACalibResult`, `X1CCalibInfos`, and related upload/result paths.

### 8. BBS Extended 3MF

- Candidate for later removal or reduction because full Orca compatibility is not required.
- High risk because it is likely the normal Orca project format and carries many metadata paths.
- Defer until after SLA, Bambu/cloud, web, and MMU policy are clearer.
- Main area:
  - `src/libslic3r/Format/bbs_3mf.cpp`

## Revised Priority Ranking

1. SLA.
2. Bambu cloud/device/account/monitoring.
3. Online content / model mall / login / web flows.
4. AMS UI.
5. Non-Moonraker/non-OctoPrint host backends.
6. MMU / wipe tower / multi-material, staged carefully.
7. Bambu-specific calibration upload/results.
8. BBS extended 3MF, later and carefully.

## Suggested Implementation Philosophy

- Prefer staged hard-disablement before deleting deeply coupled code.
- Preserve buildability after each removal batch.
- Remove UI entry points before removing core code when that reduces reachable behavior safely.
- Keep internal vectorized/extruder-aware APIs where they are harmless, but enforce one active extruder/material at product boundaries.
- For project import, either reject unsupported multi-material/Bambu projects clearly or load a reduced single-material subset with warnings.
- Do not adopt upstream main-view printer/nozzle/filament selector redesign while doing this cleanup; see `UI1` in `upstream-2.4-interesting-backlog.md`.
