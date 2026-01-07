#include "GUI_PlateHeightRanges.hpp"
#include "GUI_App.hpp"
#include "GUI_Factories.hpp"
#include "MsgDialog.hpp"
#include "I18N.hpp"
#include "Widgets/Label.hpp"

#include <wx/wupdlock.h>
#include <algorithm>

namespace Slic3r { namespace GUI {

// PlateHeightRangeEditor implementation

PlateHeightRangeEditor::PlateHeightRangeEditor(wxWindow* parent, coordf_t value, Type type)
    : wxTextCtrl(parent, wxID_ANY, wxString::Format("%.2f", value),
                 wxDefaultPosition, wxSize(parent->FromDIP(70), -1),
                 wxTE_PROCESS_ENTER)
    , m_type(type)
    , m_value(value)
{
    SetFont(Label::Body_14);

    Bind(wxEVT_TEXT_ENTER, &PlateHeightRangeEditor::on_text_enter, this);
    Bind(wxEVT_KILL_FOCUS, &PlateHeightRangeEditor::on_kill_focus, this);
}

coordf_t PlateHeightRangeEditor::get_value() const
{
    return m_value;
}

void PlateHeightRangeEditor::set_value(coordf_t value)
{
    m_value = value;
    SetValue(wxString::Format("%.2f", value));
}

void PlateHeightRangeEditor::on_text_enter(wxCommandEvent& event)
{
    commit_value();
    event.Skip();
}

void PlateHeightRangeEditor::on_kill_focus(wxFocusEvent& event)
{
    commit_value();
    event.Skip();
}

void PlateHeightRangeEditor::commit_value()
{
    double new_value;
    if (GetValue().ToDouble(&new_value)) {
        if (new_value < 0.0)
            new_value = 0.0;
        m_value = new_value;
    }
    SetValue(wxString::Format("%.2f", m_value));

    // Notify parent of change
    wxCommandEvent evt(wxEVT_TEXT_ENTER, GetId());
    evt.SetEventObject(this);
    wxPostEvent(GetParent(), evt);
}

// PlateHeightRangeItem implementation

PlateHeightRangeItem::PlateHeightRangeItem(wxWindow* parent,
                                           const t_layer_height_range& range,
                                           const ModelConfig& config,
                                           std::function<void(PlateHeightRangeItem*)> on_delete,
                                           std::function<void(PlateHeightRangeItem*)> on_settings,
                                           std::function<void()> on_range_changed)
    : wxPanel(parent, wxID_ANY)
    , m_config(config)
    , m_on_delete(on_delete)
    , m_on_settings(on_settings)
    , m_on_range_changed(on_range_changed)
{
    SetBackgroundColour(parent->GetBackgroundColour());

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

    // Min Z
    auto* min_label = new wxStaticText(this, wxID_ANY, _L("Z:"));
    min_label->SetFont(Label::Body_14);
    m_min_z_editor = new PlateHeightRangeEditor(this, range.first, PlateHeightRangeEditor::Type::MinZ);

    // To label
    auto* to_label = new wxStaticText(this, wxID_ANY, _L("to"));
    to_label->SetFont(Label::Body_14);

    // Max Z
    m_max_z_editor = new PlateHeightRangeEditor(this, range.second, PlateHeightRangeEditor::Type::MaxZ);

    // mm label
    auto* mm_label = new wxStaticText(this, wxID_ANY, _L("mm"));
    mm_label->SetFont(Label::Body_14);

    // Settings button
    m_settings_btn = new ScalableButton(this, wxID_ANY, "cog");
    m_settings_btn->SetToolTip(_L("Edit settings for this height range"));
    m_settings_btn->SetBackgroundColour(GetBackgroundColour());

    // Delete button
    m_delete_btn = new ScalableButton(this, wxID_ANY, "delete_filament");
    m_delete_btn->SetToolTip(_L("Delete this height range"));
    m_delete_btn->SetBackgroundColour(GetBackgroundColour());

    row_sizer->Add(min_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(5));
    row_sizer->Add(m_min_z_editor, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(5));
    row_sizer->Add(to_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(5));
    row_sizer->Add(m_max_z_editor, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(5));
    row_sizer->Add(mm_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    row_sizer->AddStretchSpacer();
    row_sizer->Add(m_settings_btn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(5));
    row_sizer->Add(m_delete_btn, 0, wxALIGN_CENTER_VERTICAL);

    // Settings summary
    m_settings_summary = new wxStaticText(this, wxID_ANY, "");
    m_settings_summary->SetFont(Label::Body_12);
    m_settings_summary->SetForegroundColour(wxColour(128, 128, 128));

    main_sizer->Add(row_sizer, 0, wxEXPAND);
    main_sizer->Add(m_settings_summary, 0, wxLEFT | wxTOP, FromDIP(20));

    SetSizer(main_sizer);

    // Bind events
    m_delete_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_on_delete)
            m_on_delete(this);
    });

