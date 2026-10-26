/**************************************************************************
 *   Copyright (C) 2022 by David S. Register                               *
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
 * Implement connection_edit.h -- Dialog and support for editing a connection
 */

#include <memory>
#include <set>
#include <string>
#include <vector>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "config.h"

#include <wx/tokenzr.h>
#include <wx/regex.h>

#if defined(__linux__) && !defined(__ANDROID__)
#include <linux/can.h>
#include <net/if.h>
#include <serial/serial.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "dnet.h"
#endif

#ifdef __ANDROID__
#include "androidUTIL.h"
#include "qdebug.h"
#endif

// #include "connection_edit.h"
#include "conn_edit.h"

#include "libgui/text_ctrl_w_help.h"
#include "libgui/expand_panel.h"

#include "model/comm_drv_factory.h"
#include "model/config_vars.h"
#include "model/conn_params.h"
#include "model/ocpn_utils.h"
#include "model/ser_ports.h"
#include "model/sys_events.h"

#include "conn_params_panel.h"
#include "gui_lib.h"
#include "nmea0183.h"
#include "ocpn_platform.h"
#include "ocpn_plugin.h"  // FIXME for GetOCPNScaledFont_PlugIn
#include "options.h"
#include "priority_gui.h"
#include "udev_rule_mgr.h"

// Make _() return std::string instead of wxString;
#undef _
#if wxCHECK_VERSION(3, 2, 0)
#define _(s) wxGetTranslation(wxASCII_STR(s)).ToStdString()
#else
#define _(s) wxGetTranslation((s)).ToStdString()
#endif

static const std::string kAddressDefaultHelp =
    _("Enter IP address or hostname");
static const std::string kAddressUdpHelp =
    _("IP address or hostname, often 255.255.255.255");
static const std::string kAddressMcastHelp =
    _("Group address, usually 224.0.2.0 - 224.0.255.255");
static const std::string kTcpDevice = _("Device using TCP");
static const std::string kUdpReceive = _("Device sending UDP");
static const std::string kUdpOutput = _("UDP send");
static const std::string kGpsdDevice = _("Gpsd server");
static const std::string kSignalkDevice = _("SignalK server");
static const std::string kTcpClient = _("TCP client");
static const std::string kUdpSend = _("Device(s) receiving UDP");
static const std::string kGpsdClient = _("Gpsd client");
static const std::string kSignalkClient = _("SignalK client");
static const std::string kTcpServer = _("TCP Server");
static const std::string kUdpInput = _("UDP Receive");
static const std::string kMulticastServer = _("UDP Multicast Receive and Send");
static const std::string kMulticastClient = _("UDP Multicast Send");
static const std::string kDefaultTcpPort = "10110";
static const std::string kDefaultUdpPort = "10110";
static const std::string kDefaultGpsdPort = "2947";
static const std::string kDefaultSignalkPort = "3000";
static const std::string kDefaultIpAddress = "localhost";
static const std::string kDefaultMulticastAddr = "224.0.2.21";
// "LIMITED BROADCAST" address
//  Deprecated, but still useful on simple mobile networks.
static const wxString DEFAULT_UDP_OUT_ADDRESS = "255.255.255.255";

static const std::vector<std::string> kBasicNetViews = {
    kTcpDevice, kUdpReceive, kUdpSend, kGpsdDevice, kSignalkDevice};

static const std::vector<std::string> kAdvancedNetViews = {
    // First items matches kBasicNetViews
    kTcpClient,     kUdpInput,  kUdpOutput,       kGpsdClient,
    kSignalkClient, kTcpServer, kMulticastClient, kMulticastServer};

#ifdef __ANDROID__
static bool IsAndroid() { return true; }
#else
static bool IsAndroid() { return false; }
#endif

static bool IsWindows() {
  return wxPlatformInfo::Get().GetOperatingSystemId() & wxOS_WINDOWS;
}

static bool IsApple() {
  return wxPlatformInfo::Get().GetOperatingSystemId() & wxOS_MAC_OS;
}

static bool IsLinux() {
  return wxPlatformInfo::Get().GetOperatingSystemId() & wxOS_UNIX_LINUX;
}

static wxString StringArrayToString(const wxArrayString& arr) {
  wxString ret = wxEmptyString;
  for (size_t i = 0; i < arr.Count(); i++) {
    if (i > 0) ret.Append(",");
    ret.Append(arr[i]);
  }
  return ret;
}

// Check available SocketCAN interfaces
#if defined(__linux__) && !defined(__ANDROID__)
static intf_t* intf;
std::vector<std::string> can_if_candidates;
static int print_intf(const struct intf_entry* entry, void* arg) {
  std::string iface = entry->intf_name;
  if (entry->intf_type == 1 && iface.find("can") != std::string::npos) {
    can_if_candidates.emplace_back(entry->intf_name);
  }
  return 0;
}
#endif

static bool IsAddressMultiCast(const wxString& ip) {
  wxArrayString bytes = wxSplit(ip, '.');
  if (bytes.size() != 4) {
    return false;
  }
  unsigned long ipNum = (wxAtoi(bytes[0]) << 24) + (wxAtoi(bytes[1]) << 16) +
                        (wxAtoi(bytes[2]) << 8) + wxAtoi(bytes[3]);
  unsigned long multicastStart = (224 << 24);
  unsigned long multicastEnd = (239 << 24) + (255 << 16) + (255 << 8) + 255;
  return ipNum >= multicastStart && ipNum <= multicastEnd;
}

static bool IsAddressListener(const std::string& address) {
  return address.empty() || address == "0.0.0.0";
}

/** Return  selected option from a wxChoice widget */
static std::string GetChoiceSelection(const wxChoice* choice) {
  int selected = choice->GetSelection();
  return choice->GetString(selected).ToStdString();
}

/**
 * Return net view name for given ConnectionParams. For ambigious cases
 * like kTcpDevice (basic view)/kTcpClient (Advanced view) return
 * the basic view.
 */
static std::string NetViewByConnection(const ConnectionParams* cp) {
  bool is_server = IsAddressListener(cp->network_address.ToStdString());
  if (IsAddressMultiCast(cp->network_address))
    return is_server ? kMulticastServer : kMulticastClient;
  switch (cp->net_protocol) {
    case NetworkProtocol::GPSD:
      return kGpsdDevice;
    case NetworkProtocol::SIGNALK:
      return kSignalkDevice;
    case NetworkProtocol::UDP:
      return is_server ? kUdpReceive : kUdpSend;
    case NetworkProtocol::TCP:
      return is_server ? kTcpServer : kTcpDevice;
    default:
      wxLogWarning("Cannot deduce connection params view type");
      return "";
  }
  return "";  // for the compiler
}

static wxArrayString GetAvailableSocketCANInterfaces() {
  wxArrayString rv;

#if defined(__linux__) && !defined(__ANDROID__)
  can_if_candidates.clear();

  if ((intf = intf_open()) == nullptr) {
    wxLogWarning("Error opening interface list");
    return rv;
  }

  if (intf_loop(intf, print_intf, nullptr) < 0) {
    wxLogWarning("Error looping over interface list");
  }
  intf_close(intf);

  for (const auto& iface : can_if_candidates) {
    int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
      continue;
    }

    // Get the interface index
    struct ifreq if_request = {{0}};
    strcpy(if_request.ifr_name, iface.c_str());
    if (ioctl(sock, SIOCGIFINDEX, &if_request) < 0) {
      continue;
    }

    // Check if interface is UP
    struct sockaddr_can can_address = {0};
    can_address.can_family = AF_CAN;
    can_address.can_ifindex = if_request.ifr_ifindex;
    if (ioctl(sock, SIOCGIFFLAGS, &if_request) < 0) {
      continue;
    }
    if (if_request.ifr_flags & IFF_UP) {
      rv.Add(iface);
    } else {
      continue;
    }
  }
