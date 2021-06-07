/******************************************************************************
 *
 * Project:  OpenCPN
 *
 ***************************************************************************
 *   Copyright (C) 2021 Alec Leamas                                        *
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
 ***************************************************************************
 */


#include "config.h"


#include "udev_rule_mgr.h"

#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

extern bool g_hide_udev_dongle_dialog;
extern bool g_hide_udev_device_dialog;

DongleRuleDialog::DongleRuleDialog(wxWindow* parent)
    :wxDialog(parent, wxID_ANY, _("Manage dongle udev rule"),
              wxDefaultPosition , wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP)
{
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, "Orvar"));
    SetSizer(sizer);
    Fit();
    Show();
}


DeviceRuleDialog::DeviceRuleDialog(wxWindow* parent, const char* device_path)
    :wxDialog(parent, wxID_ANY, _("Manage dongle udev rule"),
              wxDefaultPosition , wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP)
{}

static const char* const
    HIDE_DIALOG_LABEL = _("Do not show this dialog next time");

class DongleHideCheckbox: public wxCheckBox
{
    public:

        DongleHideCheckbox(wxWindow* parent)
            : wxCheckBox(parent, wxID_ANY, HIDE_DIALOG_LABEL,
                         wxDefaultPosition,wxDefaultSize, wxALIGN_RIGHT)
        {
            SetValue(g_hide_udev_dongle_dialog);
            Bind(wxEVT_CHECKBOX,
                 [](wxCommandEvent& ev) {
                     g_hide_udev_dongle_dialog = ev.IsChecked();
                 }
            );
        }
};
