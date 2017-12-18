/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "common.h"
#include "destination.h"
#include "logger.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/utils.h"
#include "utils.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#ifndef _WIN32
#  include <netdb.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

using mysqlrouter::to_string;
using mysqlrouter::TCPAddress;
using std::out_of_range;

// Timeout for trying to connect with quarantined servers
static const int kQuarantinedConnectTimeout = 1;
// How long we pause before checking quarantined servers again (seconds)
static const int kQuarantineCleanupInterval = 3;
// Make sure Quarantine Manager Thread is run even with nothing in quarantine
static const int kTimeoutQuarantineConditional = 2;

RouteDestination::~RouteDestination() {

  stopping_ = true;
  if (quarantine_thread_.joinable()) {
    quarantine_thread_.join();
  }
}

void RouteDestination::add(const TCPAddress dest) {
  auto dest_end = destinations_.end();

  auto compare = [&dest](TCPAddress &other) { return dest == other; };

  if (std::find_if(destinations_.begin(), dest_end, compare) == dest_end) {
    std::lock_guard<std::mutex> lock(mutex_update_);
    destinations_.push_back(dest);
  }
}

void RouteDestination::add(const std::string &address, uint16_t port) {
  add(TCPAddress(address, port));
}

void RouteDestination::remove(const std::string &address, uint16_t port) {
  TCPAddress to_remove(address, port);
  std::lock_guard<std::mutex> lock(mutex_update_);

  auto func_same = [&to_remove](TCPAddress a) {
    return (a.addr == to_remove.addr && a.port == to_remove.port);
  };
  destinations_.erase(std::remove_if(destinations_.begin(), destinations_.end(), func_same), destinations_.end());

}

TCPAddress RouteDestination::get(const std::string &address, uint16_t port) {
  TCPAddress needle(address, port);
  for (auto &it: destinations_) {
    if (it == needle) {
      return it;
    }
  }
  throw out_of_range("Destination " + needle.str() + " not found");
}

size_t RouteDestination::size() noexcept {
  return destinations_.size();
}

void RouteDestination::clear() {
  if (destinations_.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_update_);
  destinations_.clear();
}

int RouteDestination::get_server_socket(int connect_timeout, int *error) noexcept {

  if (destinations_.empty()) {
    log_warning("No destinations currently available for routing");
    return -1;  // no destination is available
  }

  // We start the list at the currently available server
  for (size_t i = current_pos_;
       quarantined_.size() < destinations_.size() && i < destinations_.size();
       i = (i+1) % destinations_.size()) {

    // If server is quarantined, skip
    {
      std::lock_guard<std::mutex> lock(mutex_quarantine_);
      if (is_quarantined(i)) {
        continue;
      }
    }

    // Try server
    TCPAddress addr;
    addr = destinations_.at(i);
    log_debug("Trying server %s (index %d)", addr.str().c_str(), i);
    auto sock = get_mysql_socket(addr, connect_timeout);

    if (sock != -1) {
      // Server is available
      current_pos_ = (i + 1) % destinations_.size(); // Reset to 0 when current_pos_ == size()
      return sock;
    } else {
#ifndef _WIN32
      *error = errno;
#else
      *error = WSAGetLastError();
#endif
      if (errno != ENFILE && errno != EMFILE) {
        // We failed to get a connection to the server; we quarantine.
        std::lock_guard<std::mutex> lock(mutex_quarantine_);
        add_to_quarantine(i);
        if (quarantined_.size() == destinations_.size()) {
          log_debug("No more destinations: all quarantined");
          break;
        }
        continue; // try another destination
      }
      break;
    }
  }

  current_pos_ = 0;
  return -1; // no destination is available
}

std::unique_ptr<ServerGroup> RouteDestination::GetServerGroup() {
  std::vector<int> server_fds;
  bool error = false;

  for (auto &addr : destinations_) {
    log_debug("Connecting to server %s:%d", addr.addr.c_str(), addr.port);
    int fd = socket_operations_->get_mysql_socket(addr, routing::kDefaultDestinationConnectionTimeout, true);
    if (fd <= 0) {
      log_error("Failed to connect to server when creating server group");
      error = true;
      break;
    }
    server_fds.push_back(fd);
  }
  log_debug("Connected to all servers");

  if (error) {
    for (auto fd : server_fds) {
      socket_operations_->close(fd);
    }
    server_fds.clear();
    return nullptr;
  }

  return std::unique_ptr<ServerGroup>(new ServerGroup(server_fds));
}

int RouteDestination::get_mysql_socket(const TCPAddress &addr, const int connect_timeout, const bool log_errors) {
  return socket_operations_->get_mysql_socket(addr, connect_timeout, log_errors);
}

void RouteDestination::add_to_quarantine(const size_t index) noexcept {
  assert(index < size());
  if (index >= size()) {
    log_debug("Impossible server being quarantined (index %d)", index);
    return;
  }
  if (!is_quarantined(index)) {
    log_debug("Quarantine destination server %s (index %d)", destinations_.at(index).str().c_str(), index);
    quarantined_.push_back(index);
    condvar_quarantine_.notify_one();
  }
}

void RouteDestination::cleanup_quarantine() noexcept {

  mutex_quarantine_.lock();
  // Nothing to do when nothing quarantined
  if (quarantined_.empty()) {
    mutex_quarantine_.unlock();
    return;
  }
  // We work on a copy; updating the original
  auto cpy_quarantined(quarantined_);
  mutex_quarantine_.unlock();

  for (auto it = cpy_quarantined.begin(); it != cpy_quarantined.end(); ++it) {
    if (stopping_) {
      return;
    }

    auto addr = destinations_.at(*it);
    auto sock = get_mysql_socket(addr, kQuarantinedConnectTimeout, false);

    if (sock != -1) {
#ifndef _WIN32
      shutdown(sock, SHUT_RDWR);
      close(sock);
#else
      shutdown(sock, SD_BOTH);
      closesocket(sock);
#endif
      log_debug("Unquarantine destination server %s (index %d)", addr.str().c_str(), *it);
      std::lock_guard<std::mutex> lock(mutex_quarantine_);
      quarantined_.erase(std::remove(quarantined_.begin(), quarantined_.end(), *it));
    }
  }
}

void RouteDestination::quarantine_manager_thread() noexcept {
  mysql_harness::rename_thread("RtQ:<unknown>");  //TODO change <unknown> to instance name

  std::unique_lock<std::mutex> lock(mutex_quarantine_manager_);
  while (!stopping_) {
    condvar_quarantine_.wait_for(lock, std::chrono::seconds(kTimeoutQuarantineConditional),
                                 [this] { return !quarantined_.empty(); });

    if (!stopping_) {
      cleanup_quarantine();
      // Temporize
      std::this_thread::sleep_for(std::chrono::seconds(kQuarantineCleanupInterval));
    }
  }
}

size_t RouteDestination::size_quarantine() {
  std::lock_guard<std::mutex> lock(mutex_quarantine_);
  return quarantined_.size();
}