#endif
  return rv;
}

static void LoadSerialPorts(wxComboBox* box) {
  /** Sort all links to same device as equals. */
  class PortSorter {
  private:
    [[nodiscard]] static std::string GetKey(const std::string& s) {
      if (s.find("->") == std::string::npos) return s;
      return ocpn::trim(ocpn::split(s, "->")[1]) + " link";
    }

  public:
    bool operator()(const std::string& lhs, const std::string& rhs) const {
      return GetKey(lhs) < GetKey(rhs);
    }
  } port_sorter;

  std::set<std::string, PortSorter> sorted_ports(port_sorter);
  std::unique_ptr<wxArrayString> ports(EnumerateSerialPorts());
  for (size_t i = 0; i < ports->GetCount(); i++)
    sorted_ports.insert((*ports)[i].ToStdString());

  auto value = box->GetValue();
  box->Clear();
  for (auto& p : sorted_ports) box->Append(p);
  if (!value.empty()) box->SetValue(value);
}

static bool CheckPort(wxWindow* parent, TextCtrlWithHelp& ctrl) {
  if (ctrl.IsPristine() || ctrl.GetValue().empty()) {
    auto dlg = wxMessageDialog(parent, _("Required field port is missing"),
                               _("OpenCPN error"), wxOK | wxICON_ERROR);
    dlg.ShowModal();
    return false;
  };
  int port = 0;
  try {
    port = std::stoi(ctrl.GetValue().ToStdString());
  } catch (std::logic_error&) {
    auto dlg = wxMessageDialog(parent, _("Invalid port number"),
                               _("OpenCPN error"), wxOK | wxICON_ERROR);
    dlg.ShowModal();
    return false;
  }
  if (port < 1024) {
    static const std::string kMsg =
        _(R"(Port numbers smaller than 1024 are reserved for use by the
operating system and should normally not be used by OpenCPN)");
    auto dlg = wxMessageDialog(parent, kMsg, _("OpenCPN warning"),
                               wxOK | wxICON_WARNING);
    dlg.ShowModal();
    return true;
  }
  return true;
}

bool CheckAddress(wxWindow* parent, TextCtrlWithHelp& ctrl) {
  if (ctrl.IsPristine() || ctrl.GetValue().empty()) {
    auto dlg = wxMessageDialog(parent, _("Required field address is missing"),
                               _("OpenCPN error"), wxOK | wxICON_ERROR);
    dlg.ShowModal();
    return false;
  };
  // Checking the address requires using gethostbyname() or so since it
  // could be a hostname. Not worthwhile in this context.
  return true;
}

bool CheckIoDirections(wxWindow* parent, const wxCheckBox* input,
                       const wxCheckBox* output) {
  if (input->GetValue() || output->GetValue()) return true;

  auto dlg =
      wxMessageDialog(parent, _("Either input or output must be checked"),
                      _("OpenCPN error"), wxOK | wxICON_ERROR);
  dlg.ShowModal();
  return false;
}

/** Initiate the nmea protocol 0183/2000 choide */
static void SetupProtocolChoice(wxChoice* choice) {
  choice->Clear();
  choice->Append("NMEA 0183");
  choice->Append("NMEA 2000");
  choice->SetSelection(0);
  choice->Enable();
}

#ifdef __ANDROID__
if (m_type_internal_gps_radiobtn) {
  m_type_internal_gps_radiobtn->SetValue(true);
  SetNMEAFormToGPS();
} else {
  m_type_net_radiobtn->SetValue(true);
  SetNMEAFormToNet();
}
#elif defined(ORVAR)
m_type_serial_radiobtn->SetValue(bserial);
m_type_net_radiobtn->SetValue(!bserial);
bserial ? SetNMEAFormToSerial() : SetNMEAFormToNet();
m_type_can_radiobtn->SetValue(false);
m_type_canable_radiobtn->SetValue(false);
#endif

SentenceListDlg::SentenceListDlg(wxWindow* parent, FilterDirection dir,
                                 ListType type, const wxArrayString& list)
    : wxDialog(parent, wxID_ANY, _("Sentence Filter"), wxDefaultPosition,
               wxSize(280, 420)),
      m_type(type),
      m_dir(dir),
      m_sentences(NMEA0183().GetRecognizedArray()) {
  m_sentences.Sort();
  auto* mainSizer = new wxBoxSizer(wxVERTICAL);
  auto* secondSizer = new wxBoxSizer(wxHORIZONTAL);
  auto* pclbBox = new wxStaticBox(this, wxID_ANY, GetBoxLabel());
  auto* stcSizer = new wxStaticBoxSizer(pclbBox, wxVERTICAL);
  m_sentences_clb = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition,
                                       wxDefaultSize, m_sentences);
  auto* btnEntrySizer = new wxBoxSizer(wxVERTICAL);
  auto* btnCheckAll = new wxButton(this, wxID_ANY, _("Select All"));
  auto* btnClearAll = new wxButton(this, wxID_ANY, _("Clear All"));
  auto* btnAdd = new wxButton(this, wxID_ANY, _("Add"));
  m_del_btn = new wxButton(this, wxID_ANY, _("Delete"));
  m_del_btn->Disable();
  auto* btnSizer = new wxStdDialogButtonSizer();
  auto* btnOK = new wxButton(this, wxID_OK);
  auto* btnCancel = new wxButton(this, wxID_CANCEL, _("Cancel"));

  secondSizer->Add(stcSizer, 1, wxALL | wxEXPAND, 5);
  stcSizer->Add(m_sentences_clb, 1, wxALL | wxEXPAND, 5);
  btnEntrySizer->Add(btnCheckAll, 0, wxALL, 5);
  btnEntrySizer->Add(btnClearAll, 0, wxALL, 5);
  btnEntrySizer->AddSpacer(1);
  btnEntrySizer->Add(btnAdd, 0, wxALL, 5);
  btnEntrySizer->Add(m_del_btn, 0, wxALL, 5);
  secondSizer->Add(btnEntrySizer, 0, wxALL | wxEXPAND, 5);
  mainSizer->Add(secondSizer, 1, wxEXPAND, 5);
  btnSizer->AddButton(btnOK);
  btnSizer->AddButton(btnCancel);
  btnSizer->Realize();
  mainSizer->Add(btnSizer, 0, wxALL | wxEXPAND, 5);

  SetSizer(mainSizer);
  mainSizer->SetSizeHints(this);
  Centre();

  btnAdd->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
                  wxCommandEventHandler(SentenceListDlg::OnAddClick), nullptr,
                  this);
  m_del_btn->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
                     wxCommandEventHandler(SentenceListDlg::OnDeleteClick),
                     nullptr, this);
  m_sentences_clb->Connect(wxEVT_COMMAND_LISTBOX_SELECTED,
                           wxCommandEventHandler(SentenceListDlg::OnCLBSelect),
                           nullptr, this);
  btnCheckAll->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
                       wxCommandEventHandler(SentenceListDlg::OnCheckAllClick),
                       nullptr, this);
  btnClearAll->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
                       wxCommandEventHandler(SentenceListDlg::OnClearAllClick),
                       nullptr, this);
  Populate(list);
}

wxString SentenceListDlg::GetBoxLabel() const {
  if (m_dir == FILTER_OUTPUT)
    return m_type == WHITELIST ? _("Transmit sentences") : _("Drop sentences");
  else
    return m_type == WHITELIST ? _("Accept only sentences")
                               : _("Ignore sentences");
}

