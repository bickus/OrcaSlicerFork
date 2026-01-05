# UI & Visualization Features

User interface features, G-code preview, and visualization options.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| ISO Preview Angle | small | UI/Preview | Adjustable isometric preview angle for 3D viewport. | v1.3.1 | - | - |
| Fan Speed Preview | small | UI/Preview | Fan speed visualization mode in G-code preview. | v1.3.2 | - | - |
| Layer Time Visualization | medium | UI/Preview | Enhanced layer time display with estimated print time in ruler. | v1.4.1 | v1.8.0-beta | - |
| G-code Window | medium | UI/Viewer | Toggle G-code window with `C` key for viewing generated code during preview. | v1.5.0 | - | - |
| Tab Key Switching | tiny | UI/Navigation | Tab key switches between Prepare and Preview tabs. | v1.5.0 | - | - |
| Thinner Sliders | tiny | UI/Appearance | Slimmer slider design for cleaner interface. | v1.7.0-beta | - | - |
| Transparent Shell Display | small | UI/Preview | Transparent shell display in G-code preview for better toolpath inspection. | v1.7.0-beta | - | - |
| Space Mouse Support | small | UI/Input | Space mouse integration with axis inversion option (CTRL-M). | v1.7.0-beta | v2.1.0-rc | - |
| Print Time in Ruler | small | UI/Preview | Show estimated print time when hovering over ruler. | v1.8.0-beta | - | - |
| Custom G-code Overview | medium | UI/Preview | Visual overview of pause commands, filament changes, and custom G-code insertions. | v1.9.0-alpha | - | [#2728](https://github.com/SoftFever/OrcaSlicer/pull/2728) |
| Sidebar Improvements | medium | UI/Layout | Sidebar size remembered across sessions, collapsible, resizable, movable, dockable. | v1.9.0-alpha | v2.0.0-beta | [#2972](https://github.com/SoftFever/OrcaSlicer/pull/2972) |
| Window Size Improvements | small | UI/Layout | Removed window size limitations, fixed maximize behavior. | v1.9.0-alpha | - | [#3148](https://github.com/SoftFever/OrcaSlicer/pull/3148) |
| 3D Navigator | medium | UI/Navigation | 3D navigation viewcube tool for viewport orientation, toggleable from menu. | v2.0.0-beta | v2.1.0 | [#4181](https://github.com/SoftFever/OrcaSlicer/pull/4181) |
| G-code Editor Dialog | medium | UI/Editing | Ported from PrusaSlicer, allows direct G-code editing within the application. | v2.0.0-beta | - | [#3417](https://github.com/SoftFever/OrcaSlicer/pull/3417) |
| Icon Redesign | large | UI/Appearance | Complete icon set redesign emphasizing consistency, modernity, and pixel-perfection. | v2.1.0-beta | - | - |
| Reverse Mouse Zoom | tiny | UI/Input | Option to reverse mouse zoom direction. | v2.1.0-beta | - | - |
| Touchpad-Friendly Navigation | small | UI/Input | FreeCAD-inspired: Shift+move for panning, Alt+move for rotation. | v2.1.0-beta | - | - |
| G-code Preview Rendering Performance | medium | UI/Performance | Improved G-code preview rendering performance for Filament/Speed color schemes. | v2.2.0-beta | - | [#6223](https://github.com/SoftFever/OrcaSlicer/pull/6223) |
| G-code Legend Improvements | medium | UI/Preview | Compact layout design maximizing preview space while maintaining clarity. | v2.3.1-alpha | v2.3.1-beta | [#8198](https://github.com/SoftFever/OrcaSlicer/pull/8198) |

## G-code Preview

### Color Schemes
- Feature type coloring
- Filament coloring
- Speed-based coloring
- Fan speed visualization (v1.3.2)

### Performance ([#6223](https://github.com/SoftFever/OrcaSlicer/pull/6223))
- Improved rendering for Filament/Speed color schemes
- Estimations display consistently (v2.3.1-beta)
- Outline algorithm using depth buffer ([#6492](https://github.com/SoftFever/OrcaSlicer/pull/6492))

## Navigation

### 3D Viewport ([#4181](https://github.com/SoftFever/OrcaSlicer/pull/4181))
- 3D Navigator viewcube
- Touchpad-friendly controls (v2.1.0-beta)
- Space mouse support with customization

### Keyboard Shortcuts
- Tab: Switch Prepare/Preview
- C: Toggle G-code window
- F5: Reload printer view (v1.6.0)

## Input Devices

### Space Mouse
- Full axis control
- Inversion options via CTRL-M dialog
- Keyboard shortcut support (v2.1.0-rc)
- Parameter loading fixed ([#6703](https://github.com/SoftFever/OrcaSlicer/pull/6703))

### Touchpad
- Pan with Shift+move
- Rotate with Alt+move
- Zoom with standard gestures

## Notes

- Legend window size issues on Linux fixed in v2.3.1-beta
- G-code viewer first layer height in vase mode fixed in v1.7.0-beta
- Preview misalignment issues fixed ([#9529](https://github.com/SoftFever/OrcaSlicer/pull/9529))
- Unsaved project titles marked with asterisk ([#5910](https://github.com/SoftFever/OrcaSlicer/pull/5910))
- Drop object on plate feature added ([#6166](https://github.com/SoftFever/OrcaSlicer/pull/6166))
