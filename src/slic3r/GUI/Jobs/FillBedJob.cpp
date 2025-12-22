#include "FillBedJob.hpp"

#include "libslic3r/Model.hpp"
#include "libslic3r/ClipperUtils.hpp"
#include "libslic3r/ModelArrange.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/GUI_ObjectList.hpp"
#include "libnest2d/common.hpp"

#include <numeric>
#include <limits>
#include <boost/format.hpp>

namespace Slic3r {
namespace GUI {

namespace {

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
            std::string title;
            double accuracy;
            double distance_multiplier;
            bool allow_rotations;
            bool final_align;
            bool align_to_y;
        };

        std::vector<StrategyConfig> strategies{
            { _u8L("Baseline search"), 0.90, 1.0, false, true,  false },
            { _u8L("Rotational permutations"), 1.0, 1.0, true,  false, false },
            { _u8L("Edge-aligned sweep"), 0.95, 1.0, false, true,  true }
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
        };

        StrategyResult best_result;
        bool has_best = false;

        for (size_t idx = 0; idx < strategies.size(); ++idx) {
            if (ctl.was_canceled())
                break;

            const auto &strategy = strategies[idx];
            auto selected = base_selected;
            auto unselected = base_unselected;
            auto local_params = base_params;
            Points local_bedpts;

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
                continue;
            }

            StrategyMetrics metrics = collect_metrics(selected);
            if (!has_best || is_better(metrics, best_result.metrics)) {
                has_best = true;
                best_result.metrics = metrics;
                best_result.params = local_params;
                best_result.selected = std::move(selected);
                best_result.bedpts = std::move(local_bedpts);
            }
        }

        if (has_best) {
            m_selected = std::move(best_result.selected);
            params = best_result.params;
            m_bedpts = std::move(best_result.bedpts);
        } else {
            params.do_final_align = !is_bbl;
            run_arrangement(m_selected, m_unselected, params, m_bedpts, {});
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