void SentenceListDlg::Populate(const wxArrayString& list) {
  if (m_dir == FILTER_OUTPUT) {
    wxString s;
    m_sentences.Add(g_TalkerIdText + wxString("RMB"));
    m_sentences.Add(g_TalkerIdText + wxString("RMC"));
    m_sentences.Add(g_TalkerIdText + wxString("APB"));
    m_sentences.Add(g_TalkerIdText + wxString("XTE"));
  }
  m_sentences.Add("AIVDM");
  m_sentences.Add("AIVDO");
  m_sentences.Add("FRPOS");
  m_sentences.Add(g_TalkerIdText);
  m_sentences.Add("CD");
  m_sentences.Sort();
  m_sentences_clb->Clear();
  m_sentences_clb->InsertItems(m_sentences, 0);

  wxArrayString new_strings;
  if (list.Count() == 0) {
    for (size_t i = 0; i < m_sentences_clb->GetCount(); ++i)
      m_sentences_clb->Check(i, m_type == WHITELIST);
  } else {
    for (size_t i = 0; i < list.Count(); ++i) {
      int item = m_sentences_clb->FindString(list[i]);
      if (item != wxNOT_FOUND)
        m_sentences_clb->Check(item);
      else
        new_strings.Add(list[i]);
    }
    if (new_strings.GetCount()) {
      m_sentences_clb->InsertItems(new_strings, m_sentences_clb->GetCount());
      for (size_t i = 0; i < new_strings.GetCount(); ++i) {
        int item = m_sentences_clb->FindString(new_strings[i]);
        if (item != wxNOT_FOUND) m_sentences_clb->Check(item);
      }
    }
  }
}

wxString SentenceListDlg::GetSentences() {
  wxArrayString retString;
  for (size_t i = 0; i < m_sentences_clb->GetCount(); i++) {
    if (m_sentences_clb->IsChecked(i))
      retString.Add(m_sentences_clb->GetString(i));
  }
  return StringArrayToString(retString);
}

void SentenceListDlg::OnCLBSelect(wxCommandEvent& e) {
  // Only activate the "Delete" button if the selection is not in the standard
  // list
  m_del_btn->Enable(m_sentences.Index(e.GetString()) == wxNOT_FOUND);
}

void SentenceListDlg::OnAddClick(wxCommandEvent& event) {
#ifdef __ANDROID__
  androidDisableRotation();
#endif

  wxTextEntryDialog textdlg(
      this,
      _("Enter the NMEA sentence (2, 3 or 5 characters)\n  or a valid REGEX "
        "expression (6 characters or longer)"),
      _("Enter the NMEA sentence"));

  textdlg.SetTextValidator(wxFILTER_ASCII);
  int result = textdlg.ShowModal();

#ifdef __ANDROID__
  androidEnableRotation();
#endif

  if (result == wxID_CANCEL) return;
  wxString stc = textdlg.GetValue();

  if (stc.Length() == 2 || stc.Length() == 3 || stc.Length() == 5) {
    m_sentences_clb->Append(stc);
    m_sentences_clb->Check(m_sentences_clb->FindString(stc));
    return;
  } else if (stc.Length() < 2) {
    OCPNMessageBox(
        this,
        _("An NMEA sentence is generally 3 characters long (like RMC, GGA etc.)\n \
            It can also have a two letter prefix identifying the source, or TALKER, of the message.\n \
            The whole sentences then looks like GPGGA or AITXT.\n \
            You may filter out all the sentences with certain TALKER prefix (like GP, AI etc.).\n \
            The filter also accepts Regular Expressions (REGEX) with 6 or more characters. \n\n"),
        _("OpenCPN Info"));
    return;
  }

  else {
    // Verify that a longer text entry is a valid RegEx
    wxRegEx r(stc);
    if (r.IsValid()) {
      m_sentences_clb->Append(stc);
      m_sentences_clb->Check(m_sentences_clb->FindString(stc));
      return;
    } else {
      OCPNMessageBox(this, _("REGEX syntax error: \n") + stc,
                     _("OpenCPN Info"));
      return;
    }
  }
}

void SentenceListDlg::OnDeleteClick(wxCommandEvent& event) {
  m_sentences_clb->Delete(m_sentences_clb->GetSelection());
}

void SentenceListDlg::OnClearAllClick(wxCommandEvent& event) {
  for (size_t i = 0; i < m_sentences_clb->GetCount(); i++)
    m_sentences_clb->Check(i, false);
}

void SentenceListDlg::OnCheckAllClick(wxCommandEvent& event) {
  for (size_t i = 0; i < m_sentences_clb->GetCount(); i++)
    m_sentences_clb->Check(i, true);
}

/*--------------------------------------*/
namespace {

/** Interface implemented by widgets supporting reload from parameter(s) */
class Reloadable {
public:
  virtual ~Reloadable() = default;

  /** Reload GUI value(s) from parameter(s) */
  virtual void Reload() = 0;
};

/**
 * The three radio buttons selecting serial, network or socketcan
 * connection type.
 */
class TypeButtons : public wxPanel, public Reloadable {
public:
  explicit TypeButtons(wxWindow* parent,
                       std::function<void(ConnectionType)> on_click,
                       ConnectionParams& params)
      : wxPanel(parent, wxID_ANY),
        m_serial_btn(new wxRadioButton(this, wxID_ANY, _("Serial"))),
        m_net_btn(new wxRadioButton(this, wxID_ANY, _("Network"))),
        m_can_btn(nullptr),
        m_on_click(std::move(on_click)),
        m_params(params) {
    auto hbox = new wxBoxSizer(wxHORIZONTAL);
    auto on_clicked = [&](wxCommandEvent& e) { OnClick(e); };
    m_serial_btn->Bind(wxEVT_RADIOBUTTON, on_clicked);
    hbox->Add(m_serial_btn, wxSizerFlags().Border());

    m_net_btn->Bind(wxEVT_RADIOBUTTON, on_clicked);
    hbox->Add(m_net_btn, wxSizerFlags().Border());

    if (IsLinux()) {
      m_can_btn = new wxRadioButton(this, wxID_ANY, _("socketCAN"));
      m_can_btn->Bind(wxEVT_RADIOBUTTON, on_clicked);
      hbox->Add(m_can_btn, wxSizerFlags().Border());
    }
    SetSelectedType(ConnectionType::SERIAL);

    SetSizer(hbox);
    hbox->Layout();
    wxWindow::Show();
  }

  void Reload() override { SetSelectedType(m_params.type); }

  void SetSelectedType(ConnectionType type) {
    switch (type) {
      case ConnectionType::NETWORK:
        m_net_btn->SetValue(true);
        break;
      case ConnectionType::SERIAL:
        m_serial_btn->SetValue(true);
        break;
      case ConnectionType::SOCKETCAN:
        m_can_btn->SetValue(true);
        break;
      default:
        assert(false && "Illegal connection type");
    }
  }

private:
  wxRadioButton* m_serial_btn;
  wxRadioButton* m_net_btn;
  wxRadioButton* m_can_btn;
  std::function<void(ConnectionType)> m_on_click;
  ConnectionParams& m_params;

  void OnClick(wxCommandEvent& e) {
    m_on_click(GetSelectedType());
    e.Skip();
  }

