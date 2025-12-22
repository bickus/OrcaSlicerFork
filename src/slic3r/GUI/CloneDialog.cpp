#include "CloneDialog.hpp"

#include "GUI_App.hpp"
#include "MainFrame.hpp"

#include "Widgets/TextInput.hpp"
#include "Widgets/Label.hpp"

#include <algorithm>
#include <wx/valnum.h>

namespace Slic3r { namespace GUI {

namespace {

class TightFillSettingsDialog : public DPIDialog
{
public:
    TightFillSettingsDialog(wxWindow *parent, double min_distance_mm, bool allow_rotation)
        : DPIDialog(parent ? parent : static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY,
                    _L("Fill tightly options"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
    {
        min_distance_mm = std::max(0.1, std::min(10.0, min_distance_mm));

        const bool dark = wxGetApp().dark_mode();
        const auto bg = dark ? wxColour(43, 43, 43) : wxColour(247, 247, 247);
        const auto text_color = dark ? wxColour(235, 235, 235) : wxColour(30, 30, 30);
        SetBackgroundColour(bg);

        auto main_sizer = new wxBoxSizer(wxVERTICAL);

        auto distance_label = new Label(this, _L("Minimal distance between objects (mm)"));
        distance_label->SetForegroundColour(text_color);
        main_sizer->Add(distance_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

        auto distance_input = new ::TextInput(this,
                                              wxString::Format("%.2f", min_distance_mm),
                                              wxEmptyString,
                                              "",
                                              wxDefaultPosition,
                                              wxSize(FromDIP(240), -1),
                                              wxTE_PROCESS_ENTER);
        distance_input->GetTextCtrl()->SetValidator(wxFloatingPointValidator<double>(2, nullptr, wxNUM_VAL_ZERO_AS_BLANK));
        distance_input->GetTextCtrl()->SetForegroundColour(text_color);
        distance_input->GetTextCtrl()->SetBackgroundColour(dark ? wxColour(30, 30, 30) : *wxWHITE);
        distance_input->GetTextCtrl()->SetFocus();
        main_sizer->Add(distance_input, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
        m_distance_input = distance_input;

        m_rotation_cb = new ::CheckBox(this);
        m_rotation_cb->SetValue(allow_rotation);
        auto rotation_row = new wxBoxSizer(wxHORIZONTAL);
        rotation_row->Add(m_rotation_cb, 0, wxALIGN_CENTER_VERTICAL);
        auto rotation_label = new Label(this, _L("Allow rotation of objects"));
        rotation_label->SetForegroundColour(text_color);
        rotation_row->Add(rotation_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
        main_sizer->Add(rotation_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));

        auto buttons_sizer = new wxBoxSizer(wxHORIZONTAL);
        buttons_sizer->AddStretchSpacer();

        auto ok_btn = new Button(this, _L("OK"));
        ok_btn->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);
        ok_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });

        auto cancel_btn = new Button(this, _L("Cancel"));
        cancel_btn->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
        cancel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });

        buttons_sizer->Add(ok_btn, 0, wxRIGHT, FromDIP(8));
        buttons_sizer->Add(cancel_btn, 0);
        main_sizer->Add(buttons_sizer, 0, wxEXPAND | wxALL, FromDIP(10));

        SetSizerAndFit(main_sizer);
        CentreOnParent();
        wxGetApp().UpdateDlgDarkUI(this);
    }

    double min_distance_mm() const
    {
        double value = 0.1;
        if (auto ctrl = m_distance_input ? m_distance_input->GetTextCtrl() : nullptr) {
            ctrl->GetValue().ToDouble(&value);
        }
        return std::max(0.1, std::min(10.0, value));
    }

    bool allow_rotation() const { return m_rotation_cb->GetValue(); }

private:
    ::TextInput *m_distance_input { nullptr };
    ::CheckBox *m_rotation_cb { nullptr };

