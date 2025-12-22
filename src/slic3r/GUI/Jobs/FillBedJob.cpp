#include "FillBedJob.hpp"

#include "libslic3r/Model.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/ModelArrange.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/GUI_ObjectList.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "libnest2d/common.hpp"

#include <numeric>
#include <limits>
#include <array>
#include <random>
#include <chrono>
#include <boost/format.hpp>

namespace Slic3r {
namespace GUI {

namespace {

class StrategyProgressDialog : public DPIDialog
{
public:
    StrategyProgressDialog(wxWindow *parent, const std::vector<wxString> &titles)
        : DPIDialog(parent, wxID_ANY, _L("Tight fill progress"), wxDefaultPosition, wxDefaultSize,
                    wxCAPTION | wxCLOSE_BOX | wxSTAY_ON_TOP)
    {
        auto main_sizer = new wxBoxSizer(wxVERTICAL);
        auto grid = new wxFlexGridSizer(3, static_cast<int>(titles.size()) + 1, FromDIP(6), FromDIP(12));
        grid->AddGrowableCol(2, 1);

        auto add_header = [&](const wxString &text) {
            auto lbl = new wxStaticText(this, wxID_ANY, text);
            lbl->SetFont(Label::Body_14);
            grid->Add(lbl, 0, wxALIGN_LEFT | wxRIGHT, FromDIP(4));
        };

        add_header(_L("Strategy"));
        add_header(_L("Status"));
        add_header(_L("Result"));

        for (const auto &title : titles) {
            auto title_lbl = new wxStaticText(this, wxID_ANY, title);
            title_lbl->SetFont(Label::Body_14);
            grid->Add(title_lbl, 0, wxALIGN_LEFT);

            auto status_lbl = new wxStaticText(this, wxID_ANY, _L("Queued"));
            grid->Add(status_lbl, 0, wxALIGN_LEFT);

            auto result_lbl = new wxStaticText(this, wxID_ANY, wxEmptyString);
            grid->Add(result_lbl, 0, wxALIGN_LEFT);

            m_status_labels.push_back(status_lbl);
            m_result_labels.push_back(result_lbl);
        }

        main_sizer->Add(grid, 0, wxALL, FromDIP(12));
        SetSizerAndFit(main_sizer);
        CentreOnParent();
        wxGetApp().UpdateDlgDarkUI(this);
    }

    void update(size_t idx, const wxString &status, const wxString &result)
    {
        if (idx >= m_status_labels.size())
            return;
        m_status_labels[idx]->SetLabel(status);
        if (!result.empty())
            m_result_labels[idx]->SetLabel(result);
        Layout();
    }

private:
    std::vector<wxStaticText*> m_status_labels;
    std::vector<wxStaticText*> m_result_labels;