  ConnectionType GetSelectedType() {
    if (m_serial_btn->GetValue()) return ConnectionType::SERIAL;
    if (m_net_btn->GetValue()) return ConnectionType::NETWORK;
    if (m_can_btn && m_can_btn->GetValue()) return ConnectionType::SOCKETCAN;
    assert(false && "No connection type radiobutton active(!)");
    return ConnectionType::UNKNOWN;
  }
};

/** Configure input filters: accept or ignore list of sentences. */
class FilterConfigPanel : public wxPanel, public Reloadable {
  /** Container fir accept/ignore buttons and  filtered sentences. */
  class Statbox : public wxStaticBox, public Reloadable {
  public:
    Statbox(wxWindow* parent, ConnectionParams& params)
        : wxStaticBox(parent, wxID_ANY, _("Input filtering")),
          m_params(params) {
      auto radio_btn_box = new wxBoxSizer(wxHORIZONTAL);
      m_accept_btn =
          new wxRadioButton(this, wxID_ANY, _("Accept only sentences"));
      m_ignore_btn = new wxRadioButton(this, wxID_ANY, _("Ignore sentences"));
      radio_btn_box->Add(m_accept_btn, wxSizerFlags().Border());
      radio_btn_box->Add(m_ignore_btn, wxSizerFlags().Border());

      auto setup_box = new wxBoxSizer(wxHORIZONTAL);
      m_sentences_text = new wxTextCtrl(this, wxID_ANY, "");
      m_sentences_text->Disable();
      m_edit_btn = new wxButton(this, wxID_ANY, "...");
      m_edit_btn->Bind(wxEVT_BUTTON, [&](wxCommandEvent& e) { OnEdit(e); });
      m_edit_btn->SetMaxSize(wxSize(wxWindow::GetCharWidth() * 4, -1));
      setup_box->Add(m_sentences_text, wxSizerFlags(1).Border());
      setup_box->Add(m_edit_btn, wxSizerFlags().Border());

      auto vbox = new wxBoxSizer(wxVERTICAL);
      vbox->Add(radio_btn_box, wxSizerFlags().Border());
      vbox->Add(setup_box, wxSizerFlags(1).Border().Expand());
      SetSizer(vbox);
      // FIXME (leamas) - Why?
      wxWindowBase::SetMinSize(wxSize(-1, 7 * wxWindow::GetCharHeight()));
      wxWindow::Layout();

      Statbox::Reload();
      m_ignore_btn->Bind(wxEVT_RADIOBUTTON,
                         [&](wxCommandEvent& e) { OnDirectionChange(e); });
      m_accept_btn->Bind(wxEVT_RADIOBUTTON,
                         [&](wxCommandEvent& e) { OnDirectionChange(e); });
    }

    void Reload() override {
      bool accept = m_params.input_sentence_list_type == ListType::WHITELIST;
      m_accept_btn->SetValue(accept);
      bool ignore = m_params.input_sentence_list_type == ListType::BLACKLIST;
      m_ignore_btn->SetValue(ignore);
      m_sentences_text->SetValue(
          StringArrayToString(m_params.input_sentence_list));
    }

  private:
    ConnectionParams& m_params;
    wxRadioButton* m_accept_btn;
    wxRadioButton* m_ignore_btn;
    wxTextCtrl* m_sentences_text;
    wxButton* m_edit_btn;

    void OnDirectionChange(wxCommandEvent& e) {
      if (m_accept_btn->GetValue())
        m_params.input_sentence_list_type = ListType::WHITELIST;
      if (m_ignore_btn->GetValue())
        m_params.input_sentence_list_type = ListType::BLACKLIST;
      e.Skip();
    }

    void OnEdit(wxCommandEvent& e) {
      const ListType type = m_params.input_sentence_list_type;
      const wxArrayString list =
          wxStringTokenize(m_sentences_text->GetValue(), ",");
      SentenceListDlg dlg(m_parent, FILTER_INPUT, type, list);
      if (dlg.ShowModal() != wxID_OK) return;

      m_sentences_text->SetValue(dlg.GetSentences());
      m_params.input_sentence_list =
          wxStringTokenize(m_sentences_text->GetValue(), ",");
    }
  };  // Statbox

public:
  FilterConfigPanel(wxWindow* parent, ConnectionParams& params)
      : wxPanel(parent, wxID_ANY), m_params(params) {
    auto* vbox = new wxBoxSizer(wxVERTICAL);
    auto flags = wxSizerFlags().Border();
    auto* box = new Statbox(this, m_params);
    vbox->Add(box, flags);
    SetSizer(vbox);
    wxWindowBase::Layout();
  }

  void Reload() override {}

private:
  ConnectionParams& m_params;
};  // FilterConfigPanel

/** Two checkboxes for enabling port input and/or output. */
class InputOutputPanel : public wxPanel {
  class InputCheckBox : public wxCheckBox, public Reloadable {
  public:
    InputCheckBox(wxWindow* parent, ConnectionParams& params,
                  std::function<void()> on_change)
        : wxCheckBox(parent, wxID_ANY, _("Receive Input on this Port")),
          m_params(params),
          m_on_change(std::move(on_change)) {
      InputCheckBox::Reload();
      Bind(wxEVT_CHECKBOX, [&](wxCommandEvent& ev) {
        m_on_change();
        ev.Skip();
      });
    }

    void Reload() override {
      SetValue(m_params.direction != PortDirection::kOutput);
    }

  private:
    ConnectionParams& m_params;
    std::function<void()> m_on_change;
  };

  class OutputCheckBox : public wxCheckBox, public Reloadable {
  public:
    OutputCheckBox(wxWindow* parent, ConnectionParams& params,
                   std::function<void()> on_change)
        : wxCheckBox(parent, wxID_ANY,
                     _("Output on this port as autopilot or NMEA repeater")),
          m_params(params),
          m_on_change(std::move(on_change)) {
      OutputCheckBox::Reload();
      Bind(wxEVT_CHECKBOX, [&](wxCommandEvent& ev) {
        m_on_change();
        ev.Skip();
      });
    }

    void Reload() override {
      SetValue(m_params.direction != PortDirection::kInput);
    }

  private:
    ConnectionParams& m_params;
    std::function<void()> m_on_change;
  };

public:
  InputOutputPanel(wxWindow* parent, ConnectionParams& params)
      : wxPanel(parent), m_params(params) {
    auto vbox = new wxBoxSizer(wxVERTICAL);
    auto flags = wxSizerFlags().Border();
    m_input_box = new InputCheckBox(this, m_params, [&] { OnChange(); });
    m_output_box = new OutputCheckBox(this, m_params, [&] { OnChange(); });
    vbox->Add(m_input_box, flags);
    vbox->Add(m_output_box, flags);
    SetSizer(vbox);
    wxWindow::Layout();
  }

  void EnableOutput(bool enabled = true) { m_output_box->Enable(enabled); }

  void EnableInput(bool enabled = true) { m_input_box->Enable(enabled); }

  void SetOutput(bool value) { m_output_box->SetValue(value); }

  void SetInput(bool value) { m_input_box->SetValue(value); }

private:
  ConnectionParams& m_params;
  wxCheckBox* m_input_box;
  wxCheckBox* m_output_box;

  void OnChange() {
    if (m_input_box->GetValue() && m_output_box->GetValue())
      m_params.direction = PortDirection::kInOut;
    else if (m_input_box->GetValue())
      m_params.direction = PortDirection::kInput;
    else if (m_output_box->GetValue())
      m_params.direction = PortDirection::kOutput;
    else
      wxLogWarning("Inconsistent GUI direction state");
  }
};  //  InputOutputPanel

class SerialConfigPanel : public wxPanel {
  /** Select serial port combobox. */
  class PortCombo : public wxComboBox, public Reloadable {
  public:
    PortCombo(wxWindow* parent, wxString& port)
        : wxComboBox(parent, wxID_ANY), m_port(port) {
      LoadSerialPorts();
      Bind(wxEVT_COMBOBOX, [&](wxCommandEvent& e) {
        m_port = GetValue();
        e.Skip();
      });
    }

    void Reload() override { SetValue(m_port); }

  private:
    wxString& m_port;

