#ifndef FILLBEDJOB_HPP
#define FILLBEDJOB_HPP

#include "ArrangeJob.hpp"
#include "FillBedOptions.hpp"

namespace Slic3r { namespace GUI {

class Plater;

class FillBedJob : public Job
{
    int     m_object_idx = -1;

    using ArrangePolygon  = arrangement::ArrangePolygon;
    using ArrangePolygons = arrangement::ArrangePolygons;

    ArrangePolygons m_selected;
    ArrangePolygons m_unselected;
    //BBS: add partplate related logic
    ArrangePolygons m_locked;;

    Points m_bedpts;

    arrangement::ArrangeParams params;

    int m_status_range = 0;
    Plater *m_plater;
    FillBedOptions m_options;

public:

    void prepare();
    void process(Ctl &ctl) override;

    explicit FillBedJob(const FillBedOptions &options = FillBedOptions{});

    int status_range() const
    {
        return m_status_range;
    }

    void finalize(bool canceled, std::exception_ptr &e) override;

    bool is_tight_mode() const { return m_options.mode == FillBedMode::Tight; }
};

}} // namespace Slic3r::GUI

#endif // FILLBEDJOB_HPP
