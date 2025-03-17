/***************************************************************************
 *   Copyright (C) 2025 Alec Leamas                                        *
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
 * Dialogs handing user defined filters.
 */

#ifndef FILTER_DLG_H__
#define FILTER_DLG_H__

#include <functional>

#include <wx/window.h>

void CreateFilterDlg(wxWindow* parent);

void RemoveFilterDlg(wxWindow* parent);

void EditFilterDlg(wxWindow* parent,
                   std::function<void(const std::string&)> on_update);

#endif  // FILTER_DLG_H__
