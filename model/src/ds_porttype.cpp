/***************************************************************************
 *   Copyright (C) 2014  ALec Leamas                                       *
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
 *   along with this program; if not, see <https://www.gnu.org/licenses/>. *
 ***************************************************************************/

/**
 * \file
 *
 * Implement ds_porttype.h -- port type definition and support
 */

#include <cassert>

#include "model/ds_porttype.h"

#include "model/conn_params.h"

static std::string PortDirectionToString(PortDirection pd) {
  switch (static_cast<int>(pd)) {
    case static_cast<int>(PortDirection::kInOut):
      return "IN/OUT";
      break;
    case static_cast<int>(PortDirection::kOutput):
      return "OUT";
      break;
    case static_cast<int>(PortDirection::kInput):
      return "IN";
      break;
    default:
      assert(false);
  };
  assert(false && "Compiler error (undefined dsPortType)");
  return "";  // for the compiler
}
