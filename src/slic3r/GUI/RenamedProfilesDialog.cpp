#include "RenamedProfilesDialog.hpp"

#include <wx/stattext.h>

#include "GUI_App.hpp"
#include "Widgets/Label.hpp"

namespace Slic3r {
namespace GUI {

namespace {
wxString type_label(Preset::Type type)
{
    switch (type) {
    case Preset::TYPE_PRINTER:  return _L("Printer");
    case Preset::TYPE_FILAMENT: return _L("Material");
    default:                    return _L("Preset");
    }
}
}

RenamedProfilesDialog::RenamedProfilesDialog(wxWindow *parent, const std::vector<RenameUpdateOption> &options)
    : DPIDialog(parent,
                wxID_ANY,
                _L("Update renamed presets"),
                wxDefaultPosition,
                wxDefaultSize,
                wxCAPTION | wxCLOSE_BOX),
      m_options(options)
{
    SetBackgroundColour(wxColour(255, 255, 255));

    auto main_sizer = new wxBoxSizer(wxVERTICAL);

    auto intro = new wxStaticText(this,
                                  wxID_ANY,
                                  _L("The following presets were renamed. Select which ones you would like to update in this project."));
    intro->SetFont(Label::Body_12);
    intro->Wrap(FromDIP(420));
    main_sizer->Add(intro, 0, wxEXPAND | wxALL, FromDIP(10));

    auto list_sizer = new wxBoxSizer(wxVERTICAL);
    for (const auto &option : m_options) {
        auto checkbox = new ::CheckBox(this);
        checkbox->SetLabel(describe_option(option));
        checkbox->SetValue(true);
        checkbox->Wrap(FromDIP(420));
        list_sizer->Add(checkbox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(5));
        m_checkboxes.push_back(checkbox);
    }
    main_sizer->Add(list_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));

    m_buttons = new DialogButtons(this, {_L("OK"), _L("Cancel")});
    m_buttons->GetOK()->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
    m_buttons->GetCANCEL()->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    main_sizer->Add(m_buttons, 0, wxEXPAND | wxALL, FromDIP(10));

    SetSizer(main_sizer);
    main_sizer->Fit(this);
    Centre(wxBOTH);

    wxGetApp().UpdateDlgDarkUI(this);
}

std::vector<RenameUpdateOption> RenamedProfilesDialog::selection() const
{
    std::vector<RenameUpdateOption> selected;
    const size_t count = std::min(m_options.size(), m_checkboxes.size());
    for (size_t idx = 0; idx < count; ++idx) {
        if (m_checkboxes[idx]->GetValue())
            selected.push_back(m_options[idx]);
    }
    return selected;
}

void RenamedProfilesDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    Fit();
    if (suggested_rect.IsEmpty())
        CentreOnParent();
}

wxString RenamedProfilesDialog::describe_option(const RenameUpdateOption &option) const
{
    wxString arrow = wxString::FromUTF8(" \u2192 "); // arrow symbol
    return wxString::Format("%s: %s%s%s",
                            type_label(option.type),
                            from_u8(option.old_name),
                            arrow,
                            from_u8(option.new_name));
}

} // namespace GUI
} // namespace Slic3r