    void on_dpi_changed(const wxRect &) override {}
};

struct StrategyMetrics {
    size_t clones_on_bed      = 0;
    size_t existing_off_bed   = 0;
    size_t total_on_bed       = 0;
    double footprint_area_mm2 = std::numeric_limits<double>::max();
};

inline StrategyMetrics collect_metrics(const arrangement::ArrangePolygons &items)
{
    StrategyMetrics metrics;
    BoundingBox footprint;
    bool has_footprint = false;
    for (const auto &ap : items) {
        if (ap.priority == 0 && ap.bed_idx == 0)
            ++metrics.clones_on_bed;
        if (ap.priority > 0 && ap.bed_idx != 0)
            ++metrics.existing_off_bed;
        if (ap.bed_idx == 0)
            ++metrics.total_on_bed;

        if (ap.bed_idx == 0) {
            BoundingBox ap_bb = ap.transformed_poly().contour.bounding_box();
            if (!has_footprint) {
                footprint = ap_bb;
                has_footprint = true;
            } else {
                footprint.merge(ap_bb);
            }
        }
    }

    if (has_footprint) {
        metrics.footprint_area_mm2 = unscale<double>(footprint.size().x()) *
                                     unscale<double>(footprint.size().y());
    }

    return metrics;
}

inline bool is_better(const StrategyMetrics &lhs, const StrategyMetrics &rhs)
{
    if (lhs.existing_off_bed != rhs.existing_off_bed)
        return lhs.existing_off_bed < rhs.existing_off_bed;
    if (lhs.clones_on_bed != rhs.clones_on_bed)
        return lhs.clones_on_bed > rhs.clones_on_bed;
    if (lhs.total_on_bed != rhs.total_on_bed)
        return lhs.total_on_bed > rhs.total_on_bed;
    return lhs.footprint_area_mm2 < rhs.footprint_area_mm2;
}

enum class SeedMode {
    None,
    AreaDesc,
    AreaAsc,
    AspectRatio,
    HeightDesc,
    Randomized
};

inline double polygon_area_mm2(const arrangement::ArrangePolygon &ap)
{
    static const double scale2 = scaled<double>(1.) * scaled(1.);
    return std::abs(ap.poly.area()) / scale2;
}

inline double polygon_aspect_ratio(const arrangement::ArrangePolygon &ap)
{
    BoundingBox bb = ap.poly.contour.bounding_box();
    double w = std::max(unscale<double>(bb.size().x()), 0.001);
    double h = std::max(unscale<double>(bb.size().y()), 0.001);
    return std::max(w, h) / std::min(w, h);
}

inline void apply_seed_mode(arrangement::ArrangePolygons &items, SeedMode mode, std::mt19937 &rng)
{
    switch (mode) {
    case SeedMode::AreaDesc:
        std::stable_sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
            return polygon_area_mm2(a) > polygon_area_mm2(b);
        });
        break;
    case SeedMode::AreaAsc:
        std::stable_sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
            return polygon_area_mm2(a) < polygon_area_mm2(b);
        });
        break;
    case SeedMode::AspectRatio:
        std::stable_sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
            return polygon_aspect_ratio(a) > polygon_aspect_ratio(b);
        });
        break;
    case SeedMode::HeightDesc:
        std::stable_sort(items.begin(), items.end(), [](const auto &a, const auto &b) {
            return a.height > b.height;
        });
        break;
    case SeedMode::Randomized:
        std::shuffle(items.begin(), items.end(), rng);
        break;
    case SeedMode::None:
    default:
        break;
    }
}

inline Polygon make_bed_polygon(const Points &bedpts)
{
    Polygon bed(bedpts);
    return bed;
}

inline ExPolygon transformed_polygon_with_clearance(const arrangement::ArrangePolygon &ap, coord_t clearance)
{
    ExPolygon poly = ap.poly;
    if (ap.rotation != 0.0)
        poly.rotate(ap.rotation);
    if (ap.translation.x() != 0 || ap.translation.y() != 0)
        poly.translate(ap.translation.x(), ap.translation.y());
    coord_t inflation = std::max(ap.inflation, clearance);
    if (inflation > 0) {
        auto polys = offset_ex(poly, inflation);
        if (!polys.empty())
            poly = polys.front();
    }
    return poly;
}

inline bool polygon_inside_bed(const ExPolygon &poly, const Polygon &bed_polygon)
{
    for (const auto &pt : poly.contour.points)
        if (!bed_polygon.contains(pt))
            return false;
    for (const auto &hole : poly.holes)
        for (const auto &pt : hole.points)
            if (!bed_polygon.contains(pt))
                return false;
    return true;
}

inline bool intersects_existing(const ExPolygon &candidate, const std::vector<ExPolygon> &existing)
{
    for (const auto &other : existing)
        if (!intersection(ExPolygons { candidate }, ExPolygons { other }).empty())
            return true;
    return false;
}

