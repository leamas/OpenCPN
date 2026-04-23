/***************************************************************************
 *   Copyright (C) 2022 by David Register, Alec Leamas                     *
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
 **************************************************************************/

/**
 * \file
 *
 * Nmea2000 driver
 */

#ifndef COMMDRIVERN2K_H__
#define COMMDRIVERN2K_H__

#include <memory>

#include "model/comm_driver.h"

class CommDriverN2k : public AbstractCommDriver {
public:
  CommDriverN2k(const std::string& s = "n2k0");

  virtual ~CommDriverN2k();

  virtual bool SendMessage(std::shared_ptr<const NavMsg> msg,
                           std::shared_ptr<const NavAddr> addr) override = 0;
  virtual void SetListener(DriverListener& l) override;
  virtual std::shared_ptr<NavAddr2000> GetAddress(const N2kName& name);
  virtual int SetTxPgn(int pgn) { return 0; }
};

#endif  // guard
