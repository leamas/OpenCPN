/***************************************************************************
 *   Copyright (C) 2023 by David Register                                  *
 *   Copyright (C) 2023 Alec Leamas                                        *
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
 * Nmea2000 IP network driver
 */

#ifndef _COMMDRIVERN2KNET_H
#define _COMMDRIVERN2KNET_H

#ifndef __WXMSW__
#include <sys/socket.h>  // needed for (some) Mac builds
#include <netinet/in.h>
#endif

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/datetime.h>

#include "model/comm_buffers.h"
#include "model/comm_can_util.h"
#include "model/comm_drv_n2k.h"
#include "model/comm_drv_n2k_net.h"
#include "model/comm_drv_stats.h"
#include "model/conn_params.h"

#ifdef __WXGTK__
// newer versions of glib define its own GSocket but we unfortunately use this
// name in our own (semi-)public header and so can't change it -- rename glib
// one instead
#define GSocket GlibGSocket
#include <wx/socket.h>
#undef GSocket
#else
#include <wx/socket.h>
#endif

#define RX_BUFFER_SIZE_NET 4096

#define ESCAPE 0x10
#define STARTOFTEXT 0x02
#define ENDOFTEXT 0x03

#define MsgTypeN2kData 0x93
#define MsgTypeN2kRequest 0x94

typedef enum {
  N2kFormatUndefined = 0,
  N2KFormatYdRaw,
  N2KFormatActisenseRawAscii,
  N2KFormatActisenseN2kAscii,
  N2kFormatActisenseN2k,
  N2kFormatActisenseRaw,
  N2kFormatActisenseNgt,
  N2kFormatSeaSmart,
  N2kFormatMiniPlex
} N2kFormat;

typedef enum { TX_FORMAT_YDEN = 0, TX_FORMAT_ACTISENSE } GW_TX_FORMAT;

class MrqContainer;           // forward in .cpp file
class CommDriverN2KNetEvent;  // Internal