size_t bottom_left_fill(arrangement::ArrangePolygons &selected,
                        const arrangement::ArrangePolygons &obstacles,
                        const arrangement::ArrangeParams &params,
                        const Points &bedpts,
                        coord_t clearance,
                        bool allow_rotation)
{
    Polygon bed_polygon = make_bed_polygon(bedpts);
    BoundingBox bed_bb = bed_polygon.bounding_box();
    coord_t step = std::max<coord_t>(scaled<coord_t>(0.5), clearance > 0 ? clearance / 2 : scaled<coord_t>(0.25));

    std::vector<ExPolygon> occupied;
    auto add_occupied = [&](const arrangement::ArrangePolygons &items) {
        for (const auto &ap : items)
            if (ap.bed_idx == 0)
                occupied.emplace_back(transformed_polygon_with_clearance(ap, clearance));
    };
    add_occupied(selected);
    add_occupied(obstacles);

    size_t rescued = 0;
    std::vector<double> rotation_candidates = { 0.0 };
    if (allow_rotation) {
        rotation_candidates.push_back(PI / 2.0);
        rotation_candidates.push_back(PI);
        rotation_candidates.push_back(3.0 * PI / 2.0);
    }

    for (auto &ap : selected) {
        if (ap.priority != 0 || ap.bed_idx == 0)
            continue;

        bool placed = false;
        for (double rot : rotation_candidates) {
            arrangement::ArrangePolygon candidate = ap;
            candidate.rotation = rot;
            ExPolygon rotated = candidate.poly;
            rotated.rotate(rot);
            BoundingBox bb = rotated.contour.bounding_box();
            coord_t w = bb.size().x();
            coord_t h = bb.size().y();

            for (coord_t y = bed_bb.min.y(); y <= bed_bb.max.y() - h && !placed; y += step) {
                for (coord_t x = bed_bb.min.x(); x <= bed_bb.max.x() - w; x += step) {
                    candidate.translation(X) = x;
                    candidate.translation(Y) = y;

                    ExPolygon candidate_poly = rotated;
                    candidate_poly.translate(x, y);
                    if (!polygon_inside_bed(candidate_poly, bed_polygon))
                        continue;
                    if (intersects_existing(candidate_poly, occupied))
                        continue;

                    ap.translation = candidate.translation;
                    ap.rotation = candidate.rotation;
                    ap.bed_idx = 0;
                    occupied.emplace_back(candidate_poly);
                    ++rescued;
                    placed = true;
                    break;
                }
            }
            if (placed) break;
        }
    }

    return rescued;
}

void compaction_pass(arrangement::ArrangePolygons &selected,
                     const arrangement::ArrangeParams &params,
                     const Points &bedpts,
                     coord_t clearance)
{
    Polygon bed_polygon = make_bed_polygon(bedpts);
    coord_t step = std::max<coord_t>(scaled<coord_t>(0.25), clearance > 0 ? clearance / 2 : scaled<coord_t>(0.1));
    bool moved = true;
    size_t guard = 0;

    auto can_place = [&](const ArrangePolygon &candidate, size_t idx) {
        ExPolygon candidate_poly = transformed_polygon_with_clearance(candidate, clearance);
        if (!polygon_inside_bed(candidate_poly, bed_polygon))
            return false;
        for (size_t i = 0; i < selected.size(); ++i) {
            if (i == idx || selected[i].bed_idx != 0)
                continue;
            ExPolygon other_poly = transformed_polygon_with_clearance(selected[i], clearance);
            if (!intersection(ExPolygons { candidate_poly }, ExPolygons { other_poly }).empty())
                return false;
        }
        return true;
    };

    while (moved && guard++ < 64) {
        moved = false;
        for (size_t idx = 0; idx < selected.size(); ++idx) {
            auto &ap = selected[idx];
            if (ap.priority != 0 || ap.bed_idx != 0)
                continue;

            constexpr std::array<std::pair<coord_t, coord_t>, 4> directions = {{
                { -1, 0 }, { 0, -1 }, { -1, -1 }, { -1, 1 }
            }};

            for (auto [dx_unit, dy_unit] : directions) {
                coord_t dx = dx_unit == 0 ? 0 : (dx_unit > 0 ? step : -step);
                coord_t dy = dy_unit == 0 ? 0 : (dy_unit > 0 ? step : -step);
                if (dx == 0 && dy == 0)
                    continue;

                ArrangePolygon candidate = ap;
                bool local_move = false;
                while (true) {
                    candidate.translation(X) += dx;
                    candidate.translation(Y) += dy;
                    if (!can_place(candidate, idx))
                        break;
                    ap.translation = candidate.translation;
                    local_move = true;
                    moved = true;
                }
                if (local_move)
                    break;
            }
        }
    }
}

} // namespace

