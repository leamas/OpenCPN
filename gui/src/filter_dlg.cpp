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
#include <wx/log.h>
#include <wx/choicdlg.h>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>

#include "model/filters_on_disk.h"
#include "filter_dlg.h"

static wxArrayString GetUserFilters() {
  auto std_filters = filters_on_disk::List(false);
  wxArrayString wx_filters;
  for (auto& f : std_filters) wx_filters.Add(f);
  return wx_filters;
}

const char* const kFilterExists =
    _(R"( Filter already exists. Please use Edit to update it
or Remove to delete it prior to create.)");

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

class BadFilterNameDlg : public wxMessageDialog {
public:
  BadFilterNameDlg(wxWindow* parent) : wxMessageDialog(parent, kFilterExists) {}
};

void CreateFilterDlg(wxWindow* parent) {
  NewFilterDlg dlg(parent);
  dlg.ShowModal();
  auto name = dlg.GetValue().ToStdString();
  if (filters_on_disk::Exists(name)) {
    BadFilterNameDlg dlg(0);
    dlg.ShowModal();
  } else {
    NavmsgFilter filter;
    filter.m_name = name;
    filters_on_disk::Write(filter, name);
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
    wxMessageDialog dlg(wxTheApp->GetTopWindow(), _("Filter removed"));
  } else {
    wxMessageDialog dlg(wxTheApp->GetTopWindow(), _("Cannot remove filter"));
  }
}

void EditFilterDlg(wxWindow* parent) {}
