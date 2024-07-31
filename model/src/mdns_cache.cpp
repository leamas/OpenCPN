/**************************************************************************
 *   Copyright (C) 2024 Alec Leamas                                        *
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

/** \file  mdns_cache.cpp Implement mdns_cache.h */

#include <algorithm>
#include <iostream>

#include <curl/curl.h>

#include "model/logger.h"
#include "model/mdns_cache.h"

/**
 * Check if we can connect to given host/port, does not
 * care if we cannot recieve data.
 */
static bool Ping(const std::string& url, long port = 8443L) {
  CURL* c = curl_easy_init();
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_PORT, port);
  curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 2000L);
  CURLcode result = curl_easy_perform(c);
  curl_easy_cleanup(c);
  bool ok = result == CURLE_RECV_ERROR || result == CURLE_OK;
  auto status = ok ? "ok" : curl_easy_strerror(result);
  DEBUG_LOG << "Checked mdns host: " << url << ": " << status;
  return ok;
}

MdnsCache& MdnsCache::GetInstance() {
  static MdnsCache dns_cache;
  return dns_cache;
}

bool MdnsCache::Add(const MdnsCache::Entry& entry) {
  std::unique_lock lock(m_mutex);
  DEBUG_LOG << "Adding mdns cache entry, ip: " << entry.ip;
  auto found =
      std::find_if(the_cache.begin(), the_cache.end(),
                   [entry](MdnsCache::Entry& e) { return e.ip == entry.ip; });
  if (found == the_cache.end()) the_cache.push_back(entry);
  DEBUG_LOG << "Added mdns cache entry, ip: " << entry.ip << ", status: " <<
      (found == the_cache.end() ? "true" : "false");
  return found == the_cache.end();
}

bool MdnsCache::Add(const std::string& service, const std::string& host,
                    const std::string& _ip, const std::string& _port) {
  return Add(MdnsCache::Entry(service, host, _ip, _port));
}

bool MdnsCache::Add(const std::string& _ip, const std::string& _port) {
  return Add(MdnsCache::Entry("opencpn", "unknown", _ip, _port));
}

void MdnsCache::Validate() {
  std::unique_lock lock(m_mutex);
  for (auto it = the_cache.begin(); it != the_cache.end();) {
    if (!Ping(it->ip)) {
      the_cache.erase(it);
    } else {
      it++;
    }
  }
}
