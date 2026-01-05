# Connectivity Features

Network printing, cloud integrations, and remote monitoring.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Print Host Setup | medium | Connectivity/Network | Direct printer communication via print host configuration. | v1.1 | - | - |
| Third-Party Printer Support | large | Connectivity/Printers | Support for Voron, Prusa MK3S, and many other third-party printers. | v1.1 | - | - |
| Moonraker Metadata Enhancement | small | Connectivity/Klipper | Enhanced metadata in G-code for Moonraker integration. | v1.3.1 | - | - |
| RRF Firmware Support | medium | Connectivity/Firmware | Experimental RepRapFirmware support for direct communication. | v1.3.4 | - | - |
| Custom URL for Printer WebUI | small | Connectivity/UI | Custom URL support for printer web interfaces including SonicPad. | v1.5.0 | - | - |
| Stealth Mode | medium | Connectivity/Privacy | Disable BBL HMS connections for privacy-conscious users. | v1.6.4 | - | - |
| PrusaLink Integration | medium | Connectivity/Prusa | PrusaLink and Prusaconnect webview support for Prusa printers. | v1.7.0-beta | - | - |
| Custom IP Camera Support | medium | Connectivity/Monitoring | Configure custom IP cameras for remote print monitoring. | v2.0.0-beta | - | - |
| Obico Cloud Integration | large | Connectivity/Cloud | Full support for Obico cloud monitoring and management. | v2.0.0-beta | - | [#4116](https://github.com/SoftFever/OrcaSlicer/pull/4116) |
| ESP3D Wireless Connection | medium | Connectivity/Wireless | Support for ESP3D-equipped printers for wireless G-code transmission. | v2.1.0-beta | - | - |
| SimplyPrint Integration | medium | Connectivity/Cloud | Integration with SimplyPrint cloud printing service. | v2.1.0 | - | - |

## Network Printing

### Direct Connection
- Print host setup for OctoPrint, Moonraker, Duet, etc.
- Custom URL support for various interfaces
- Hostname-based connections (fixed regression in v1.8.0-beta)

### Klipper Integration
- Moonraker metadata for proper job tracking
- Combined SET_VELOCITY_LIMIT commands (v1.8.0-beta)
- Exclude Objects support (v1.6.0)
- Print time accuracy improvements (v1.3.2)

### RepRapFirmware
- Experimental support since v1.3.4
- Motion ability page display (fixed v1.8.0-beta)

## Cloud Services

### Obico ([#4116](https://github.com/SoftFever/OrcaSlicer/pull/4116))
- Remote monitoring
- AI failure detection
- Print management

### SimplyPrint (v2.1.0)
- Cloud printing service
- Multi-printer management
- Remote control

### BambuLab Cloud
- Native integration for BBL printers
- Stealth mode for privacy (v1.6.4)
- Camera registration and live view

## Prusa Integration

### PrusaLink/Prusaconnect (v1.7.0-beta)
- Webview integration
- Direct file upload
- Print monitoring

### Large File Upload
- Fixed for large G-code files (v2.1.1)

## Notes

- Third-party printer BL network plugin bypass added in v1.3.3
- WebView2 runtime check on startup with installation prompts (v1.7.0-beta)
- WebView developer tools enabled in developer mode (v2.2.0-beta)
- macOS WebView text dragging crash fixed ([#6668](https://github.com/SoftFever/OrcaSlicer/pull/6668))
- Stealth mode setup wizard added ([#6104](https://github.com/SoftFever/OrcaSlicer/pull/6104))