class CommDriverN2KNet : public CommDriverN2k,
                         public wxEvtHandler,
                         public DriverStatsProvider {
public:
  CommDriverN2KNet();
  CommDriverN2KNet(const ConnectionParams* params, DriverListener& listener);

  virtual ~CommDriverN2KNet();

  DriverStats GetDriverStats() const override { return m_driver_stats; }

  void SetListener(DriverListener& l) override {};

  ConnectionParams GetParams() const { return m_params; }

  bool SendMessage(std::shared_ptr<const NavMsg> msg,
                   std::shared_ptr<const NavAddr> addr) override;

private:
  ConnectionParams m_params;
  DriverListener& m_listener;
  StatsTimer m_stats_timer;
  DriverStats m_driver_stats;

  wxString m_net_port;
  NetworkProtocol m_net_protocol;
  wxIPV4address m_addr;
  wxSocketBase* m_sock;
  wxSocketBase* m_tsock;
  wxSocketServer* m_socket_server;
  bool m_is_multicast;
  MrqContainer* m_mrq_container;

  int m_txenter;
  int m_dog_value;
  std::string m_sock_buffer;
  wxString m_portstring;
  dsPortType m_io_select;
  wxDateTime m_connect_time;
  bool m_brx_connect_event;
  bool m_bchecksumCheck;
  ConnectionType m_connection_type;

  wxTimer m_socket_timer;
  wxTimer m_socketread_watchdog_timer;

  bool m_is_ok;
  int m_ib;
  bool m_is_in_msg, m_got_esc, m_got_sot;

  CircularBuffer<unsigned char> m_circle;
  unsigned char* rx_buffer;
  std::string m_sentence;

  FastMessageMap* fast_messages;
  N2kFormat m_n2k_format;
  uint8_t m_order;
  char m_tx_flag;
  bool m_tx_available;
  wxTimer m_prodinfo_timer;

  ObsListener resume_listener;

  bool PrepareForTx();
  std::vector<unsigned char> PrepareLogPayload(
      std::shared_ptr<const Nmea2000Msg>& msg,
      std::shared_ptr<const NavAddr2000> addr);
  void OnProdInfoTimer(wxTimerEvent& ev);
  std::vector<std::vector<unsigned char>> GetTxVector(
      const std::shared_ptr<const Nmea2000Msg>& msg,
      std::shared_ptr<const NavAddr2000> dest_addr);
  bool SendSentenceNetwork(
      const std::vector<std::vector<unsigned char>>& payload);
  bool HandleMgntMsg(uint64_t pgn, std::vector<unsigned char>& payload);
  N2kFormat DetectFormat(const std::vector<unsigned char>& packet);
  bool ProcessActisenseAsciiRaw(const std::vector<unsigned char>& packet);
  bool ProcessActisenseAsciiN2k(const std::vector<unsigned char>& packet);
  bool ProcessActisenseN2k(const std::vector<unsigned char>& packet);
  bool ProcessActisenseRaw(const std::vector<unsigned char>& packet);
  bool ProcessActisenseNgt(const std::vector<unsigned char>& packet);
  bool ProcessSeaSmart(const std::vector<unsigned char>& packet);
  bool ProcessMiniPlex(const std::vector<unsigned char>& packet);

  bool SendN2KNetwork(std::shared_ptr<const Nmea2000Msg>& msg,
                      std::shared_ptr<const NavAddr2000> dest_addr);

  wxSocketBase* GetSock() const { return m_sock; }
  NetworkProtocol GetProtocol() { return m_net_protocol; }
  void SetBrxConnectEvent(bool event) { m_brx_connect_event = event; }
  bool GetBrxConnectEvent() { return m_brx_connect_event; }

  void SetConnectTime(wxDateTime time) { m_connect_time = time; }
  wxDateTime GetConnectTime() { return m_connect_time; }

  dsPortType GetPortType() const { return m_io_select; }
  wxString GetPort() const { return m_portstring; }

  std::vector<unsigned char> PushFastMsgFragment(const CanHeader& header,
                                                 int position);
  std::vector<unsigned char> PushCompleteMsg(const CanHeader header,
                                             int position,
                                             const can_frame frame);

  void HandleCanFrameInput(can_frame frame);
  bool SetOutputSocketOptions(wxSocketBase* tsock);
  void OnServerSocketEvent(wxSocketEvent& event);  // The listener
  void OnTimerSocket(wxTimerEvent& event) { OnTimerSocket(); }
  void OnTimerSocket();
  void OnSocketEvent(wxSocketEvent& event);
  void OpenNetworkGpsd();
  void OpenNetworkTcp(unsigned int addr);
  void OpenNetworkUdp(unsigned int addr);
  void OnSocketReadWatchdogTimer(wxTimerEvent& event);
  void HandleResume();
  wxTimer* GetSocketTimer() { return &m_socket_timer; }
  void SetSock(wxSocketBase* sock) { m_sock = sock; }
  void SetTSock(wxSocketBase* sock) { m_tsock = sock; }
  wxSocketBase* GetTSock() const { return m_tsock; }
  void SetSockServer(wxSocketServer* sock) { m_socket_server = sock; }
  wxSocketServer* GetSockServer() const { return m_socket_server; }
  void SetMulticast(bool multicast) { m_is_multicast = multicast; }
  bool GetMulticast() const { return m_is_multicast; }
  ConnectionType GetConnectionType() const { return m_connection_type; }

  void Open();
  void Close();
  bool IsChecksumOk(const std::string& sentence);
  void SetOk(bool ok) { m_is_ok = ok; };
  void HandleN2kMsg(CommDriverN2KNetEvent& event);
  wxString GetNetPort() const { return m_net_port; }
  wxIPV4address GetAddr() const { return m_addr; }
  wxTimer* GetSocketThreadWatchdogTimer() {
    return &m_socketread_watchdog_timer;
  }

  DECLARE_EVENT_TABLE()
};

#endif  // guard
