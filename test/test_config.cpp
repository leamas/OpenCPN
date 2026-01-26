/**************************************************************************
 *   Copyright (C) 2026 Alec Leamas                                        *
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
 * Configure file the file test_config.h to contain useful test constants
 * related to locales and local time zone
 */

#include <ctime>
#include <fstream>

#include <wx/intl.h>

#if wxCHECK_VERSION(3, 1, 6)
#include <wx/uilocale.h>
#endif

int main(int argc, char** argv) {
  std::ofstream stream("test_config.h");
#if wxCHECK_VERSION(3, 1, 6)
  wxUILocale us_locale = wxUILocale::FromTag("en_US.UTF-8");
  if (us_locale.IsSupported())
    stream << "#define HAS_EN_US\n";
  else
    stream << "// #define HAS_EN_US\n";
  wxUILocale sv_locale = wxUILocale::FromTag("sv_SE.UTF-8");
  if (sv_locale.IsSupported())
    stream << "#define HAS_SV_SE\n";
  else
    stream << "// #define HAS_SV_SE\n";
#else
  stream << "\n";
#endif
  struct tm tm;
  tm.tm_sec = 57;
  tm.tm_min = 45;
  tm.tm_hour = 13;
  tm.tm_mon = 1;
  tm.tm_mday = 22;
  tm.tm_year = 123;
  tm.tm_wday = 3;
  char buf[128];
  std::strftime(buf, sizeof(buf), "%A, %B %d, %Y %H:%M:%S", &tm);
  stream << "#define LOCALTIME_DATE \"" << buf << "\"\n";
}