    void LoadSerialPorts() {
      /** Sort all links to same device as equals. */
      class PortSorter {
      private:
        [[nodiscard]] static std::string GetKey(const std::string& s) {
          if (s.find("->") == std::string::npos) return s;
          return ocpn::trim(ocpn::split(s, "->")[1]) + " link";
        }

      public:
        bool operator()(const std::string& lhs, const std::string& rhs) const {
          return GetKey(lhs) < GetKey(rhs);
        }
      } port_sorter;

      std::set<std::string, PortSorter> sorted_ports(port_sorter);
      std::unique_ptr<wxArrayString> ports(EnumerateSerialPorts());
      for (size_t i = 0; i < ports->GetCount(); i++)
        sorted_ports.insert((*ports)[i].ToStdString());

      auto value = GetValue();
      Clear();
      for (auto& p : sorted_ports) Append(p);
      if (!value.empty())
        SetValue(value);
      else if (FindString(m_port) != wxNOT_FOUND)
        Select(FindString(m_port));
      else if (!sorted_ports.empty())
        SetSelection(0);
    }
  };  // PortCombo

  /** Select baudrate  combobox. */
  class BaudChoice : public wxComboBox, public Reloadable {
  public:
    BaudChoice(wxWindow* parent, int& baudrate)
        : wxComboBox(parent, wxID_ANY), m_baudrate(baudrate) {
      BaudChoice::Reload();
      Bind(wxEVT_COMBOBOX, [&](wxCommandEvent& e) { OnBaudChange(e); });
    };

    void Reload() override { LoadBaudrates(); }

  private:
    int& m_baudrate;

    void LoadBaudrates() {
      static const std::vector<std::string> speeds = {
          "150",   "300",   "600",   "1200",   "2400",   "4800",   "9600",
          "19200", "38400", "57600", "115200", "230400", "460800", "921600"};
      Clear();
      for (const auto& speed : speeds) Append(speed);
      auto found = FindString(std::to_string(m_baudrate));
      if (found != wxNOT_FOUND)
        SetSelection(found);
      else
        SetSelection(5);  // 4800
    }

    void OnBaudChange(wxCommandEvent& e) {
      std::string speed = GetStringSelection().ToStdString();
      try {
        m_baudrate = std::stoi(speed);
      } catch (std::logic_error&) {
        assert(false && "Cannot parse preconfigured baudrate");
      }
      e.Skip();
    }
  };  // BaudChoice

  /** Select serial NMEA0183/NMEA2000 protocol combobox. */
  class ProtocolChoice : public wxChoice, public Reloadable {
  public:
    ProtocolChoice(wxWindow* parent, DataProtocol& protocol)
        : wxChoice(parent, wxID_ANY), m_protocol(protocol) {
      ProtocolChoice::Reload();
      Bind(wxEVT_CHOICE, [&](wxCommandEvent& e) { OnProtocolChange(e); });
    };

    void Reload() override { LoadProtocols(); }

  private:
    DataProtocol& m_protocol;

    void OnProtocolChange(wxCommandEvent& e) {
      std::string protocol = GetStringSelection().ToStdString();
      if (protocol == "0")
        m_protocol = PROTO_NMEA0183;
      else if (protocol == "1")
        m_protocol = PROTO_NMEA2000;
      else
        wxLogWarning("Illegal serial protocol %s", protocol.c_str());
      e.Skip();
    }

    void LoadProtocols() {
      Clear();
      Append("NMEA 0183");
      Append("NMEA 2000");
      SetSelection(0);
      if (m_protocol == PROTO_NMEA2000) SetSelection(1);
    }
  };  //  ProtocolChoice

  /** User comment text entry. */
  class CommentText : public wxTextCtrl, public Reloadable {
  public:
    CommentText(wxWindow* parent, wxString& comment)
        : wxTextCtrl(parent, wxID_ANY), m_comment(comment) {
      CommentText::Reload();
      Bind(wxEVT_TEXT, [&](wxCommandEvent& e) { m_comment = GetValue(); });
    }

    void Reload() override { SetValue(m_comment); }

  private:
    wxString& m_comment;
  };

  class SerialParamsGrid : public wxFlexGridSizer {
  public:
    SerialParamsGrid(wxWindow* parent, ConnectionParams& params)
        : wxFlexGridSizer(2), m_params(params) {
      auto flags = wxSizerFlags().Border();
      auto text_flags = flags.CentreVertical();
      Add(new wxStaticText(parent, wxID_ANY, _("Data port")), text_flags);
      Add(new PortCombo(parent, m_params.serial_port), flags.Expand());
      Add(new wxStaticText(parent, wxID_ANY, _("Baudrate")), text_flags);
      Add(new BaudChoice(parent, m_params.baudrate), flags.Expand());
      Add(new wxStaticText(parent, wxID_ANY, _("Protocol")), text_flags);
      Add(new ProtocolChoice(parent, m_params.data_protocol), flags.Expand());
      Add(new wxStaticText(parent, wxID_ANY, _("User Comment")), text_flags);
      Add(new CommentText(parent, m_params.user_comment), flags.Expand());
    }

  private:
    ConnectionParams& m_params;
  };  // NetGrid

  class AdvancedPanel : public wxPanel {
    class GarminCheckBox : public wxCheckBox, public Reloadable {
    public:
      GarminCheckBox(wxWindow* parent, ConnectionParams& params)
          : wxCheckBox(parent, wxID_ANY, _("Use Garmin (GRMN) mode for input")),
            m_params(params) {
        GarminCheckBox::Reload();
        Bind(wxEVT_CHECKBOX, [&](wxCommandEvent& ev) {
          m_params.is_garmin = GetValue();
          ev.Skip();
        });
      }

      void Reload() override { SetValue(m_params.is_garmin); }

    private:
      ConnectionParams& m_params;
    };

  public:
    AdvancedPanel(wxWindow* parent, ConnectionParams& params)
        : wxPanel(parent, wxID_ANY), m_params(params) {
      auto vbox = new wxBoxSizer(wxVERTICAL);
      auto flags = wxSizerFlags().Border();
      vbox->Add(new GarminCheckBox(this, m_params), flags);
      vbox->Add(new FilterConfigPanel(this, m_params), flags);
      SetSizer(vbox);
      wxWindowBase::Layout();
    }

  private:
    ConnectionParams& m_params;
  };

public:
  SerialConfigPanel(wxWindow* parent, ConnectionParams& params)
      : wxPanel(parent), m_params(params) {
    auto flags = wxSizerFlags().Border();
    m_grid = new SerialParamsGrid(this, params);

    m_advanced_panel = new AdvancedPanel(this, params);
    auto advanced_ctrl = new ExpandablePanel(this, m_advanced_panel,
                                             [&] { GetParent()->Layout(); });
    auto hbox = new wxBoxSizer(wxHORIZONTAL);
    hbox->Add(new wxStaticText(this, wxID_ANY, _("Advanced")), flags);
    hbox->Add(advanced_ctrl, flags);

    auto vbox = new wxBoxSizer(wxVERTICAL);
    vbox->Add(m_grid, flags);
    vbox->Add(new InputOutputPanel(this, params), flags);
    vbox->Add(hbox, flags);
    vbox->Add(m_advanced_panel, flags);
    SetSizer(vbox);
    wxWindow::Layout();
    wxWindow::Show();
  }

private:
  ConnectionParams& m_params;
  wxWindow* m_advanced_panel;
  SerialParamsGrid* m_grid;
};

class NetConfigPanel : public wxPanel {
  class ExpertModeCheckBox : public wxCheckBox {
  public:
    ExpertModeCheckBox(wxWindow* parent) : wxCheckBox(parent, wxID_ANY, "") {}
  };

