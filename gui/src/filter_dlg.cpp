/***************************************************************************
 *   Copyright (C) 2025  Alec Leamas                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 **************************************************************************/

/**
 * \file
 * Implement filter_dlg.h
 */

#include <functional>
#include <memory>
#include <string>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choicdlg.h>
#include <wx/frame.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>
#include <wx/wrapsizer.h>

#include "model/comm_drv_registry.h"
#include "model/comm_navmsg_bus.h"
#include "model/filters_on_disk.h"
#include "filter_dlg.h"
#include "std_filesystem.h"
#include "svg_icons.h"

const char* const kFilterExists =
    _(R"( Filter already exists. Please use Edit to update
or Remove to delete it prior to create.)");

template <typename T>
T* GetWindowById(int id) {
  return dynamic_cast<T*>(wxWindow::FindWindowById(id));
};

static wxArrayString GetUserFilters() {
  auto std_filters = filters_on_disk::List(false);
  wxArrayString wx_filters;
  for (auto& f : std_filters) wx_filters.Add(fs::path(f).stem().string());
  return wx_filters;
}

class NewFilterDlg : public wxTextEntryDialog {
public:
  NewFilterDlg(wxWindow* parent)
      : wxTextEntryDialog(wxTheApp->GetTopWindow(), _("New filter name")) {}
};

class DeleteFilterDlg : public wxSingleChoiceDialog {
public:
  DeleteFilterDlg(wxWindow* parent)
      : wxSingleChoiceDialog(wxTheApp->GetTopWindow(),
                             _("Remove filter (name):"), _("Remove filter"),
                             GetUserFilters()) {}
};

class SelectFilterDlg : public wxSingleChoiceDialog {
public:
  SelectFilterDlg(wxWindow* parent)
      : wxSingleChoiceDialog(wxTheApp->GetTopWindow(), _("Edit filter (name):"),
                             _("Edit filter"), GetUserFilters()) {}
};

class BadFilterNameDlg : public wxMessageDialog {
public:
  BadFilterNameDlg(wxWindow* parent) : wxMessageDialog(parent, kFilterExists) {}
};

/** The description with optional editing. */
class DescriptionPanel : public wxPanel {
public:
  DescriptionPanel(wxWindow* parent, NavmsgFilter& filter,
                   std::function<void()> on_update)
      : wxPanel(parent, wxID_ANY),
        m_filter(filter),
        m_is_editing(true),
        m_on_update(on_update),
        kEditBtnId(wxWindow::NewControlId()),
        kTextId(wxWindow::NewControlId()),
        kTextEntryId(wxWindow::NewControlId()) {
    auto flags = wxSizerFlags().Border();
    auto vbox = new wxStaticBoxSizer(wxVERTICAL, this, _("General"));
    auto hbox = new wxBoxSizer(wxHORIZONTAL);
    auto text = m_filter.m_description;
    hbox->Add(new wxStaticText(this, wxID_ANY, ("Description")), flags);

    auto size = wxSize(48 * GetCharWidth(), GetCharHeight());
    auto entry =
        new wxTextCtrl(this, kTextEntryId, text, wxDefaultPosition, size);
    entry->SetValue(text);
    entry->Hide();
    hbox->Add(entry, flags);

    if (text.empty()) text = _("(Empty)");
    hbox->Add(new wxStaticText(this, kTextId, text), flags);
    auto button = new EditButton(this, kEditBtnId, [&] { OnEditClick(); });
    hbox->Add(button /**, flag**/);

    vbox->Add(hbox, flags);
    SetSizer(vbox);
    Fit();
    Show();
  }

private:
  /** Rightmost button, either "Edit" or "Done" depending on state. */
  class EditButton : public wxButton {
  public:
    EditButton(wxWindow* parent, int id, std::function<void()> on_click)
        : wxButton(parent, id, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                   wxBU_EXACTFIT | wxBU_BOTTOM),
          m_on_click(on_click) {
      Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { m_on_click(); });
      SetIcon(false);
    }

    void SetIcon(bool is_editing) {
      char buffer[2048];
      strcpy(buffer, is_editing ? kCheckmarkSvg : kEditPenSvg);
#ifdef ocpnUSE_wxBitmapBundle
      auto icon_size = wxSize(GetCharHeight(), GetCharHeight());
      auto bundle = wxBitmapBundle::FromSVG(buffer, icon_size);
      assert(bundle.IsOk() && "Cannot load svg icon");
      SetBitmap(bundle);
#else
      wxStringInputStream wis(buffer);
      wxSVGDocument svg_doc(wis);
      wxImage image = svg_doc.Render(GetCharHeight(), GetCharHeight());
      assert(wxBitmap(image).IsOk() && "Cannot load svg icon");
      SetBitmap(wxBitmap(image));
#endif
    }