    void on_dpi_changed(const wxRect &) override {}
};

} // namespace

CloneDialog::CloneDialog(wxWindow *parent)
    : DPIDialog(parent ? parent : static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY, _L("Clone"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    const bool dark = wxGetApp().dark_mode();
    const auto text_color = dark ? wxColour(235, 235, 235) : wxColour(32, 32, 32);
    SetBackgroundColour(dark ? wxColour(43, 43, 43) : *wxWHITE);
    SetFont(Label::Body_14);

    m_plater = wxGetApp().plater();
    m_config = wxGetApp().app_config;
    m_cancel_process = false;

    auto v_sizer = new wxBoxSizer(wxVERTICAL);
    auto f_sizer = new wxFlexGridSizer(2, 2, FromDIP(4) , FromDIP(20));

    auto count_label = new wxStaticText(this, wxID_ANY, _L("Number of copies:"), wxDefaultPosition, wxDefaultSize, 0);
    count_label->SetForegroundColour(text_color);
    m_count_spin = new SpinInput(this, wxEmptyString, "", wxDefaultPosition, wxSize(FromDIP(120), -1), wxSP_ARROW_KEYS, 1, 1000, 1);
    m_count_spin->GetTextCtrl()->SetFocus();
    f_sizer->Add(count_label  , 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    f_sizer->Add(m_count_spin, 0, wxALIGN_CENTER_VERTICAL);

    auto arrange_label = new wxStaticText(this, wxID_ANY, _L("Auto arrange plate after cloning") + ":", wxDefaultPosition, wxDefaultSize, 0);
    arrange_label->Wrap(FromDIP(300));
    arrange_label->SetForegroundColour(text_color);
    m_arrange_cb = new ::CheckBox(this);
    m_arrange_cb->SetValue(m_config->get("auto_arrange") == "true");
    f_sizer->Add(arrange_label, 0, wxEXPAND | wxALIGN_CENTER_VERTICAL);
    f_sizer->Add(m_arrange_cb , 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(5));

    v_sizer->Add(f_sizer, 1, wxEXPAND | wxALL, FromDIP(10));

    auto bottom_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_progress = new ProgressBar(this, wxID_ANY, 100);
    m_progress->SetHeight(FromDIP(8));
    m_progress->SetMaxSize(wxSize(-1, FromDIP(8)));
    m_progress->SetProgressForedColour(StateColor::darkModeColorFor(wxColour("#DFDFDF")));
    m_progress->SetDoubleBuffered(true);
    m_progress->Hide();
    bottom_sizer->Add(m_progress, 2, wxEXPAND | wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(10));

    auto button_row = new wxBoxSizer(wxHORIZONTAL);

    auto fill_buttons = new wxBoxSizer(wxHORIZONTAL);
    auto fill_btn = new Button(this, _L("Fill"));
    fill_btn->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
    fill_btn->SetToolTip(_L("Fill bed with copies"));

    auto fill_tight_btn = new Button(this, _L("Fill tightly"));
    fill_tight_btn->SetStyle(ButtonStyle::Regular, ButtonType::Choice);
    fill_tight_btn->SetToolTip(_L("Open tight fill options"));

    fill_buttons->Add(fill_btn, 0, wxRIGHT, FromDIP(6));
    fill_buttons->Add(fill_tight_btn, 0);

    auto confirm_buttons = new wxBoxSizer(wxHORIZONTAL);
    auto ok_btn = new Button(this, _L("OK"));
    ok_btn->SetStyle(ButtonStyle::Confirm, ButtonType::Choice);
    auto cancel_btn = new Button(this, _L("Cancel"));
    cancel_btn->SetStyle(ButtonStyle::Regular, ButtonType::Choice);

    confirm_buttons->Add(ok_btn, 0, wxRIGHT, FromDIP(6));
    confirm_buttons->Add(cancel_btn, 0);

    button_row->Add(fill_buttons, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    button_row->AddStretchSpacer();
    button_row->Add(confirm_buttons, 0, wxALIGN_CENTER_VERTICAL);

    fill_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_plater->fill_bed_with_instances();
        EndModal(wxID_OK);
    });

    fill_tight_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        double stored_distance = 1.0;
        const wxString dist_str = wxString::FromUTF8(m_config->get("tight_fill_min_distance_mm"));
        if (!dist_str.empty()) {
            double parsed = wxAtof(dist_str);
            if (parsed > 0.0)
                stored_distance = std::max(0.1, std::min(10.0, parsed));
        }

        const bool allow_rotation = m_config->get_bool("tight_fill_allow_rotation");
        TightFillSettingsDialog dlg(this, stored_distance, allow_rotation);
        if (dlg.ShowModal() == wxID_OK) {
            const double distance = dlg.min_distance_mm();
            const bool rotation = dlg.allow_rotation();

            m_config->set("tight_fill_min_distance_mm",
                          wxString::Format("%.2f", distance).ToStdString());
            m_config->set_bool("tight_fill_allow_rotation", rotation);

            FillBedOptions options;
            options.mode = FillBedMode::Tight;
            options.min_distance_mm = distance;
            options.allow_rotation = rotation;
            options.enable_multi_strategy = true;

            m_plater->fill_bed_with_instances_tightly(options);
            EndModal(wxID_OK);
        }
    });

    ok_btn->Bind(wxEVT_BUTTON, [this, fill_btn, fill_tight_btn, ok_btn, v_sizer](wxCommandEvent &) {

        m_count_spin->Disable(); // also ensures input box value applied with wxEVT_KILL_FOCUS
        m_arrange_cb->Disable();

        m_count = m_count_spin->GetValue();

        m_progress->Show();

        ok_btn->Hide();
        fill_btn->Hide();
        fill_tight_btn->Hide();

        this->Layout();
        v_sizer->Fit(this);
        Refresh();

        Selection& sel = m_plater->canvas3D()->get_selection();
        m_plater->take_snapshot(std::string("Selection-clone"));
        m_plater->Freeze(); // Better to stop rendering canvas while processing
        sel.copy_to_clipboard();
        for (int i = 0; i < m_count; i++) { // same method with Selection::clone()
            m_progress->SetValue(static_cast<int>(static_cast<double>(i) / m_count * 100)); // pass 0 / 100
            sel.paste_from_clipboard();
            if(m_cancel_process){
                m_plater->undo();
                return;
            }
            wxYield(); // Allow event loop to process updates
        }

        if(!m_cancel_process){
            if (m_arrange_cb->GetValue()){
                m_plater->set_prepare_state(Job::PREPARE_STATE_MENU);
                m_plater->arrange();
            }
            m_plater->Thaw();
            EndModal(wxID_OK);
        }
    });

    cancel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_cancel_process = true;
        if(m_plater->IsFrozen())
            m_plater->Thaw();
        EndModal(wxID_CANCEL);
    });

    bottom_sizer->Add(button_row, 1, wxEXPAND | wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(10));

    v_sizer->Add(bottom_sizer, 0, wxEXPAND);

    this->SetSizer(v_sizer);
    this->Layout();
    v_sizer->Fit(this);
    wxGetApp().UpdateDlgDarkUI(this);
}

CloneDialog::~CloneDialog() {}

}} // namespace Slic3r::GUI