  class ConnectionTypeChoice : public wxChoice {
  public:
    ConnectionTypeChoice(wxWindow* parent, std::function<void()> on_choice)
        : wxChoice(parent, wxID_ANY), m_on_choice(on_choice) {
      for (const auto& view : kBasicNetViews) Append(view);  // FIXME
      Append(kBasicNetViews[0]);
      wxChoice::SetSelection(0);  // until OnConnectionTypeChanged()
      Bind(wxEVT_CHOICE, [&](wxCommandEvent e) {
        m_on_choice();
        e.Skip();
      });
    }

  private:
    std::function<void()> m_on_choice;
  };

  class ProtocolChoice : public wxChoice, public Reloadable {
  public:
    ProtocolChoice(wxWindow* parent, ConnectionParams& params)
        : wxChoice(parent, wxID_ANY), m_params(params) {
      ProtocolChoice::Reload();
      Bind(wxEVT_CHOICE, [&](wxCommandEvent& e) { OnChoice(e); });
    }

    void Reload() override {
      Clear();
      Append("NMEA 0183");
      Append("NMEA 2000");
      switch (m_params.data_protocol) {
        case PROTO_NMEA0183:
          SetSelection(0);
          break;
        case PROTO_NMEA2000:
          SetSelection(1);
          break;
        default:
          assert(false && "Illegal SignalK protocol");
          break;
      }
      Enable();
    }

  private:
    ConnectionParams& m_params;

    void OnChoice(wxCommandEvent& e) {
      switch (GetSelection()) {
        case 0:
          m_params.data_protocol = PROTO_NMEA0183;
          break;
        case 1:
          m_params.data_protocol = PROTO_NMEA2000;
          break;
        default:
          assert(false && "Illegal protocol combobox value");
      };
      e.Skip();
    }
  };  // ProtocolChoice

  class AddressTextCtrl : public TextCtrlWithHelp, public Reloadable {
  public:
    AddressTextCtrl(wxWindow* parent, ConnectionParams& params)
        : TextCtrlWithHelp(parent,
                           "IP address or hostname, often 255.255.255.255"),
          m_params(params) {
      AddressTextCtrl::Reload();
      Bind(wxEVT_TEXT, [&](wxCommandEvent& e) {
        m_params.network_address = GetLabel();
        e.Skip();
      });
      SetMinSize(wxSize(60 * GetCharWidth(), -1));
    }

    void Reload() override { SetLabel(m_params.network_address); }

  private:
    ConnectionParams& m_params;
  };

  class PortTextCtrl : public TextCtrlWithHelp, public Reloadable {
  public:
    PortTextCtrl(wxWindow* parent, ConnectionParams& params)
        : TextCtrlWithHelp(parent, "Port number (1025 - 65535, often 10110)"),
          m_params(params) {
      PortTextCtrl::Reload();
      Bind(wxEVT_TEXT, [&](wxCommandEvent& e) { OnChange(e); });
    }

    void Reload() override { SetLabel(std::to_string(m_params.network_port)); }

  private:
    ConnectionParams& m_params;

    void OnChange(wxCommandEvent& event) {
      try {
        m_params.network_port = std::stoi(GetLabel().ToStdString());
      } catch (std::logic_error& e) {
        wxLogWarning("Cannot parse configuration network port: %s", e.what());
        m_params.network_port = 99999;  // Illegal for ipv4
      }
      event.Skip();
    }
  };  // PortTextCtrl

  class CommentTextCtrl : public wxTextCtrl, public Reloadable {
  public:
    CommentTextCtrl(wxWindow* parent, ConnectionParams& params)
        : wxTextCtrl(parent, wxID_ANY), m_params(params) {
      CommentTextCtrl::Reload();
      Bind(wxEVT_TEXT, [&](wxCommandEvent& e) {
        m_params.user_comment = GetLabel();
        e.Skip();
      });
    }

    void Reload() override { SetLabel(m_params.user_comment); }

  private:
    ConnectionParams& m_params;
  };

  class NetGrid : public wxFlexGridSizer {
  public:
    NetGrid(wxWindow* parent, ConnectionParams& params,
            InputOutputPanel* in_out_panel)
        : wxFlexGridSizer(2),
          m_parent(parent),
          m_params(params),
          m_in_out_panel(in_out_panel) {
      auto flags = wxSizerFlags().Border();
      auto label_flags = wxSizerFlags().Border().CentreVertical();

      m_expert_mode_checkbox = new ExpertModeCheckBox(parent);
      m_expert_mode_checkbox->Bind(wxEVT_CHECKBOX, [&](wxCommandEvent& e) {
        OnExpertModeChange();
        e.Skip();
      });
      m_expert_mode_label =
          new wxStaticText(parent, wxID_ANY, _("Expert mode"));
      Add(m_expert_mode_label, label_flags);
      Add(m_expert_mode_checkbox, flags);

      m_connection_type_label =
          new wxStaticText(parent, wxID_ANY, _("Connection type"));
      m_connection_type_choice =
          new ConnectionTypeChoice(parent, [&] { OnConnectionTypeChange(); });
      Add(m_connection_type_label, label_flags);
      Add(m_connection_type_choice, flags);

      m_protocol_label = new wxStaticText(parent, wxID_ANY, _("Data Protocol"));
      m_protocol_choice = new ProtocolChoice(parent, m_params);
      Add(m_protocol_label, label_flags);
      Add(m_protocol_choice, flags);

      m_address_label = new wxStaticText(parent, wxID_ANY, _("Server address"));
      m_address_textctrl = new AddressTextCtrl(parent, m_params);
      Add(m_address_label, label_flags);
      Add(m_address_textctrl, wxSizerFlags(1).Expand().Border());

      m_port_label = new wxStaticText(parent, wxID_ANY, _("Data Port"));
      m_port_textctrl = new PortTextCtrl(parent, m_params);
      Add(m_port_label, label_flags);
      Add(m_port_textctrl, wxSizerFlags(1).Expand().Border());

      m_comment_label = new wxStaticText(parent, wxID_ANY, _("User comment"));
      m_comment_textctrl = new CommentTextCtrl(parent, m_params);
      Add(m_comment_label, label_flags);
      Add(m_comment_textctrl, wxSizerFlags(1).Expand().Border());
    }

  private:
    wxWindow* m_parent;
    ConnectionParams& m_params;
    InputOutputPanel* m_in_out_panel;
    wxStaticText* m_expert_mode_label;
    ExpertModeCheckBox* m_expert_mode_checkbox;
    wxStaticText* m_connection_type_label;
    wxChoice* m_connection_type_choice;
    wxStaticText* m_protocol_label;
    wxChoice* m_protocol_choice;
    wxStaticText* m_address_label;
    TextCtrlWithHelp* m_address_textctrl;
    wxStaticText* m_port_label;
    TextCtrlWithHelp* m_port_textctrl;
    wxStaticText* m_comment_label;
    wxTextCtrl* m_comment_textctrl;

    void OnExpertModeChange() {
      bool advanced = m_expert_mode_checkbox->GetValue();
      int view = m_connection_type_choice->GetSelection();
      m_connection_type_choice->Clear();
      if (advanced) {
        for (const auto& choice : kAdvancedNetViews)
          m_connection_type_choice->Append(choice);
        m_connection_type_choice->SetLabel(_("Connection type"));
        if (m_comment_label) m_comment_label->Show();
        if (m_comment_textctrl) m_comment_textctrl->Show();
      } else {
        if (view != wxNOT_FOUND) {
          if (static_cast<size_t>(view) >= kBasicNetViews.size()) view = 0;
        }
        for (const auto& choice : kBasicNetViews)
          m_connection_type_choice->Append(choice);
        m_connection_type_label->SetLabel(_("Connect to"));
        if (m_comment_label) m_comment_label->Hide();
        if (m_comment_textctrl) m_comment_textctrl->Hide();
      }
      m_connection_type_choice->SetSelection(view);
      auto view_str =
          m_connection_type_choice->GetStringSelection().ToStdString();
      if (!view_str.empty()) ConfigureControlsForView(view_str);
      m_parent->Layout();
    }