  private:
    std::function<void()> m_on_click;
  };

  void OnEditClick() {
    auto entry = GetWindowById<wxTextCtrl>(kTextEntryId);
    auto text = GetWindowById<wxStaticText>(kTextId);
    auto button = GetWindowById<EditButton>(kEditBtnId);
    if (m_is_editing) {
      m_filter.m_description = entry->GetValue();
      m_on_update();
    } else {
      entry->SetValue(m_filter.m_description);
    }
    text->SetLabel(entry->GetValue());
    if (text->GetLabel() == "") text->SetLabel(_("(empty)"));
    m_is_editing = !m_is_editing;
    button->SetIcon(m_is_editing);
    text->Show(!m_is_editing);
    entry->Show(m_is_editing);
    GetParent()->Fit();
  }

  NavmsgFilter& m_filter;
  bool m_is_editing;
  std::function<void()> m_on_update;
  const int kEditBtnId;
  const int kTextId;
  const int kTextEntryId;
};

class InterfacePanel : public wxPanel {
public:
  InterfacePanel(wxWindow* parent, NavmsgFilter& filter,
                 std::function<void()> on_update)
      : wxPanel(parent, wxID_ANY),
        m_filter(filter),
        m_on_update(on_update),
        kCheckboxId(wxWindow::NewControlId()),
        kListboxId(wxWindow::NewControlId()) {
    auto flags = wxSizerFlags().Border();
    auto vbox = new wxStaticBoxSizer(wxVERTICAL, this, _("Interfaces"));
    auto hbox = new wxBoxSizer(wxHORIZONTAL);
    hbox->Add(new wxStaticText(this, wxID_ANY, _("Use interfaces:")), flags);
    auto checkbox = new wxCheckBox(this, kCheckboxId, _("All"));
    hbox->Add(checkbox, flags);

    wxArrayString choices;
    for (auto& driver : CommDriverRegistry::GetInstance().GetDrivers())
      choices.Add(driver->iface);
    choices.Add("Internal");
    auto listbox = new wxCheckListBox(this, kListboxId, wxDefaultPosition,
                                      wxDefaultSize, choices);
    hbox->Add(listbox, flags);

    vbox->Add(hbox);
    SetSizer(vbox);
    Layout();

    LoadFromFilter();
    listbox->Show(!checkbox->IsChecked());
    checkbox->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent&) { OnCheckboxClick(); });
    listbox->Bind(wxEVT_CHECKLISTBOX,
                  [&](wxCommandEvent& ev) { OnItemCheck(ev.GetInt()); });
  }

private:
  void OnCheckboxClick() {
    auto listbox = GetWindowById<wxCheckListBox>(kListboxId);
    auto checkbox = GetWindowById<wxCheckBox>(kCheckboxId);

    UpdateFilter();
    if (checkbox->IsChecked()) {
      m_filter.interfaces.clear();
      for (unsigned i = 0; i < listbox->GetCount(); i += 1) {
        listbox->Check(i);
        m_filter.interfaces.insert(listbox->GetString(i).ToStdString());
      }
    }
    m_on_update();
    listbox->Show(!checkbox->IsChecked());
    GetParent()->Fit();
  }

  void LoadFromFilter() {
    auto checkbox = GetWindowById<wxCheckBox>(kCheckboxId);
    checkbox->SetValue(true);
    auto listbox = GetWindowById<wxCheckListBox>(kListboxId);
    for (unsigned i = 0; i < listbox->GetCount(); i += 1) {
      if (m_filter.interfaces.count(listbox->GetString(i).ToStdString()) > 0)
        listbox->Check(i);
      if (!listbox->IsChecked(i)) checkbox->SetValue(false);
    }
  }

  void OnItemCheck(int ix) {
    auto listbox = GetWindowById<wxCheckListBox>(kListboxId);
    int checked = 0;
    for (unsigned i = 0; i < listbox->GetCount(); i += 1)
      if (listbox->IsChecked(i)) checked += 1;
    if (checked == 0) {
      // Refuse to create a filter with no interfaces.
      listbox->Check(ix);
      return;
    }
    UpdateFilter();
  }
  void UpdateFilter() {
    auto checkbox = GetWindowById<wxCheckBox>(kCheckboxId);
    m_filter.interfaces.clear();
    auto listbox = GetWindowById<wxCheckListBox>(kListboxId);
    for (unsigned i = 0; i < listbox->GetCount(); i += 1) {
      if (!listbox->IsChecked(i)) continue;
      auto iface = listbox->GetString(i).ToStdString();
      m_filter.interfaces.insert(iface);
    }
    m_on_update();
  }

  NavmsgFilter& m_filter;
  std::function<void()> m_on_update;
  const int kCheckboxId;
  const int kListboxId;
};

