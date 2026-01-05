# Printer Profiles Features

Profile management, OTA updates, and printer support.

## Features

| Feature | Scale | Category | Description | Added | Modified | PRs |
|---------|-------|----------|-------------|-------|----------|-----|
| Third-Party Printer Profiles | large | Profiles/Printers | Extensive library of third-party printer profiles (Voron, Prusa, Creality, etc.). | v1.1 | ongoing | - |
| Machine Limits Customization | medium | Profiles/Limits | Customize machine speed, acceleration, and jerk limits. | v1.2 | - | - |
| Profile Cloud Syncing | medium | Profiles/Sync | Cloud syncing for profiles (fixed for third-party printers in v1.4.1). | v1.0 | v1.4.1 | - |
| Datadir Parameter | small | Profiles/Storage | Custom data folder location for profiles and settings. | v1.5.0 | v1.9.0-beta | - |
| Printer-Specific Settings Memory | medium | Profiles/Memory | Filament, bed, and process settings remembered per printer. | v1.6.4-beta | - | - |
| Stealth Mode | medium | Profiles/Privacy | Disable BBL HMS connections for privacy-conscious users. | v1.6.4 | - | - |
| OTA Profile Updates | large | Profiles/Updates | Over-the-air profile updates for all printer types, not just specific models. | v2.0.0-beta | - | [#4069](https://github.com/SoftFever/OrcaSlicer/pull/4069) |
| User Profile Backup | medium | Profiles/Backup | Automatic backup of user profiles during version upgrades. | v2.3.0 | - | - |

## Profile Types

### Printer Profiles
Define printer hardware capabilities:
- Bed size and shape
- Nozzle configurations
- Machine limits
- Start/end G-code

### Process Profiles
Print quality settings:
- Layer heights
- Speeds and accelerations
- Wall and infill settings
- Support configurations

### Filament Profiles
Material-specific settings:
- Temperatures
- Flow rates
- Cooling settings
- Retraction parameters

## Supported Printer Manufacturers

Major manufacturers with profiles:
- **Bambu Lab**: X1, X1C, P1P, P1S, A1
- **Prusa**: MK3S, MK4, Mini, XL
- **Creality**: Ender 3 series, K1 series, CR-10 series
- **Voron**: 0.1, 2.4, Trident
- **Anycubic**: Kobra series
- **QIDI**: X-series, Plus4
- **Artillery**: Sidewinder, Genius
- **Elegoo**: Neptune series, OrangeStorm
- And many more...

## Profile Management

### Per-Printer Memory (v1.6.4-beta)
Settings remembered separately for each printer:
- Last used filament
- Bed type
- Process settings

### OTA Updates ([#4069](https://github.com/SoftFever/OrcaSlicer/pull/4069))
- Automatic profile updates
- Works for all printer types
- Optional update notifications

### Backup System (v2.3.0)
- Automatic backup on upgrade
- Protects custom settings
- Easy restoration

## Notes

- Printer config bundle export issues fixed in v2.3.1-beta
- Last used printer memory after restart fixed in v1.4.0
- Corrupted config file auto-recovery added in v1.8.0-rc
- Profile validation tools enhanced in v2.0.0-beta
- Async profile loading for faster startup ([#9118](https://github.com/SoftFever/OrcaSlicer/pull/9118))