    void OnConnectionTypeChange() {
      int selection = m_connection_type_choice->GetSelection();
      if (selection == wxNOT_FOUND) return;
      std::string view =
          m_connection_type_choice->GetStringSelection().ToStdString();
      ConfigureControlsForView(view);
      OnExpertModeChange();
    }

    void ConfigureControlsForView(const std::string& view) {
      auto found =
          std::find(kBasicNetViews.begin(), kBasicNetViews.end(), view);
      int selection = m_connection_type_choice->GetSelection();
      assert(selection != wxNOT_FOUND);
      m_expert_mode_checkbox->SetValue(found == kBasicNetViews.end());
      m_address_textctrl->Enable();
      m_address_label->SetLabel(_("Server address"));
      m_in_out_panel->EnableOutput(false);
      m_in_out_panel->EnableInput(false);
      m_address_label->Show();
      m_address_textctrl->Show();
      auto port = m_port_textctrl->GetValue();
      if (port == kDefaultGpsdPort || port == kDefaultSignalkPort ||
          port.empty())
        m_port_textctrl->RestoreHelp();
      if (m_protocol_choice->GetCount() != 2)
        SetupProtocolChoice(m_protocol_choice);  // FIXME - leamas
      if (view == kTcpDevice || view == kTcpClient) {
        m_address_label->Show();
        m_address_label->Show();
        if (m_address_textctrl->GetValue() == "0.0.0.0")
          m_address_textctrl->RestoreHelp();
        if (m_address_textctrl->IsPristine())
          m_address_textctrl->SetHelp(kAddressDefaultHelp);
        if (m_port_textctrl->IsPristine())
          m_port_textctrl->SetHelp(
              _("Port number (1025 - 65535, often 10110)"));
        m_in_out_panel->EnableInput();
        m_in_out_panel->EnableOutput();
      } else if (view == kUdpReceive || view == kUdpInput) {
        m_address_label->Hide();
        m_address_textctrl->ChangeValue("0.0.0.0");
        m_address_textctrl->Disable();
        m_address_textctrl->Hide();
        if (m_address_textctrl->IsPristine())
          m_address_textctrl->SetHelp(kAddressDefaultHelp);
        if (m_port_textctrl->IsPristine())
          m_port_textctrl->SetHelp(
              _("Port number (1025 - 65535, often 10110)"));
        m_in_out_panel->SetOutput(false);
        m_in_out_panel->SetInput(true);
      } else if (view == kUdpSend || view == kUdpOutput) {
        if (m_address_textctrl->GetValue() == "0.0.0.0")
          m_address_textctrl->RestoreHelp();
        if (m_address_textctrl->IsPristine())
          m_address_textctrl->SetHelp(kAddressUdpHelp);
        m_in_out_panel->SetInput(false);
        m_in_out_panel->SetOutput(true);
      } else if (view == kGpsdClient || view == kGpsdDevice) {
        m_protocol_choice->Clear();
        m_protocol_choice->Append("gpsd");
        m_protocol_choice->SetSelection(0);
        m_protocol_choice->Disable();
        if (m_address_textctrl->GetValue() == "0.0.0.0")
          m_address_textctrl->RestoreHelp();
        if (m_address_textctrl->IsPristine())
          m_address_textctrl->SetHelp(kAddressDefaultHelp);
        if (m_port_textctrl->IsPristine())
          m_port_textctrl->ChangeValue(kDefaultGpsdPort);
        m_in_out_panel->SetInput(true);
        m_in_out_panel->SetOutput(false);
      } else if (view == kSignalkClient || view == kSignalkDevice) {
        m_protocol_choice->Clear();
        m_protocol_choice->Append("SignalK");
        m_protocol_choice->SetSelection(0);
        m_protocol_choice->Disable();
        if (m_address_textctrl->GetValue() == "0.0.0.0")
          m_address_textctrl->RestoreHelp();
        if (m_address_textctrl->IsPristine())
          m_address_textctrl->SetHelp(kAddressDefaultHelp);
        if (m_port_textctrl->IsPristine())
          m_port_textctrl->ChangeValue(kDefaultSignalkPort);
        m_in_out_panel->SetInput(true);
        m_in_out_panel->SetOutput(false);
      } else if (view == kTcpServer) {
        m_address_label->SetLabel(_("Interface"));
        m_address_textctrl->ChangeValue("0.0.0.0");
        m_address_textctrl->Disable();
        m_in_out_panel->EnableInput();
        m_in_out_panel->EnableOutput();
      } else if (view == kMulticastClient || view == kMulticastServer) {
        if (m_address_textctrl->GetValue().empty())
          m_address_textctrl->RestoreHelp();
        m_address_label->SetLabel(_("Multicast group"));
        if (m_port_textctrl->IsPristine())
          m_port_textctrl->SetHelp(_("Port number, usually 49152 - 65535"));
        if (m_address_textctrl->IsPristine())
          m_address_textctrl->SetHelp(kAddressMcastHelp);
        if (view == kMulticastClient) {
          m_in_out_panel->SetInput(false);
          m_in_out_panel->SetOutput(true);
        } else {
          m_in_out_panel->SetInput(true);
          m_in_out_panel->EnableOutput();
        }
      } else {
        wxLogError("Illegal view: %s", view.c_str());
        assert(false && "Illegal view");
      }
    }
  };  // NetGrid

public:
  NetConfigPanel(wxWindow* parent, ConnectionParams& params)
      : wxPanel(parent), m_params(params) {
    auto flags = wxSizerFlags().Border();
    auto vbox = new wxBoxSizer(wxVERTICAL);
    auto input_output_panel = new InputOutputPanel(this, m_params);
    vbox->Add(new NetGrid(this, m_params, input_output_panel), flags);
    vbox->Add(input_output_panel, flags);

    auto advanced_panel = new FilterConfigPanel(this, m_params);
    auto hbox = new wxBoxSizer(wxHORIZONTAL);
    hbox->Add(new wxStaticText(this, wxID_ANY, _("Advanced")));
    hbox->Add(new ExpandablePanel(this, advanced_panel,
                                  [&] { GetParent()->Layout(); }));
    vbox->Add(hbox, flags);
    vbox->Add(advanced_panel, flags);

    SetSizer(vbox);
    wxWindow::Layout();
    wxWindow::Show();
  }

private:
  ConnectionParams& m_params;
};

class CanConfigPanel : public wxPanel {
  class CanChoice : public wxChoice, public Reloadable {
  public:
    CanChoice(wxWindow* parent, ConnectionParams& params)
        : wxChoice(parent, wxID_ANY), m_params(params) {
      CanChoice::Reload();
      Bind(wxEVT_CHOICE, [&](wxCommandEvent& e) {
        m_params.socket_can_port = GetStringSelection();
        e.Skip();
      });
    };

    void Reload() override {
      Clear();
      for (const auto& port : GetAvailableSocketCanInterfaces()) Append(port);
      int found = FindString(m_params.socket_can_port);
      SetSelection(found != wxNOT_FOUND ? found : 0);
    }

  private:
    ConnectionParams& m_params;