    m_settings_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_on_settings)
            m_on_settings(this);
    });

    auto on_range_edit = [this](wxCommandEvent&) {
        // Validate min < max (must have non-zero height)
        coordf_t min_z = m_min_z_editor->get_value();
        coordf_t max_z = m_max_z_editor->get_value();
        constexpr coordf_t min_range_height = 0.01;  // Minimum 0.01mm range height
        if (max_z <= min_z) {
            // Set max_z to at least min_z + minimum height
            m_max_z_editor->set_value(min_z + min_range_height);
        }
        if (m_on_range_changed)
            m_on_range_changed();
    };

    m_min_z_editor->Bind(wxEVT_TEXT_ENTER, on_range_edit);
    m_max_z_editor->Bind(wxEVT_TEXT_ENTER, on_range_edit);

    update_settings_summary();
}

t_layer_height_range PlateHeightRangeItem::get_range() const
{
    return { m_min_z_editor->get_value(), m_max_z_editor->get_value() };
}

void PlateHeightRangeItem::set_range(const t_layer_height_range& range)
{
    m_min_z_editor->set_value(range.first);
    m_max_z_editor->set_value(range.second);
}

void PlateHeightRangeItem::set_config(const ModelConfig& config)
{
    m_config = config;
    update_settings_summary();
}

void PlateHeightRangeItem::update_settings_summary()
{
    if (m_config.empty()) {
        m_settings_summary->SetLabel(_L("No settings configured"));
        m_settings_summary->Show(true);
        return;
    }

    wxString summary;
    const std::vector<std::string>& keys = m_config.keys();
    int count = 0;
    const int max_display = 3;

    for (const std::string& key : keys) {
        if (count >= max_display) {
            summary += wxString::Format("  ... +%d more", (int)keys.size() - max_display);
            break;
        }
        if (!summary.empty())
            summary += "\n";
        summary += "  " + wxString::FromUTF8(key) + ": " + wxString::FromUTF8(m_config.opt_serialize(key));
        count++;
    }

    m_settings_summary->SetLabel(summary);
    m_settings_summary->Show(true);

    GetParent()->Layout();
    GetParent()->Fit();
}

// PlateHeightRangesPanel implementation

PlateHeightRangesPanel::PlateHeightRangesPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
    , m_bmp_add(this, "add_filament")
    , m_bmp_delete(this, "delete_filament")
    , m_bmp_settings(this, "cog")
{
    SetBackgroundColour(*wxWHITE);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    // Title row with add button
    wxBoxSizer* title_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto* title = new wxStaticText(this, wxID_ANY, _L("Height Range Modifiers"));
    title->SetFont(Label::Head_14);

    m_add_btn = new ScalableButton(this, wxID_ANY, m_bmp_add);
    m_add_btn->SetToolTip(_L("Add a new height range modifier"));
    m_add_btn->SetBackgroundColour(GetBackgroundColour());

    title_sizer->Add(title, 0, wxALIGN_CENTER_VERTICAL);
    title_sizer->AddStretchSpacer();
    title_sizer->Add(m_add_btn, 0, wxALIGN_CENTER_VERTICAL);

    // Empty state text
    m_empty_text = new wxStaticText(this, wxID_ANY,
        _L("No height range modifiers defined.\nClick + to add a modifier that applies settings at specific Z heights."));
    m_empty_text->SetFont(Label::Body_12);
    m_empty_text->SetForegroundColour(wxColour(128, 128, 128));

    // Items container
    m_items_sizer = new wxBoxSizer(wxVERTICAL);

    main_sizer->Add(title_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    main_sizer->Add(m_empty_text, 0, wxALIGN_CENTER | wxBOTTOM, FromDIP(10));
    main_sizer->Add(m_items_sizer, 0, wxEXPAND);

    SetSizer(main_sizer);

    // Bind events
    m_add_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        Freeze();
        add_range();
        update_empty_state();
        GetParent()->Layout();
        GetParent()->Fit();
        Thaw();
    });

    update_empty_state();
}

void PlateHeightRangesPanel::sync_ranges(const t_layer_config_ranges& ranges)
{
    Freeze();
    clear_ranges();

    for (const auto& range_pair : ranges) {
        add_range(range_pair.first, range_pair.second);
    }

    update_empty_state();
    Layout();
    Fit();
    Thaw();
}

