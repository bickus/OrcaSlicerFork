# OrcaSlicer Logging - Developer Guide

This document covers how to add and use logging in OrcaSlicer code. For user-facing information (log locations, viewing logs, changing log levels), see [user-guide.md](user-guide.md).

## Overview

OrcaSlicer uses **Boost.Log** with the `BOOST_LOG_TRIVIAL` macro. Log files are written to the user's data directory with automatic rotation at 100MB.

## Log Levels

| Level | Value | Name    | When to Use |
|-------|-------|---------|-------------|
| 0     | fatal | Fatal   | Application cannot continue |
| 1     | error | Error   | Operation failed but app can continue |
| 2     | warning | Warning | Unexpected but recoverable situation |
| 3     | info  | Info    | Significant milestones (start/end of operations) |
| 4     | debug | Debug   | Detailed algorithm state, diagnostics |
| 5     | trace | Trace   | Fine-grained tracing, loop iterations |

## Adding Logging in Code

### Basic Usage

```cpp
#include <boost/log/trivial.hpp>

BOOST_LOG_TRIVIAL(info) << "Exporting G-code to " << path;
BOOST_LOG_TRIVIAL(debug) << "Processing layer " << layer_id;
BOOST_LOG_TRIVIAL(warning) << "Unusual parameter value: " << value;
BOOST_LOG_TRIVIAL(error) << "Failed to open file: " << filename;
BOOST_LOG_TRIVIAL(trace) << "Detailed trace: " << debug_info;
```

### With Boost Format

```cpp
#include <boost/format.hpp>

BOOST_LOG_TRIVIAL(info) << boost::format("Processing %1% objects") % count;
BOOST_LOG_TRIVIAL(debug) << boost::format("Layer %1%: height=%2%, z=%3%") % id % height % z;
```

### Memory Logging

```cpp
#include "libslic3r/Utils.hpp"

BOOST_LOG_TRIVIAL(info) << "Slicing started" << log_memory_info();
// Output: [info] Slicing started WorkingSet: 1,234MB; PrivateBytes: 987MB; ...
```

### Manual Trace Function

```cpp
#include "libslic3r/Utils.hpp"

Slic3r::trace(3, "This is an info-level message");  // Level 3 = info
```

## Mandatory Logging for New Features

When developing new features, **you must include debug-level logging** with a unique prefix. This is essential for diagnosing issues in the field.

Requirements:
- Add `BOOST_LOG_TRIVIAL(debug)` statements at key points in new code
- Use a unique, grep-friendly prefix (e.g., `"FEATURE_NAME:"`)
- Log entry/exit of significant functions, important state changes, and decision points

### Using Unique Prefixes

Use a unique prefix to easily filter large log files:

```cpp
BOOST_LOG_TRIVIAL(debug) << "MYDBG: entering bridge detection, layer=" << layer_id;
BOOST_LOG_TRIVIAL(debug) << "MYDBG: bridge angle=" << angle << ", width=" << width;
```

Then filter:
```bash
grep "MYDBG:" debug_*.log.*
```

### Example Patterns

```cpp
// Good: Milestone logging
BOOST_LOG_TRIVIAL(info) << "Exporting G-code to " << path;

// Good: Operation result
BOOST_LOG_TRIVIAL(debug) << "Sliced " << layer_count << " layers in " << elapsed << "ms";

// Good: Error with context
BOOST_LOG_TRIVIAL(error) << "Failed to load file '" << filename << "': " << error_message;

// Good: Memory checkpoint
BOOST_LOG_TRIVIAL(info) << "Starting complex operation" << log_memory_info();

// Good: Temporary debug with unique prefix
BOOST_LOG_TRIVIAL(debug) << "XYZDBG: variable state x=" << x << " y=" << y;

// Avoid in hot loops unless trace level
for (const auto& layer : layers) {
    BOOST_LOG_TRIVIAL(trace) << "Processing layer " << layer.id;  // OK for trace
}
```

## Debug SVG Output

For algorithm debugging, output SVG visualizations:

```cpp
#include "libslic3r/SVG.hpp"
#include "libslic3r/Utils.hpp"

// Files written to <data_dir>/SVG/
SVG svg(debug_out_path("my_debug_%d.svg", iteration).c_str(), bounding_box);
svg.draw(polygon, "red", 0.5);
svg.draw_outline(expolygon, "blue");
```

## Programmatic Log Control

```cpp
#include "libslic3r/Utils.hpp"

// Set by numeric level
Slic3r::set_logging_level(4);  // debug

// Convert string to level
unsigned level = Slic3r::level_string_to_boost("trace");
Slic3r::set_logging_level(level);

// Get current level
unsigned current = Slic3r::get_logging_level();
std::string level_name = Slic3r::get_string_logging_level(current);

// Flush pending log records
Slic3r::flush_logs();
```

## Key Source Files

| File | Purpose |
|------|---------|
| `src/libslic3r/utils.cpp:93-180` | Log level management, trace function |
| `src/libslic3r/utils.cpp:330-373` | Log file initialization, flushing |
| `src/libslic3r/Utils.hpp:77-87` | Public logging API declarations |
| `src/slic3r/GUI/GUI_App.cpp:2025-2070` | GUI app logging initialization |
| `src/slic3r/GUI/GUI_App.cpp:2233-2253` | wxWidgets to boost::log bridge |
| `src/OrcaSlicer.cpp:1176-1184` | CLI mode `--debug` handling |
| `src/slic3r/GUI/Preferences.cpp:1431-1561` | Debug page (non-release only) |

## Technical Details

### Boost.Log Components Used

- `boost/log/trivial.hpp` - Trivial severity logger (BOOST_LOG_TRIVIAL macro)
- `boost/log/core.hpp` - Core framework
- `boost/log/expressions.hpp` - Filter and formatter expressions
- `boost/log/sinks/text_file_backend.hpp` - File output sink
- `boost/log/utility/setup/file.hpp` - File sink setup
- `boost/log/utility/setup/common_attributes.hpp` - ThreadID, timestamp

### Initialization Sequence (GUI Mode)

1. Static initializer sets default log level to 2 (warning) when libslic3r loads
2. `GUI_App::init_app_config()` creates log filename with timestamp
3. `set_log_path_and_level()` creates log directory and configures boost::log file sink
   - Sets level to 5 (trace) for non-release builds, 3 (info) for release
4. App config is loaded, **overwriting** log level from `log_severity_level` setting (default: "warning" = 2)
5. `wxBoostLog` is set as wxWidgets active log target

**Note**: `--debug` CLI parameter is ignored in GUI mode - log level is set during GUI initialization.

### Initialization Sequence (CLI Mode)

1. Command-line arguments are parsed
2. If CLI actions are detected, `--debug` parameter is read and `set_logging_level()` is called
3. Log file path is set up during operation

### Thread Safety

- Boost.Log is thread-safe by default
- The global log sink uses `synchronous_sink` for thread-safe file writing
- Memory logging functions are safe to call from any thread

### Conditional Compilation

The Debug preferences page is controlled by `BBL_RELEASE_TO_PUBLIC`:
```cpp
#if !BBL_RELEASE_TO_PUBLIC
    auto debug_page = create_debug_page();
#endif
```

### Building Console Version (Windows)

For stdout/stderr output on Windows, build without `SLIC3R_WRAPPER_NOCONSOLE`:
- See `src/CMakeLists.txt:176` and `src/OrcaSlicer_app_msvc.cpp:211-219`