//BBS: add partplate related logic
void FillBedJob::prepare()
{
    PartPlateList& plate_list = m_plater->get_partplate_list();

    m_locked.clear();
    m_selected.clear();
    m_unselected.clear();
    m_bedpts.clear();

    params = init_arrange_params(m_plater);

    m_object_idx = m_plater->get_selected_object_idx();
    if (m_object_idx == -1)
        return;

    //select current plate at first
    int sel_id = m_plater->get_selection().get_instance_idx();
    sel_id = std::max(sel_id, 0);

    int sel_ret = plate_list.select_plate_by_obj(m_object_idx, sel_id);
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":select plate obj_id %1%, ins_id %2%, ret %3%}") % m_object_idx % sel_id % sel_ret;

    PartPlate* plate = plate_list.get_curr_plate();
    Model& model = m_plater->model();
    BoundingBox plate_bb = plate->get_bounding_box_crd();
    int plate_cols = plate_list.get_plate_cols();
    int cur_plate_index = plate->get_index();

    ModelObject *model_object = m_plater->model().objects[m_object_idx];
    if (model_object->instances.empty()) return;

    const Slic3r::DynamicPrintConfig& global_config = wxGetApp().preset_bundle->full_config();
    m_selected.reserve(model_object->instances.size());
    for (size_t oidx = 0; oidx < model.objects.size(); ++oidx)
    {
        ModelObject* mo = model.objects[oidx];
        for (size_t inst_idx = 0; inst_idx < mo->instances.size(); ++inst_idx)
        {
            bool selected = (oidx == m_object_idx);

            ArrangePolygon ap = get_instance_arrange_poly(mo->instances[inst_idx], global_config);
            BoundingBox ap_bb = ap.transformed_poly().contour.bounding_box();
            ap.name = mo->name;

            if (selected)
            {
                if (mo->instances[inst_idx]->printable)
                {
                    ++ap.priority;
                    ap.itemid = m_selected.size();
                    m_selected.emplace_back(ap);
                }
                else
                {
                    if (plate_bb.contains(ap_bb))
                    {
                        ap.bed_idx = 0;
                        ap.itemid = m_unselected.size();
                        ap.row = cur_plate_index / plate_cols;
                        ap.col = cur_plate_index % plate_cols;
                        ap.translation(X) -= bed_stride_x(m_plater) * ap.col;
                        ap.translation(Y) += bed_stride_y(m_plater) * ap.row;
                        m_unselected.emplace_back(ap);
                    }
                    else
                    {
                        ap.bed_idx = PartPlateList::MAX_PLATES_COUNT;
                        ap.itemid = m_locked.size();
                        m_locked.emplace_back(ap);
                    }
                }
            }
            else
            {
                if (plate_bb.contains(ap_bb))
                {
                    ap.bed_idx = 0;
                    ap.itemid = m_unselected.size();
                    ap.row = cur_plate_index / plate_cols;
                    ap.col = cur_plate_index % plate_cols;
                    ap.translation(X) -= bed_stride_x(m_plater) * ap.col;
                    ap.translation(Y) += bed_stride_y(m_plater) * ap.row;
                    m_unselected.emplace_back(ap);
                }
                else
                {
                    ap.bed_idx = PartPlateList::MAX_PLATES_COUNT;
                    ap.itemid = m_locked.size();
                    m_locked.emplace_back(ap);
                }
            }
        }
    }
    /*
    for (ModelInstance *inst : model_object->instances)
        if (inst->printable) {
            ArrangePolygon ap = get_arrange_poly(inst);
            // Existing objects need to be included in the result. Only
            // the needed amount of object will be added, no more.
            ++ap.priority;
            m_selected.emplace_back(ap);
        }*/

    if (m_selected.empty()) return;

    //add the virtual object into unselect list if has
    double scaled_exclusion_gap = scale_(1);
    plate_list.preprocess_exclude_areas(params.excluded_regions, 1, scaled_exclusion_gap);
    plate_list.preprocess_exclude_areas(m_unselected);

    m_bedpts = get_bed_shape(*m_plater->config());

    auto &objects = m_plater->model().objects;
    /*BoundingBox bedbb = get_extents(m_bedpts);

    for (size_t idx = 0; idx < objects.size(); ++idx)
        if (int(idx) != m_object_idx)
            for (ModelInstance *mi : objects[idx]->instances) {
                ArrangePolygon ap = get_arrange_poly(mi);
                auto ap_bb = ap.transformed_poly().contour.bounding_box();

                if (ap.bed_idx == 0 && !bedbb.contains(ap_bb))
                    ap.bed_idx = arrangement::UNARRANGED;

                m_unselected.emplace_back(ap);
            }*/
    if (auto wt = get_wipe_tower_arrangepoly(*m_plater))
        m_unselected.emplace_back(std::move(*wt));

    double sc = scaled<double>(1.) * scaled(1.);

    auto polys = offset_ex(m_selected.front().poly, params.min_obj_distance / 2);
    ExPolygon poly = polys.empty() ? m_selected.front().poly : polys.front();
    double poly_area = poly.area() / sc;
    double unsel_area = std::accumulate(m_unselected.begin(),
                                        m_unselected.end(), 0.,
                                        [cur_plate_index](double s, const auto &ap) {
                                            //BBS: m_unselected instance is in the same partplate
                                            return s + (ap.bed_idx == cur_plate_index) * ap.poly.area();
                                            //return s + (ap.bed_idx == 0) * ap.poly.area();
                                        }) / sc;

    double fixed_area = unsel_area + m_selected.size() * poly_area;
    double bed_area   = Polygon{m_bedpts}.area() / sc;

    // This is the maximum number of items, the real number will always be close but less.
    int needed_items = (bed_area - fixed_area) / poly_area;

    //int sel_id = m_plater->get_selection().get_instance_idx();
    // if the selection is not a single instance, choose the first as template
    //sel_id = std::max(sel_id, 0);
    ModelInstance *mi = model_object->instances[sel_id];
    ArrangePolygon template_ap = get_instance_arrange_poly(mi, global_config);

    for (int i = 0; i < needed_items; ++i) {
        ArrangePolygon ap = template_ap;
        ap.poly = m_selected.front().poly;
        ap.bed_idx = PartPlateList::MAX_PLATES_COUNT;
        ap.itemid = -1;
        ap.setter = [this, mi](const ArrangePolygon &p) {
            ModelObject *mo = m_plater->model().objects[m_object_idx];
            ModelObject* newObj = m_plater->model().add_object(*mo);
            newObj->name = mo->name +" "+ std::to_string(p.itemid);
            for (ModelInstance *newInst : newObj->instances) { newInst->apply_arrange_result(p.translation.cast<double>(), p.rotation); }
            //m_plater->sidebar().obj_list()->paste_objects_into_list({m_plater->model().objects.size()-1});
        };
        m_selected.emplace_back(ap);
    }

    m_status_range = m_selected.size();

    // The strides have to be removed from the fixed items. For the
    // arrangeable (selected) items bed_idx is ignored and the
    // translation is irrelevant.
    //BBS: remove logic for unselected object
    /*double stride = bed_stride(m_plater);
    for (auto &p : m_unselected)
        if (p.bed_idx > 0)
            p.translation(X) -= p.bed_idx * stride;*/
}

