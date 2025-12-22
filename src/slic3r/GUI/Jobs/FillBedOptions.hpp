#ifndef SLIC3R_GUI_JOBS_FILLBEDOPTIONS_HPP
#define SLIC3R_GUI_JOBS_FILLBEDOPTIONS_HPP

namespace Slic3r {
namespace GUI {

enum class FillBedMode {
    Standard,
    Tight
};

struct FillBedOptions {
    FillBedMode mode             = FillBedMode::Standard;
    double      min_distance_mm  = 0.0;
    bool        allow_rotation   = false;
    bool        enable_multi_strategy = false;
};

} // namespace GUI
} // namespace Slic3r

#endif // SLIC3R_GUI_JOBS_FILLBEDOPTIONS_HPP
