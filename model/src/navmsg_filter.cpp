
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
 * Implement monitor_filter.h
 */

#include "model/navmsg_filter.h"
NavmsgFilter::NavmsgFilter() {}

bool NavmsgFilter::Pass(NavmsgStatus msg_status,
                        const std::shared_ptr<NavMsg>& msg) {
  if (directions.size() > 0) {
    if (directions.find(msg_status.direction) == directions.end()) return false;
  }
  if (status.size() > 0) {
    if (status.find(msg_status.status) == status.end()) return false;
  }
  if (buses.size() > 0) {
    if (buses.find(msg->bus) == buses.end()) return false;
  }
  if (include_msg.size() > 0) {
    if (include_msg.find(msg->key()) == include_msg.end()) return false;
  }
  if (exclude_msg.size() > 0) {
    if (exclude_msg.find(msg->key()) != exclude_msg.end()) return false;
  }
  if (interfaces.size() > 0) {
    if (interfaces.find(msg->source->iface) != interfaces.end()) return false;
  }
  auto n2k_msg = std::dynamic_pointer_cast<Nmea2000Msg>(msg);
  if (n2k_msg) {
    if (pgns.size() > 0) {
      if (pgns.find(n2k_msg->PGN) == pgns.end()) return false;
    }
    if (src_pgns.size() > 0) {
      auto src = std::static_pointer_cast<const NavAddr2000>(msg->source);
      if (src && src_pgns.find(src->name) == src_pgns.end()) return false;
    }
  }

  return true;
}