t_layer_config_ranges PlateHeightRangesPanel::get_ranges() const
{
    t_layer_config_ranges result;
    for (const auto* item : m_range_items) {
        result[item->get_range()] = item->get_config();
    }
    return result;
}

void PlateHeightRangesPanel::clear_ranges()
{
    for (auto* item : m_range_items) {
        m_items_sizer->Detach(item);
        item->Destroy();
    }
    m_range_items.clear();
}

void PlateHeightRangesPanel::add_range(const t_layer_height_range& range, const ModelConfig& config)
{
    auto* item = new PlateHeightRangeItem(
        this,
        range,
        config,
        [this](PlateHeightRangeItem* i) { delete_range(i); },
        [this](PlateHeightRangeItem* i) { on_settings(i); },
        [this]() { on_range_changed(); }
    );

    m_range_items.push_back(item);
    m_items_sizer->Add(item, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
}

void PlateHeightRangesPanel::delete_range(PlateHeightRangeItem* item)
{
    auto it = std::find(m_range_items.begin(), m_range_items.end(), item);
    if (it != m_range_items.end()) {
        m_range_items.erase(it);
        m_items_sizer->Detach(item);
        item->Destroy();

        update_empty_state();
        GetParent()->Layout();
        GetParent()->Fit();
    }
}

void PlateHeightRangesPanel::on_settings(PlateHeightRangeItem* item)
{
    // Get current config
    DynamicPrintConfig config;
    config.apply(item->get_config().get());

    // Get available settings for layer modifiers
    SettingsFactory::Bundle cat_options = SettingsFactory::get_bundle(&config, true, true);

    // Build list of all possible options
    std::vector<std::string> all_options;
    for (const auto& cat : cat_options) {
        for (const auto& opt : cat.second) {
            all_options.push_back(opt);
        }
    }

    // If no options configured yet, get all available layer settings
    if (all_options.empty()) {
        auto all_cat_options = SettingsFactory::get_all_visible_options(false);
        for (const auto& cat : all_cat_options) {
            for (const auto& opt : cat.second) {
                all_options.push_back(opt.name);
            }
        }
    }

    // Create settings dialog - only include options with valid ConfigOptionDef
    wxArrayString choices;
    wxArrayInt selections;
    std::vector<std::string> valid_options;  // Track which options made it into choices

    std::vector<std::string> current_keys = item->get_config().keys();

    for (size_t i = 0; i < all_options.size(); ++i) {
        const std::string& opt = all_options[i];
        const ConfigOptionDef* def = print_config_def.get(opt);
        if (def) {
            valid_options.push_back(opt);  // Track the valid option
            choices.Add(wxString::FromUTF8(def->label.empty() ? opt : def->label));
            if (std::find(current_keys.begin(), current_keys.end(), opt) != current_keys.end()) {
                selections.Add(static_cast<int>(valid_options.size()) - 1);  // Use index into valid_options
            }
        }
    }

    wxMultiChoiceDialog dlg(this,
        _L("Select settings to override in this height range:"),
        _L("Height Range Settings"),
        choices);
    dlg.SetSelections(selections);

    if (dlg.ShowModal() == wxID_OK) {
        selections = dlg.GetSelections();

        // Build new config with selected options
        DynamicPrintConfig new_config;
        const DynamicPrintConfig& full_config = wxGetApp().preset_bundle->full_config();

        for (int idx : selections) {
            if (idx >= 0 && idx < static_cast<int>(valid_options.size())) {
                const std::string& opt_key = valid_options[idx];  // Use valid_options, not all_options
                if (full_config.has(opt_key)) {
                    new_config.set_key_value(opt_key, full_config.option(opt_key)->clone());
                }
            }
        }

        ModelConfig new_model_config;
        new_model_config.assign_config(new_config);
        item->set_config(new_model_config);
        item->update_settings_summary();

        Layout();
        Fit();
        GetParent()->Layout();
        GetParent()->Fit();
    }
}

void PlateHeightRangesPanel::on_range_changed()
{
    sort_ranges();
}

void PlateHeightRangesPanel::update_empty_state()
{
    bool is_empty = m_range_items.empty();
    m_empty_text->Show(is_empty);
    m_items_sizer->Show(!is_empty);
}

void PlateHeightRangesPanel::sort_ranges()
{
    // Sort items by min Z value
    std::sort(m_range_items.begin(), m_range_items.end(),
        [](const PlateHeightRangeItem* a, const PlateHeightRangeItem* b) {
            return a->get_range().first < b->get_range().first;
        });

    // Reorder in sizer
    m_items_sizer->Clear(false);  // Don't delete windows
    for (auto* item : m_range_items) {
        m_items_sizer->Add(item, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }

    Layout();
}

}} // namespace Slic3r::GUI