void FillBedJob::process(Ctl &ctl)
{
    auto statustxt = is_tight_mode() ? _u8L("Filling tightly") : _u8L("Filling");
    ctl.call_on_main_thread([this] { prepare(); }).wait();
    ctl.update_status(0, statustxt);

    if (m_object_idx == -1 || m_selected.empty()) return;

    auto &partplate_list               = m_plater->get_partplate_list();
    const Slic3r::DynamicPrintConfig& global_config = wxGetApp().preset_bundle->full_config();
    const bool is_bbl = wxGetApp().preset_bundle->is_bbl_vendor();

    auto run_arrangement = [&](ArrangePolygons &selected,
                               ArrangePolygons &unselected,
                               arrangement::ArrangeParams &local_params,
                               Points &bedpts,
                               const std::string &progress_label)
    {
        update_arrange_params(local_params, m_plater->config(), selected);
        bedpts = get_shrink_bedpts(m_plater->config(), local_params);

        if (is_bbl && local_params.avoid_extrusion_cali_region && global_config.opt_bool("scan_first_layer"))
            partplate_list.preprocess_nonprefered_areas(unselected, MAX_NUM_PLATES);

        update_selected_items_inflation(selected, m_plater->config(), local_params);
        update_unselected_items_inflation(unselected, m_plater->config(), local_params);

        bool do_stop = false;
        local_params.stopcondition = [&ctl, &do_stop]() {
            return ctl.was_canceled() || do_stop;
        };

        local_params.progressind = [this, &ctl, &statustxt, progress_label](unsigned st, std::string str = std::string{})
        {
            if (st == 0)
                return;
            std::string message = statustxt;
            if (!progress_label.empty())
                message += " " + progress_label;
            if (!str.empty())
                message += " " + str;
            ctl.update_status(st * 100 / status_range(), message);
        };

        local_params.on_packed = [&do_stop](const ArrangePolygon &ap) {
            do_stop = ap.bed_idx > 0 && ap.priority == 0;
        };

        if (selected.size() > 100) {
            Vec2f step = unscaled<float>(get_extents(selected.front().poly).size()) +
                         Vec2f(selected.front().brim_width, selected.front().brim_width);
            std::vector<Vec2f> empty_cells = Plater::get_empty_cells(step);
            size_t n = std::min(selected.size(), empty_cells.size());
            for (size_t i = 0; i < n; ++i) {
                selected[i].translation = scaled<coord_t>(empty_cells[i]);
                selected[i].bed_idx = 0;
            }
            for (size_t i = n; i < selected.size(); ++i)
                selected[i].bed_idx = arrangement::UNARRANGED;
        } else {
            arrangement::arrange(selected, unselected, bedpts, local_params);
        }
    };

    if (!is_tight_mode()) {
        params.do_final_align = !is_bbl;
        run_arrangement(m_selected, m_unselected, params, m_bedpts, {});
    } else {
        struct StrategyConfig {
            wxString title;
            double accuracy;
            double distance_multiplier;
            bool allow_rotations;
            bool final_align;
            bool align_to_y;
            SeedMode seed_mode;
            bool enable_bottom_left;
            bool enable_compaction;
        };

        std::vector<StrategyConfig> strategies{
            { _u8L("Baseline search"),          0.95, 1.0, false, true,  false, SeedMode::AreaDesc,   false, true  },
            { _u8L("Rotational permutations"),  1.00, 1.0, true,  false, false, SeedMode::AspectRatio, false, true },
            { _u8L("Edge-aligned sweep"),       0.95, 1.05, false, true,  true,  SeedMode::HeightDesc, false, true },
            { _u8L("Bottom-left refinement"),   1.00, 1.0, false, true,  false, SeedMode::AreaAsc,    true,  true },
            { _u8L("Stochastic anneal pass"),   1.00, 1.0, true,  false, false, SeedMode::Randomized, false, true }
        };

        if (!m_options.enable_multi_strategy && !strategies.empty())
            strategies.resize(1);

        ArrangePolygons base_selected = m_selected;
        ArrangePolygons base_unselected = m_unselected;
        arrangement::ArrangeParams base_params = params;

        struct StrategyResult {
            arrangement::ArrangeParams params;
            ArrangePolygons selected;
            Points bedpts;
            StrategyMetrics metrics;
            size_t rescued = 0;
        };

        StrategyResult best_result;
        bool has_best = false;

        std::vector<wxString> strategy_titles;
        strategy_titles.reserve(strategies.size());
        for (const auto &s : strategies)
            strategy_titles.push_back(s.title);

        StrategyProgressDialog *progress_dialog = nullptr;
        if (is_tight_mode() && strategies.size() > 1) {
            ctl.call_on_main_thread([&] {
                progress_dialog = new StrategyProgressDialog(
                    static_cast<wxWindow *>(wxGetApp().mainframe), strategy_titles);
                progress_dialog->Show();
            }).wait();
        }

        auto update_strategy_ui = [&](size_t idx, const wxString &status, const wxString &result = wxEmptyString) {
            if (!progress_dialog)
                return;
            ctl.call_on_main_thread([progress_dialog, idx, status, result] {
                if (progress_dialog)
                    progress_dialog->update(idx, status, result);
            }).wait();
        };

        std::mt19937 rng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));

        for (size_t idx = 0; idx < strategies.size(); ++idx) {
            if (ctl.was_canceled())
                break;

            const auto &strategy = strategies[idx];
            update_strategy_ui(idx, _L("Running…"));
            auto selected = base_selected;
            auto unselected = base_unselected;
            auto local_params = base_params;
            Points local_bedpts;

            apply_seed_mode(selected, strategy.seed_mode, rng);

            double min_mm = unscale<double>(local_params.min_obj_distance);
            if (min_mm <= 0.0 && m_options.min_distance_mm > 0.0)
                min_mm = m_options.min_distance_mm;
            min_mm = std::max(min_mm, m_options.min_distance_mm);
            min_mm *= strategy.distance_multiplier;
            local_params.min_obj_distance = scaled(min_mm);

            double shrink_mm = std::max<double>(local_params.bed_shrink_x, min_mm);
            local_params.bed_shrink_x = shrink_mm;
            local_params.bed_shrink_y = shrink_mm;

            local_params.allow_rotations = strategy.allow_rotations && m_options.allow_rotation;
            local_params.accuracy = strategy.accuracy;
            local_params.align_to_y_axis = strategy.align_to_y;
            local_params.do_final_align = strategy.final_align;

            std::string label = (boost::format("[%1%/%2%] %3%")
                                % (idx + 1)
                                % strategies.size()
                                % strategy.title).str();

            try {
                run_arrangement(selected, unselected, local_params, local_bedpts, label);
            } catch (const std::exception &ex) {
                BOOST_LOG_TRIVIAL(warning) << "Tight fill strategy failed: " << ex.what();
                update_strategy_ui(idx, _L("Failed"), wxString::FromUTF8(ex.what()));
                continue;
            }

            coord_t clearance = local_params.min_obj_distance > 0 ?
                                local_params.min_obj_distance / 2 :
                                scaled<coord_t>(std::max(0.1, m_options.min_distance_mm));
            size_t rescued = 0;
            if (strategy.enable_bottom_left)
                rescued = bottom_left_fill(selected, unselected, local_params, local_bedpts, clearance, local_params.allow_rotations);
            if (strategy.enable_compaction)
                compaction_pass(selected, local_params, local_bedpts, clearance);

            StrategyMetrics metrics = collect_metrics(selected);
            if (!has_best || is_better(metrics, best_result.metrics)) {
                has_best = true;
                best_result.metrics = metrics;
                best_result.params = local_params;
                best_result.selected = std::move(selected);
                best_result.bedpts = std::move(local_bedpts);
                best_result.rescued = rescued;
            }

            Polygon bed_poly(local_bedpts);
            double bed_area = bed_poly.area() / (scaled<double>(1.) * scaled(1.));
            double coverage = bed_area > 0 ? (metrics.footprint_area_mm2 / bed_area) * 100.0 : 0.0;
            wxString result_text = wxString::Format(_L("%zu clones, %.1f%% coverage"), metrics.clones_on_bed, coverage);
            if (rescued > 0)
                result_text += wxString::Format(_L(", %zu rescued"), rescued);
            if (strategy.enable_compaction)
                result_text += _L(", compacted");
            update_strategy_ui(idx, _L("Completed"), result_text);
        }

        if (has_best) {
            m_selected = std::move(best_result.selected);
            params = best_result.params;
            m_bedpts = std::move(best_result.bedpts);
        } else {
            params.do_final_align = !is_bbl;
            run_arrangement(m_selected, m_unselected, params, m_bedpts, {});
        }

        if (progress_dialog) {
            ctl.call_on_main_thread([progress_dialog] {
                if (progress_dialog)
                    progress_dialog->Destroy();
            }).wait();
            progress_dialog = nullptr;
        }
    }

    const auto final_message = ctl.was_canceled()
        ? (is_tight_mode() ? _u8L("Tight bed filling canceled.") : _u8L("Bed filling canceled."))
        : (is_tight_mode() ? _u8L("Tight bed filling done.") : _u8L("Bed filling done."));

    ctl.update_status(100, final_message);
}

