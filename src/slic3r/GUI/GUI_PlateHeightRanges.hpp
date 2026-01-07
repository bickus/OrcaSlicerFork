#ifndef slic3r_GUI_PlateHeightRanges_hpp_
#define slic3r_GUI_PlateHeightRanges_hpp_

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/collpane.h>

#include "libslic3r/Slicing.hpp"
#include "libslic3r/Model.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"

namespace Slic3r { namespace GUI {

class PlateHeightRangeEditor : public wxTextCtrl
{
public:
    enum class Type { MinZ, MaxZ };

    PlateHeightRangeEditor(wxWindow* parent, coordf_t value, Type type);

    coordf_t get_value() const;
    void set_value(coordf_t value);
    Type get_type() const { return m_type; }

private:
    Type m_type;
    coordf_t m_value;

    void on_text_enter(wxCommandEvent& event);
    void on_kill_focus(wxFocusEvent& event);
    void commit_value();
};

// A single height range item with min/max Z inputs, settings button, and delete button
class PlateHeightRangeItem : public wxPanel
{
public:
    PlateHeightRangeItem(wxWindow* parent,
                         const t_layer_height_range& range,
                         const ModelConfig& config,
                         std::function<void(PlateHeightRangeItem*)> on_delete,
                         std::function<void(PlateHeightRangeItem*)> on_settings,
                         std::function<void()> on_range_changed);

    t_layer_height_range get_range() const;
    void set_range(const t_layer_height_range& range);

    const ModelConfig& get_config() const { return m_config; }
    ModelConfig& get_config() { return m_config; }
    void set_config(const ModelConfig& config);

    void update_settings_summary();

private:
    PlateHeightRangeEditor* m_min_z_editor { nullptr };
    PlateHeightRangeEditor* m_max_z_editor { nullptr };
    ScalableButton* m_settings_btn { nullptr };
    ScalableButton* m_delete_btn { nullptr };
    wxStaticText* m_settings_summary { nullptr };

    ModelConfig m_config;

    std::function<void(PlateHeightRangeItem*)> m_on_delete;
    std::function<void(PlateHeightRangeItem*)> m_on_settings;
    std::function<void()> m_on_range_changed;
};

// Panel containing all height range items with add button
class PlateHeightRangesPanel : public wxPanel
{
public:
    PlateHeightRangesPanel(wxWindow* parent);

    void sync_ranges(const t_layer_config_ranges& ranges);
    t_layer_config_ranges get_ranges() const;

    bool has_ranges() const { return !m_range_items.empty(); }
    void clear_ranges();

private:
    wxBoxSizer* m_items_sizer { nullptr };
    ScalableButton* m_add_btn { nullptr };
    wxStaticText* m_empty_text { nullptr };

    std::vector<PlateHeightRangeItem*> m_range_items;

    ScalableBitmap m_bmp_add;
    ScalableBitmap m_bmp_delete;
    ScalableBitmap m_bmp_settings;

    void add_range(const t_layer_height_range& range = {0.0, 10.0},
                   const ModelConfig& config = ModelConfig());
    void delete_range(PlateHeightRangeItem* item);
    void on_settings(PlateHeightRangeItem* item);
    void on_range_changed();
    void update_empty_state();
    void sort_ranges();
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_PlateHeightRanges_hpp_
