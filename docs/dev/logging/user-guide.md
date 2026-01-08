# OrcaSlicer Logging - User Guide

## Log File Location

| Platform | Location |
|----------|----------|
| **Windows** | `%APPDATA%\OrcaSlicer\log\` |
| **Linux** | `~/.config/OrcaSlicer/log/` |
| **macOS** | `~/Library/Application Support/OrcaSlicer/log/` |

Log files are named: `debug_<timestamp>_<pid>.log.<N>` (e.g., `debug_Mon_Jan_08_14_30_45_12345.log.0`)

## Log Levels

| Level | Name    | Description |
|-------|---------|-------------|
| 0     | fatal   | Fatal errors only |
| 1     | error   | Errors |
| 2     | warning | Warnings (**default**) |
| 3     | info    | Informational messages |
| 4     | debug   | Detailed debugging |
| 5     | trace   | Fine-grained tracing |

## Changing Log Level

Edit the config file:
- **Windows**: `%APPDATA%\OrcaSlicer\OrcaSlicer.conf`
- **Linux**: `~/.config/OrcaSlicer/OrcaSlicer.conf`
- **macOS**: `~/Library/Application Support/OrcaSlicer/OrcaSlicer.conf`

Find the `"app"` section and set `"log_severity_level"`:
```json
{
  "app": {
    "log_severity_level": "trace",
    ...
  }
}
```

Valid values: `"fatal"`, `"error"`, `"warning"` (default), `"info"`, `"debug"`, `"trace"`

Restart OrcaSlicer after saving.

## Viewing Logs

### Windows

**Open log folder in Explorer:**
```
%APPDATA%\OrcaSlicer\log
```

**View latest log in PowerShell:**
```powershell
Get-Content "$env:APPDATA\OrcaSlicer\log\debug_*.log.*" -Tail 50 -Wait
```

**List recent log files:**
```powershell
dir "$env:APPDATA\OrcaSlicer\log\debug_*" | Sort-Object LastWriteTime -Descending | Select-Object -First 5
```

**Copy latest log to WSL-accessible folder:**
```powershell
$f = gci "$env:APPDATA\OrcaSlicer\log\debug_*.log.*" | sort LastWriteTime -Desc | select -First 1; cp $f.FullName "e:\wsl\orcalogs\"; "/mnt/e/wsl/orcalogs/$($f.Name)"
```

### Linux

```bash
# View logs in real-time
tail -f ~/.config/OrcaSlicer/log/debug_*.log.*

# List recent logs
ls -lt ~/.config/OrcaSlicer/log/debug_* | head -5
```

### macOS

```bash
tail -f ~/Library/Application\ Support/OrcaSlicer/log/debug_*.log.*
```

## Crash Logs (Windows)

Crash logs are in the same folder: `%APPDATA%\OrcaSlicer\log\crash_*.log`

## SVG Debug Output

Algorithm debug visualizations are written to:
- **Windows**: `%APPDATA%\OrcaSlicer\SVG\`
- **Linux**: `~/.config/OrcaSlicer/SVG/`
- **macOS**: `~/Library/Application Support/OrcaSlicer/SVG/`
