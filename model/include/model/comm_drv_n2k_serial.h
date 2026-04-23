/***************************************************************************
 *   Copyright (C) 2022 by David Register                                  *
 *   Copyright (C) 2022 Alec Leamas                                        *
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
 * Nmea2000 serial driver
 */

#ifndef _COMMDRIVERN2KSERIAL_H
#define _COMMDRIVERN2KSERIAL_H

#include <atomic>

#include <wx/thread.h>

#include "config.h"
#include "model/comm_drv_n2k.h"
#include "model/conn_params.h"
#include "model/comm_drv_stats.h"

#ifndef __ANDROID__
#include "serial/serial.h"
#endif

#define OUT_QUEUE_LENGTH 20
#define MAX_OUT_QUEUE_MESSAGE_LENGTH 200

#define ESCAPE 0x10
#define STARTOFTEXT 0x02
#define ENDOFTEXT 0x03

#define MsgTypeN2kData 0x93
#define MsgTypeN2kRequest 0x94

using namespace std::literals::chrono_literals;

class CommDriverN2KSerialThread;  // forward
class CommDriverN2KSerialEvent;   // forward in .cpp file

class CommDriverN2kSerial : public CommDriverN2k,
                            public wxEvtHandler,
                            public DriverStatsProvider {
public:
  CommDriverN2kSerial();
  CommDriverN2kSerial(const ConnectionParams* params, DriverListener& listener);

  virtual ~CommDriverN2kSerial();

  void SetListener(DriverListener& l) override {};

  bool Open();
  void Close();

  bool SendMessage(std::shared_ptr<const NavMsg> msg,
                   std::shared_ptr<const NavAddr> addr) override;

  void AddTxPgn(int pgn);

  int SetTxPgn(int pgn_arg) override;

  //    Secondary thread life toggle
  //    Used to inform launching object (this) to determine if the thread can
  //    be safely called or polled, e.g. wxThread->Destroy();
  void SetSecThreadActive() { m_bsec_thread_active = true; }
  void SetSecThreadInActive() { m_bsec_thread_active = false; }
  bool IsSecThreadActive() const { return m_bsec_thread_active; }

  void SetSecondaryThread(CommDriverN2KSerialThread* secondary_Thread) {
    m_secondary_thread = secondary_Thread;
  }
  CommDriverN2KSerialThread* GetSecondaryThread() { return m_secondary_thread; }
  void SetThreadRunFlag(int run) { m_thread_run_flag = run; }

  void HandleN2kSerialRaw(CommDriverN2KSerialEvent& event);
  int GetMfgCode();

  DriverStats GetDriverStats() const override;

  std::atomic_int m_thread_run_flag;
  ConnectionParams m_params;

private:
  void ProcessManagementPacket(std::vector<unsigned char>* payload);
  /**
   * Sends a management message over NMEA 2000 serial interface.
   *
   * @note This implementation is excluded on Android platforms
   */
  int SendMgmtMsg(unsigned char* string, size_t string_size,
                  unsigned char cmd_code, int timeout_msec,
                  bool* response_flag);

  bool m_bok;
  std::string m_portstring;
  std::string m_baudrate;
  int m_handshake;

  CommDriverN2KSerialThread* m_secondary_thread;
  bool m_bsec_thread_active;

  DriverListener& m_listener;

  bool m_is_mg47_resp;
  bool m_is_mg01_resp;
  bool m_is_mg4B_resp;
  bool m_is_mg41_resp;
  bool m_is_mg42_resp;

  std::string m_device_common_name;
  uint64_t name;
  int m_manufacturers_code;
  bool m_got_mfg_code;
  StatsTimer m_stats_timer;
  DriverStats m_driver_stats;
  std::vector<int> pgn_tx_list;
  bool m_closing;
};

#endif  // guard
