#ifndef _ABSTRACT_NMEA_LOG__
#define _ABSTRACT_NMEA_LOG__

#include <wx/string.h>

#include "model/comm_navmsg.h"
#include "model/navmsg_filter.h"

/** Item in the log window. */
struct Logline {
  const std::string line;
  const NavmsgStatus state;
  const std::string stream_name;
  std::string error_msg;

  Logline() : line(), state() {}
  Logline(const std::string& s, NavmsgStatus sts, const std::string& stream)
      : line(s), state(sts), stream_name(stream), error_msg("Unknown error") {}
};

class NmeaLog {
public:
  /** Add an formatted string to log output. */
  virtual void Add(struct Logline l) = 0;

  /** Return true if log is visible i. e., if it's any point using Add(). */
  virtual bool Active() const = 0;
};

#endif  // _ABSTRACT_NMEA_LOG__