class BusPanel : public wxPanel {};

class DirectionPanel : public wxPanel {};

class AcceptedPanel : public wxPanel {};

class MessageTypePanel : public wxPanel {
  // FIXME: Oops, this is the message types...
  // auto interfaces = NavMsgBus::GetInstance().GetActiveMessages();
  // for (auto iface : interfaces) choices.Add(iface);
};

class EditFilterFrame : public wxFrame {
  class Buttons : public wxPanel {
  public:
    Buttons(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
      auto vbox = new wxBoxSizer(wxVERTICAL);
      auto buttons = new wxStdDialogButtonSizer();
      buttons->AddButton(new wxButton(this, wxID_CLOSE));
      vbox->Add(buttons, wxSizerFlags().Expand());
      buttons->Realize();
      SetSizer(vbox);
      Layout();
    }
  };

public:
  EditFilterFrame(wxWindow* parent, const std::string& name,
                  std::function<void(const std::string&)> on_update)
      : wxFrame(parent, wxID_ANY,
                [name] { return _("Edit filter: ") + name; }()),
        m_on_update(on_update),
        m_name(name) {
    SetName(std::string("EditFilterFrame::") + name);
    m_filter = filters_on_disk::Read(m_name);

    auto flags = wxSizerFlags().Border().Expand();
    auto vbox = new wxBoxSizer(wxVERTICAL);
    SetSizer(vbox);
    vbox->Add(new DescriptionPanel(this, m_filter, [&] { Update(); }), flags);
    vbox->Add(new InterfacePanel(this, m_filter, [&] { Update(); }), flags);
    vbox->Add(new Buttons(this), flags);
    Layout();
    Hide();

    Bind(wxEVT_CLOSE_WINDOW, [&](wxCloseEvent&) { Destroy(); });
    Bind(wxEVT_BUTTON, [&](wxCommandEvent& evt) { OnButtonEvent(evt); });
  }

private:
  void Update() {
    filters_on_disk::Write(m_filter, m_name);
    m_on_update(m_name);
  }

  void OnButtonEvent(wxCommandEvent& evt) {
    if (evt.GetId() == wxID_CLOSE) {
      Destroy();
      evt.Skip();
    }
  }

  std::function<void(const std::string&)> m_on_update;
  std::string m_name;
  NavmsgFilter m_filter;
};

void CreateFilterDlg(wxWindow* parent) {
  NewFilterDlg dlg(parent);
  dlg.ShowModal();
  auto name = dlg.GetValue().ToStdString();
  if (filters_on_disk::Exists(name)) {
    BadFilterNameDlg dlg(wxTheApp->GetTopWindow());
    dlg.ShowModal();
  } else {
    NavmsgFilter filter;
    filter.m_name = name;
    filters_on_disk::Write(filter, name);
    FilterEvents::GetInstance().filter_list_change.Notify();
  }
}

void RemoveFilterDlg(wxWindow* parent) {
  if (GetUserFilters().empty()) {
    wxMessageDialog dlg(wxTheApp->GetTopWindow(), _("No filters available"));
    dlg.ShowModal();
    return;
  }
  DeleteFilterDlg dlg(parent);
  int sts = dlg.ShowModal();
  if (sts != wxID_OK) return;

  fs::path path(dlg.GetStringSelection().ToStdString());
  if (filters_on_disk::Remove(path.stem())) {
    FilterEvents::GetInstance().filter_list_change.Notify();
    wxMessageDialog dlg(wxTheApp->GetTopWindow(), _("Filter removed"));
  } else {
    wxMessageDialog dlg(wxTheApp->GetTopWindow(), _("Cannot remove filter"));
  }
}

void EditFilterDlg(wxWindow* parent) {
  SelectFilterDlg dlg(parent);
  int sts = dlg.ShowModal();
  if (sts != wxID_OK) return;
  auto name = dlg.GetStringSelection().ToStdString();
  auto window_name = std::string("EditFilterFrame::") + name;
  wxWindow* frame = wxWindow::FindWindowByName(window_name);
  auto on_update = [](const std::string& name) {
    FilterEvents::GetInstance().filter_list_change.Notify();
  };
  if (frame) {
    frame->Raise();
  } else {
    new EditFilterFrame(parent, name, [](const std::string& _name) {
      FilterEvents::GetInstance().filter_update.Notify(_name);
    });
    frame = wxWindow::FindWindowByName(window_name);
  }
  assert(frame && "Cannot create EditFilter frame");
  frame->Show();
}