    static std::vector<std::string> GetAvailableSocketCanInterfaces() {
      std::vector<std::string> rv;

#if defined(__linux__) && !defined(__ANDROID__)
      struct intf_entry* entry;

      can_if_candidates.clear();  // FIXME: global...

      if ((intf = intf_open()) == nullptr) {
        wxLogWarning("Error opening interface list");
        return rv;
      }

      if (intf_loop(intf, print_intf, nullptr) < 0) {
        wxLogWarning("Error looping over interface list");
      }
      intf_close(intf);

      for (const auto& iface : can_if_candidates) {
        int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (sock < 0) {
          continue;
        }

        // Get the interface index
        ifreq if_request;
        strcpy(if_request.ifr_name, iface.c_str());
        if (ioctl(sock, SIOCGIFINDEX, &if_request) < 0) {
          continue;
        }

        // Check if interface is UP
        sockaddr_can can_address;
        can_address.can_family = AF_CAN;
        can_address.can_ifindex = if_request.ifr_ifindex;
        if (ioctl(sock, SIOCGIFFLAGS, &if_request) < 0) {
          continue;
        }
        if (if_request.ifr_flags & IFF_UP) {
          rv.push_back(iface);
        }
      }
#endif  // defined(__linux__) && !defined(__ANDROID__)
      return rv;
    }
  };

public:
  CanConfigPanel(wxWindow* parent, ConnectionParams& params)
      : wxPanel(parent), m_params(params) {
    auto hbox = new wxBoxSizer(wxHORIZONTAL);
    hbox->Add(new wxStaticText(this, wxID_ANY, "Socketcan source"),
              wxSizerFlags().Border().CentreVertical());
    m_choice = new CanChoice(this, m_params);
    m_choice->Append("foobar");
    hbox->Add(m_choice, wxSizerFlags().Border());
    SetSizer(hbox);
    wxWindow::Layout();
    wxWindow::Show();
  }

private:
  ConnectionParams& m_params;
  wxChoice* m_choice;

  static std::vector<std::string> GetAvailableSocketCanInterfaces() {
    std::vector<std::string> rv;

#if defined(__linux__) && !defined(__ANDROID__)
    struct intf_entry* entry;

    can_if_candidates.clear();  // FIXME: global...

    if ((intf = intf_open()) == nullptr) {
      wxLogWarning("Error opening interface list");
      return rv;
    }

    if (intf_loop(intf, print_intf, nullptr) < 0) {
      wxLogWarning("Error looping over interface list");
    }
    intf_close(intf);

    for (const auto& iface : can_if_candidates) {
      int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
      if (sock < 0) {
        continue;
      }

      // Get the interface index
      ifreq if_request;
      strcpy(if_request.ifr_name, iface.c_str());
      if (ioctl(sock, SIOCGIFINDEX, &if_request) < 0) {
        continue;
      }

      // Check if interface is UP
      sockaddr_can can_address;
      can_address.can_family = AF_CAN;
      can_address.can_ifindex = if_request.ifr_ifindex;
      if (ioctl(sock, SIOCGIFFLAGS, &if_request) < 0) {
        continue;
      }
      if (if_request.ifr_flags & IFF_UP) {
        rv.push_back(iface);
      }
    }
#endif  // defined(__linux__) && !defined(__ANDROID__)
    return rv;
  }
};

class ButtonSizer : public wxStdDialogButtonSizer {
public:
  ButtonSizer(wxWindow* parent, std::function<void(wxCommandEvent&)> on_ok,
              std::function<void(wxCommandEvent&)> on_cancel)
      : wxStdDialogButtonSizer(),
        m_on_ok(std::move(on_ok)),
        m_on_cancel(std::move(on_cancel)) {
    auto ok_btn = new wxButton(parent, wxID_OK);
    ok_btn->Bind(wxEVT_BUTTON, [&](wxCommandEvent& ev) { m_on_ok(ev); });
    AddButton(ok_btn);
    auto cancel_btn = new wxButton(parent, wxID_CANCEL);
    cancel_btn->Bind(wxEVT_BUTTON,
                     [&](wxCommandEvent& ev) { m_on_cancel(ev); });
    AddButton(cancel_btn);
    SetAffirmativeButton(ok_btn);
    Realize();
  }

private:
  std::function<void(wxCommandEvent&)> m_on_ok;
  std::function<void(wxCommandEvent&)> m_on_cancel;
};

}  // Anonymous namespace

class ConnectionEditDlg : public AbstractConnectionEditDlg {
public:
  explicit ConnectionEditDlg(wxWindow* parent, std::function<void()> on_cancel,
                             ConnEditCallback on_ok)
      : AbstractConnectionEditDlg(parent),
        m_on_ok(std::move(on_ok)),
        m_on_cancel(std::move(on_cancel)),
        m_params(GetDefaultParams()),
        m_is_new(true) {
    auto vbox = new wxBoxSizer(wxVERTICAL);
    auto statbox = new wxStaticBoxSizer(wxVERTICAL, this, _("Edit connection"));
    auto on_type_btn_click = [&](ConnectionType ct) { OnTypeBtnClick(ct); };
    auto flags = wxSizerFlags().Border();
    m_type_buttons = new TypeButtons(this, on_type_btn_click, m_params);
    statbox->Add(m_type_buttons, wxSizerFlags().Border());
    m_serial_config_panel = new SerialConfigPanel(this, m_params);
    statbox->Add(m_serial_config_panel);
    m_net_config_panel = new NetConfigPanel(this, m_params);
    statbox->Add(m_net_config_panel);
    m_can_config_panel = new CanConfigPanel(this, m_params);
    statbox->Add(m_can_config_panel);
    vbox->Add(statbox, flags.Expand());
    auto on_ok_ = [&](wxCommandEvent& ev) { OnOkCancel(ev, true); };
    auto on_cancel_ = [&](wxCommandEvent& ev) { OnOkCancel(ev, false); };
    auto button_sizer = new ButtonSizer(this, on_ok_, on_cancel_);
    vbox->Add(button_sizer, flags.Expand());
    m_type_buttons->SetSelectedType(ConnectionType::NETWORK);
    OnTypeBtnClick(ConnectionType::NETWORK);

    SetSizer(vbox);
    wxWindow::Fit();
    wxWindow::Show();
  }

  void SetParams(std::unique_ptr<ConnectionParams> p) override {
    m_is_new = !bool(p);
    m_params = p ? *p : GetDefaultParams();
    Reload();
    OnTypeBtnClick(m_params.type);
  }

  std::unique_ptr<ConnectionParams> GetEditedParams() override {
    return std::make_unique<ConnectionParams>(m_params);
  }

private:
  ConnEditCallback m_on_ok;
  std::function<void()> m_on_cancel;
  // std::shared_ptr<ConnectionParams> m_start_params;
  ConnectionParams m_params;

  TypeButtons* m_type_buttons;
  wxWindow* m_net_config_panel;
  wxWindow* m_can_config_panel;
  wxWindow* m_serial_config_panel;
  bool m_is_new;

  static void DoReload(wxWindow* root) {
    for (wxWindow* child : root->GetChildren()) {
      if (auto widget = dynamic_cast<Reloadable*>(child)) widget->Reload();
      DoReload(child);
    }
  }

  void Reload() { DoReload(this); }

  void OnTypeBtnClick(ConnectionType ct) {
    m_net_config_panel->Show(ct == ConnectionType::NETWORK);
    m_can_config_panel->Show(ct == ConnectionType::SOCKETCAN);
    m_serial_config_panel->Show(ct == ConnectionType::SERIAL);
    m_params.type = ct;
    Layout();
  }

  void OnOkCancel(wxCommandEvent&, bool ok) {
    if (ok)
      m_on_ok(std::make_unique<ConnectionParams>(m_params), m_is_new);
    else
      m_on_cancel();
  }

  static ConnectionParams GetDefaultParams() {
    ConnectionParams cp;
    cp.type = ConnectionType::NETWORK;
    return cp;
  }
};

AbstractConnectionEditDlg* AbstractConnectionEditDlg::Create(
    wxWindow* parent, std::function<void()> on_cancel, ConnEditCallback on_ok) {
  return new ConnectionEditDlg(parent, std::move(on_cancel), std::move(on_ok));
}