FillBedJob::FillBedJob(const FillBedOptions &options)
    : m_plater{wxGetApp().plater()}
    , m_options(options)
{}

void FillBedJob::finalize(bool canceled, std::exception_ptr &eptr)
{
    // Ignore the arrange result if aborted.
    if (canceled || eptr)
        return;

    if (m_object_idx == -1) return;

    ModelObject *model_object = m_plater->model().objects[m_object_idx];
    if (model_object->instances.empty()) return;

    //BBS: partplate
    PartPlateList& plate_list = m_plater->get_partplate_list();
    int plate_cols = plate_list.get_plate_cols();
    int cur_plate = plate_list.get_curr_plate_index();

    size_t inst_cnt = model_object->instances.size();

    int added_cnt = std::accumulate(m_selected.begin(), m_selected.end(), 0, [](int s, auto &ap) {
        return s + int(ap.priority == 0 && ap.bed_idx == 0);
    });

    int oldSize = m_plater->model().objects.size();

    if (added_cnt > 0) {
        //BBS: adjust the selected instances
        for (ArrangePolygon& ap : m_selected) {
            if (ap.bed_idx != 0) {
                BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":skipped: bed_id %1%, trans {%2%,%3%}") % ap.bed_idx % unscale<double>(ap.translation(X)) % unscale<double>(ap.translation(Y));
                /*if (ap.itemid == -1)*/
                    continue;
                ap.bed_idx = plate_list.get_plate_count();
            }
            else
                ap.bed_idx = cur_plate;

            if (m_selected.size() <= 100) {
                ap.row = ap.bed_idx / plate_cols;
                ap.col = ap.bed_idx % plate_cols;
                ap.translation(X) += bed_stride_x(m_plater) * ap.col;
                ap.translation(Y) -= bed_stride_y(m_plater) * ap.row;
            }

            ap.apply();

            BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << boost::format(":selected: bed_id %1%, trans {%2%,%3%}") % ap.bed_idx % unscale<double>(ap.translation(X)) % unscale<double>(ap.translation(Y));
        }

        int   newSize = m_plater->model().objects.size();
        auto obj_list = m_plater->sidebar().obj_list();
        for (size_t i = oldSize; i < newSize; i++) {
            obj_list->add_object_to_list(i, true, true, false);
            obj_list->update_printable_state(i, 0);
        }

        BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ": paste_objects_into_list";

        /*for (ArrangePolygon& ap : m_selected) {
            if (ap.bed_idx != arrangement::UNARRANGED && (ap.priority != 0 || ap.bed_idx == 0))
                ap.apply();
        }*/

        //model_object->ensure_on_bed();
        //BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ": model_object->ensure_on_bed()";

        m_plater->update();
    }
}

}} // namespace Slic3r::GUI
