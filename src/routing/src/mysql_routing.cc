/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
#ifdef _WIN32
#  define NOMINMAX
#endif

#include "common.h"
#include "dest_first_available.h"
#include "dest_metadata_cache.h"
#include "logger.h"
#include "mysql_routing.h"
#include "mysqlrouter/metadata_cache.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/uri.h"
#include "mysqlrouter/utils.h"
#include "plugin_config.h"
#include "protocol/protocol.h"
#include "speculator/fake_speculator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>

#include <cstdlib>
#include <sys/types.h>

#ifdef _WIN32
/* before winsock inclusion */
#  define FD_SETSIZE 4096
#else
#  undef __FD_SETSIZE
#  define __FD_SETSIZE 4096
#endif

#ifndef _WIN32
#  include <netinet/in.h>
#  include <fcntl.h>
#  include <sys/un.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#else
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif

using std::runtime_error;
using std::string;
using mysql_harness::get_strerror;
using mysqlrouter::string_format;
using mysqlrouter::to_string;
using routing::AccessMode;
using mysqlrouter::URI;
using mysqlrouter::URIError;
using mysqlrouter::URIQuery;
using mysqlrouter::TCPAddress;
using mysqlrouter::is_valid_socket_name;

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

namespace {

int kListenQueueSize = 1024;

const char *kDefaultReplicaSetName = "default";
const int kAcceptorStopPollInterval_ms = 1000;
const int kNumIndexDigits = 10;

double Mean(std::vector<long> &latencies) {
  double mean = 0;
  double i = 1;
  for (auto latency : latencies) {
    mean += (latency - mean) / i;
    i++;
  }
  return mean;
}

void DumpWaits(std::vector<int> &indices, std::vector<long> &wait_times) {
  std::ofstream waits("wait_queries");
  for (size_t i = 0; i < indices.size(); i++) {
    waits << indices[i] << ',' << wait_times[i] << std::endl;
  }
}

bool IsQuery(uint8_t *buffer) {
  return buffer[kMySQLHeaderLen] == static_cast<uint8_t>(COM_QUERY);
}

std::pair<int, std::string> ExtractQuery(uint8_t *buffer) {
  size_t query_size = mysql_get_byte3(buffer) - 1;
  char *query = reinterpret_cast<char *>(buffer + kMySQLHeaderLen + 1);
  int query_index = -1;
  if (isdigit(query[0])) {
    query[kNumIndexDigits - 1] = '\0';
    query_index = atoi(query);
    query += kNumIndexDigits;
    query_size -= kNumIndexDigits;
  }
  return std::make_pair(query_index, std::string(query, query_size));
}

std::string ToLower(const std::string &query) {
  std::string lower = query;
  std::transform(lower.begin(), lower.end(), lower.begin(), tolower);
  return std::move(lower);
}

bool IsRead(const std::string &query) {
  auto lower = ToLower(query);
  return lower.find("select") == 0 || lower.find("show") == 0;
}

bool IsWrite(const std::string &query) {
  return !IsRead(query);
}

TimePoint Now() {
  return std::chrono::high_resolution_clock::now();
}

long GetDuration(TimePoint &start) {
  auto end = Now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return static_cast<long>(duration.count());
}

long GetDuration(TimePoint &start, TimePoint &end) {
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return static_cast<long>(duration.count());
}

// The reserved_server is necessary because there may be a gap between
// checking the result has arrived and the checks below.
bool DoSpeculation(
  const std::string &query,
  ServerGroup *server_group,
  int reserved_server,
  Speculator *speculator,
  std::vector<long> &speculation_wait_time,
  std::unordered_map<std::string, int> &prefetches) {
  prefetches.clear();
  auto speculations = speculator->Speculate(query);
  if (speculations.size() == 0) {
    return true;
  }
  std::set<int> servers_in_use;
  servers_in_use.insert(reserved_server);
  bool done = false;
  for (const auto &speculation : speculations) {
    done = false;
    auto start = std::chrono::high_resolution_clock::now();
    while (!done) {
      for (size_t i = 0; i < server_group->Size(); i++) {
        auto index = static_cast<int>(i);
        if (servers_in_use.find(index) != servers_in_use.end() ||
            !server_group->IsReadyForQuery(i)) {
          continue;
        }
        if (!server_group->SendQuery(i, speculation)) {
          return false;
        }
        prefetches[speculation] = index;
        servers_in_use.insert(index);
        done = true;
        break;
      }
    }
    speculation_wait_time.push_back(GetDuration(start));
  }
  return true;
}

size_t CopyToClient(std::pair<uint8_t*, size_t> &&result, Connection *client) {
  memcpy(client->Buffer(), result.first, result.second);
  return result.second;
}

} // namespace

MySQLRouting::MySQLRouting(routing::AccessMode mode, uint16_t port,
                           const Protocol::Type protocol,
                           const string &bind_address,
                           const mysql_harness::Path& named_socket,
                           const string &route_name,
                           int max_connections,
                           int destination_connect_timeout,
                           unsigned long long max_connect_errors,
                           unsigned int client_connect_timeout,
                           unsigned int net_buffer_length,
                           SocketOperationsBase *socket_operations,
                           SocketOperationsBase *rdma_operations)
    : name(route_name),
      mode_(mode),
      max_connections_(set_max_connections(max_connections)),
      destination_connect_timeout_(set_destination_connect_timeout(destination_connect_timeout)),
      max_connect_errors_(max_connect_errors),
      client_connect_timeout_(client_connect_timeout),
      net_buffer_length_(net_buffer_length),
      bind_address_(TCPAddress(bind_address, port)),
      bind_named_socket_(named_socket),
      service_tcp_(0),
      service_named_socket_(0),
      speculator_(new FakeSpeculator("/users/POTaDOS/SQP/lobsters.sql")),
      stopping_(false),
      info_active_routes_(0),
      info_handled_routes_(0),
      socket_operations_(socket_operations),
      rdma_operations_(rdma_operations),
      protocol_(Protocol::create(protocol, socket_operations, rdma_operations)) {

  assert(socket_operations_ != nullptr);

  #ifdef _WIN32
  if (named_socket.is_set()) {
    throw std::invalid_argument(string_format("'socket' configuration item is not supported on Windows platform"));
  }
  #endif

  // This test is only a basic assertion.  Calling code is expected to check the validity of these arguments more thoroughally.
  // At the time of writing, routing_plugin.cc : init() is one such place.
  if (!bind_address_.port && !named_socket.is_set()) {
    throw std::invalid_argument(string_format("No valid address:port (%s:%d) or socket (%s) to bind to", bind_address.c_str(), port, named_socket.c_str()));
  }
}

bool MySQLRouting::block_client_host(const std::array<uint8_t, 16> &client_ip_array,
                                     const string &client_ip_str, int server) {
  bool blocked = false;
  {
    std::lock_guard<std::mutex> lock(mutex_conn_errors_);

    if (++conn_error_counters_[client_ip_array] >= max_connect_errors_) {
      log_warning("[%s] blocking client host %s", name.c_str(), client_ip_str.c_str());
      blocked = true;
    } else {
      log_info("[%s] %d connection errors for %s (max %u)",
               name.c_str(), conn_error_counters_[client_ip_array], client_ip_str.c_str(), max_connect_errors_);
    }
  }

  if (server >= 0) {
    protocol_->on_block_client_host(server, name);
  }

  return blocked;
}

const std::vector<std::array<uint8_t, 16>> MySQLRouting::get_blocked_client_hosts() const {
  std::lock_guard<std::mutex> lock(mutex_conn_errors_);

  std::vector<std::array<uint8_t, 16>> result;
  for(const auto& client_ip: conn_error_counters_) {
    if (client_ip.second >= max_connect_errors_) {
      result.push_back(client_ip.first);
    }
  }

  return result;
}

/*static*/
std::string MySQLRouting::make_thread_name(const std::string& config_name, const std::string& prefix) {

  const char* p = config_name.c_str();

  // at the time of writing, config_name starts with:
  //   "routing:<config_from_conf_file>" (with key)
  // or with:
  //   "routing" (without key).
  // Verify this assumption
  constexpr char kRouting[] = "routing";
  size_t kRoutingLen = sizeof(kRouting) - 1;  // -1 to ignore string terminator
  if (memcmp(p, kRouting, kRoutingLen))
    return prefix + ":parse err";

  // skip over "routing[:]"
  p += kRoutingLen;
  if (*p == ':')
    p++;

  // at the time of writing, bootstrap generates 4 routing configurations by default,
  // which will result in <config_from_conf_file> having one of below 4 values:
  //   "<cluster_name>_default_ro",   "<cluster_name>_default_rw",
  //   "<cluster_name>_default_x_ro", "<cluster_name>_default_x_rw"
  // since we're limited to 15 chars for thread name, we skip over
  // "<cluster_name>_default_" so that suffixes ("x_ro", etc) can fit
  std::string key = p;
  const char kPrefix[] = "_default_";
  if (key.find(kPrefix) != key.npos) {
    key = key.substr(key.find(kPrefix) + sizeof(kPrefix) - 1);  // -1 for string terminator
  }

  // now put everything together
  std::string thread_name = prefix + ":" + key;
  thread_name.resize(15); // max for pthread_setname_np()

  return thread_name;
}

void MySQLRouting::routing_select_thread(int client, const sockaddr_storage& client_addr) noexcept {
  mysql_harness::rename_thread(make_thread_name(name, "RtS").c_str());  // "Rt select() thread" would be too long :(

  // int nfds;
  // int res;
  // int error = 0;
  ssize_t bytes_down = 0;
  ssize_t bytes_up = 0;
  ssize_t bytes_read = 0;
  string extra_msg = "";
  RoutingProtocolBuffer buffer(net_buffer_length_);
  auto buffer_length = buffer.size();
  bool handshake_done = false;
  Connection client_connection(client, routing::SocketOperations::instance());
  std::unordered_map<std::string, int> prefetches;
  bool has_begun = false;
  size_t num_reads = 0;
  size_t num_misses = 0;
  size_t num_instants = 0;
  size_t num_waits = 0;
  std::vector<long> think_time;
  std::vector<long> query_process_time;
  std::vector<long> hit_time;
  std::vector<long> miss_time;
  std::vector<long> speculation_time;
  std::vector<long> query_wait_time;
  std::vector<long> wait_think_time;
  std::vector<int> wait_queries;
  std::vector<long> speculation_wait_time;
  std::vector<long> network_latency;
  std::chrono::time_point<std::chrono::high_resolution_clock> prev_query;

  auto server_group = destination_->GetServerGroup();
  if (server_group.get() == nullptr) {
    return;
  }

  std::cerr << "Initiate authentication" << std::endl;
  if (!server_group->Authenticate(&client_connection)) {
    return;
  }
  handshake_done = true;

  // int server = destination_->get_server_socket(destination_connect_timeout_, &error);
  int server = 1;

  if (!(server > 0 && client > 0)) {
    std::stringstream os;
    os << "Can't connect to remote MySQL server for client '"
      << bind_address_.addr << ":" << bind_address_.port << "'";

    log_warning("[%s] %s", name.c_str(), os.str().c_str());

    // at this point, it does not matter whether client gets the error
    protocol_->send_error(client, 2003, os.str(), "HY000", name);

    socket_operations_->shutdown(client);
    socket_operations_->shutdown(server);

    if (client > 0) {
      socket_operations_->close(client);
    }
    if (server > 0) {
      socket_operations_->close(server);
    }
    return;
  }

  std::pair<std::string, int> c_ip = get_peer_name(client);
  std::pair<std::string, int> s_ip = get_peer_name(server);

  std::string info;
  if (c_ip.second == 0) {
    // Unix socket/Windows Named pipe
    info = string_format("[%s] source %s - dest [%s]:%d",
                         name.c_str(), bind_named_socket_.c_str(),
                         s_ip.first.c_str(), s_ip.second);
  } else {
    info = string_format("[%s] source [%s]:%d - dest [%s]:%d",
                         name.c_str(), c_ip.first.c_str(), c_ip.second,
                         s_ip.first.c_str(), s_ip.second);
  }
  log_debug(info.c_str());

  ++info_active_routes_;
  ++info_handled_routes_;

  int pktnr = 0;
  while (true) {
    log_debug("Reading packet from the client...");
    bytes_read = client_connection.Recv();
    if (bytes_read <= 0) {
      log_error("Read from client fails");
      break;
    }

    if (num_reads > 0 && num_reads % 100 == 0) {
      std::cerr << "Reads\t\tMisses\t\tinstants\t\tWaits" << std::endl;
      std::cerr << num_reads << "\t\t" << num_misses << "\t\t" << num_instants << "\t\t" << num_waits << std::endl;
      std::cerr << "Think\t\tQuery Process\t\tHit\t\tMiss\t\tSpeculation\t\tQuery Wait\t\tWait Think\t\tNetwork Latency" << std::endl;
      std::cerr << Mean(think_time) << "\t\t" << Mean(query_process_time) << "\t\t" << Mean(hit_time) << "\t\t" << Mean(miss_time) << "\t\t" << Mean(speculation_time) << "\t\t" << Mean(query_wait_time) << "\t\t" << Mean(wait_think_time) << "\t\t" << Mean(network_latency) << std::endl;
      std::cerr << think_time.size() << "\t\t" << query_process_time.size() << "\t\t" << hit_time.size() << "\t\t" << miss_time.size() << "\t\t" << speculation_time.size() << "\t\t" << query_wait_time.size() << "\t\t" << wait_think_time.size() << "\t\t" << network_latency.size() << std::endl;
      std::cerr << std::endl;
    }

    if (::IsQuery(client_connection.Buffer())) {
      auto pair = ::ExtractQuery(client_connection.Buffer());
      int query_index = pair.first;
      if (query_index == 6399) {
        DumpWaits(wait_queries, query_wait_time);
      }
      std::string query = pair.second;
      speculator_->CheckBegin(query);
      speculator_->SetQueryIndex(query_index);
      has_begun = has_begun || query == "BEGIN";
      log_debug("Query is %s", query.c_str());
      size_t packet_size = 0;
      if (::IsWrite(query)) {
        prev_query = Now();
        log_debug("Got write query, forward it to all servers...");
        if (!server_group->ForwardToAll(query)) {
          log_error("Failed to forward query to servers");
          break;
        }
        log_debug("Query forwarded to all servers, waiting for one of them to be available");
        auto first_available = server_group->GetAvailableServer();
        log_debug("Got server %d to read result from", first_available);
        if (first_available < 0) {
          log_error("Failed to get available server");
          break;
        }
        packet_size = ::CopyToClient(server_group->GetResult(first_available), &client_connection);
        log_debug("Do speculations");
        if (!::DoSpeculation(query, server_group.get(), -1,
                             speculator_.get(), speculation_wait_time, prefetches)) {
          log_error("Failed to do speculations");
          break;
        }
        log_debug("Send results back to client");
        if (client_connection.Send(packet_size) <= 0) {
          log_error("Write to client fails");
          break;
        }
        bytes_down += packet_size;
      } else {
        // Ideally we want to send out all speculative queries and then
        // check if our prediction hits for the current query. But we want
        // to make sure that if prediction does not hit, the current query
        // gets the first available server. Also if the prediction hits and
        // the result has already been retrieved, then that server can be
        // reused to process the speculative query. Therefore, we first check
        // for prediction hit, and then check whether the result has arrived.
        auto query_process_start = Now();
        if (has_begun) {
          think_time.push_back(GetDuration(prev_query, query_process_start));
          num_reads++;
        }
        log_debug("Got read query");
        auto iter = prefetches.find(query);
        int server_for_current_query = -1;
        if (iter != prefetches.end()) {
          auto hit_start = Now();
          log_debug("Prediction hits, check for result");
          if (server_group->IsReadyForQuery(iter->second)) {
            // Result has been received
            log_debug("Result has already arrived");
            packet_size = ::CopyToClient(server_group->GetResult(iter->second), &client_connection);
            if (has_begun) {
              num_instants++;
            }
          } else {
            log_debug("Result is pending");
            server_for_current_query = iter->second;
          }
          if (has_begun) {
            hit_time.push_back(GetDuration(hit_start));
          }
        } else {
          auto miss_start = Now();
          if (has_begun) {
            num_misses++;
          }
          // Prediction not hit, send it now.
          server_for_current_query = server_group->GetAvailableServer();
          log_debug("Prediction fails, execute it on server %d", server_for_current_query);
          if (server_for_current_query < 0) {
            log_error("Failed to get available server");
            break;
          }
          if (!server_group->SendQuery(server_for_current_query, query)) {
            log_error("Failed to send query to server");
            break;
          }
          if (has_begun) {
            miss_time.push_back(GetDuration(miss_start));
          }
        }
        auto speculation_start = Now();
        log_debug("Do speculations");
        if (!::DoSpeculation(query, server_group.get(), server_for_current_query,
                             speculator_.get(), speculation_wait_time, prefetches)) {
          log_error("Failed to do speculations");
          break;
        }
        if (has_begun) {
          speculation_time.push_back(GetDuration(speculation_start));
        }
        // Now either wait for the result or we already have the result
        if (server_for_current_query != -1) {
          auto query_wait_start = Now();
          if (has_begun) {
            num_waits++;
            wait_queries.push_back(query_index);
            wait_think_time.push_back(think_time.back());
          }
          log_debug("Waiting for pending result");
          while (!server_group->IsReadyForQuery(server_for_current_query)) {
            ;
          }
          log_debug("Result has arrived");
          packet_size = ::CopyToClient(server_group->GetResult(server_for_current_query), &client_connection);
          if (has_begun) {
            query_wait_time.push_back(GetDuration(query_wait_start));
          }
        }
        log_debug("Send result back to client");
        if (has_begun) {
          query_process_time.push_back(GetDuration(query_process_start));
        }
        auto network_start = Now();
        if (client_connection.Send(packet_size) <= 0) {
          log_error("Write to client fails");
          break;
        }
        if (has_begun) {
          network_latency.push_back(GetDuration(network_start));
        }
        bytes_down += packet_size;
      }
    } else {
      log_debug("Forwarding packet to the server...");
      if (server_group->Write(client_connection.Buffer(), bytes_read) <= 0) {
        log_error("Write to servers fails");
        break;
      }
      bytes_up += bytes_read;

      log_debug("Reading packet from the server...");
      bytes_read = server_group->Read(client_connection.Buffer(), Connection::kBufferSize);
      if (bytes_read <= 0) {
        log_error("Read from servers fail");
        break;
      }
      log_debug("Forwarding packet back to client...");
      if (client_connection.Send(bytes_read) <= 0) {
        log_error("Write to client fails");
        break;
      }
      log_debug("Result sent back");
      bytes_down += bytes_read;
    }
  } // while (true)

  client_connection.Disconnect();

  if (!handshake_done) {
    auto ip_array = in_addr_to_array(client_addr);
    log_debug("[%s] Routing failed for %s: %s", name.c_str(), c_ip.first.c_str(), extra_msg.c_str());
    block_client_host(ip_array, c_ip.first.c_str(), server);
  }

  --info_active_routes_;
#ifndef _WIN32
  log_debug("[%s] Routing stopped (up:%zub;down:%zub) %s", name.c_str(), bytes_up, bytes_down, extra_msg.c_str());
#else
  log_debug("[%s] Routing stopped (up:%Iub;down:%Iub) %s", name.c_str(), bytes_up, bytes_down, extra_msg.c_str());
#endif
}

void MySQLRouting::start() {

  mysql_harness::rename_thread(make_thread_name(name, "RtM").c_str());  // "Rt main" would be too long :(
  if (bind_address_.port > 0) {
    try {
      setup_tcp_service();
    } catch (const runtime_error &exc) {
      stop();
      throw runtime_error(
          string_format("Setting up TCP service using %s: %s", bind_address_.str().c_str(), exc.what()));
    }
    log_info("[%s] started: listening on %s; %s", name.c_str(), bind_address_.str().c_str(),
             routing::get_access_mode_name(mode_).c_str());
  }
#ifndef _WIN32
  if (bind_named_socket_.is_set()) {
    try {
      setup_named_socket_service();
    } catch (const runtime_error &exc) {
      stop();
      throw runtime_error(
          string_format("Setting up named socket service '%s': %s", bind_named_socket_.c_str(), exc.what()));
    }
    log_info("[%s] started: listening using %s; %s", name.c_str(), bind_named_socket_.c_str(),
             routing::get_access_mode_name(mode_).c_str());
  }
#endif
  if (bind_address_.port > 0 || bind_named_socket_.is_set()) {
    //XXX this thread seems unnecessary, since we block on it right after anyway
    thread_acceptor_ = std::thread(&MySQLRouting::start_acceptor, this);
    if (thread_acceptor_.joinable()) {
      thread_acceptor_.join();
    }
#ifndef _WIN32
    if (bind_named_socket_.is_set() && unlink(bind_named_socket_.str().c_str()) == -1) {
      if (errno != ENOENT)
        log_warning(("Failed removing socket file " + bind_named_socket_.str() + " (" + get_strerror(errno) + " (" + to_string(errno) + "))").c_str());
    }
#endif
  }
}

void MySQLRouting::start_acceptor() {
  mysql_harness::rename_thread(make_thread_name(name, "RtA").c_str());  // "Rt Acceptor" would be too long :(

  int sock_client;
  struct sockaddr_storage client_addr;
  socklen_t sin_size = static_cast<socklen_t>(sizeof client_addr);
  int opt_nodelay = 1;
  int nfds = 0;

  destination_->start();

  if (service_tcp_ > 0) {
    routing::set_socket_blocking(service_tcp_, false);
  }
  if (service_named_socket_ > 0) {
    routing::set_socket_blocking(service_named_socket_, false);
  }
  nfds = std::max(service_tcp_, service_named_socket_) + 1;
  fd_set readfds;
  fd_set errfds;
  struct timeval timeout_val;
  while (!stopping()) {
    // Reset on each loop
    FD_ZERO(&readfds);
    FD_ZERO(&errfds);
    if (service_tcp_ > 0) {
      FD_SET(service_tcp_, &readfds);
    }
    if (service_named_socket_ > 0) {
      FD_SET(service_named_socket_, &readfds);
    }
    timeout_val.tv_sec = ::kAcceptorStopPollInterval_ms / 1000;
    timeout_val.tv_usec = (::kAcceptorStopPollInterval_ms % 1000) * 1000;
    int ready_fdnum = select(nfds, &readfds, nullptr, &errfds, &timeout_val);
    if (ready_fdnum <= 0) {
      if (ready_fdnum == 0) {
        // timeout - just check if stopping and continue
        continue;
      } else if (errno > 0) {
        if (errno == EINTR || errno == EAGAIN)
          continue;
        log_error("[%s] Select failed with error: %s", name.c_str(), get_strerror(errno).c_str());
        break;
  #ifdef _WIN32
      } else if (WSAGetLastError() > 0) {
        log_error("[%s] Select failed with error: %s", name.c_str(), get_message_error(WSAGetLastError()));
  #endif
        break;
      } else {
        log_error("[%s] Select failed (%i)", name.c_str(), errno);
        break;
      }
    }
    while (ready_fdnum > 0) {
      bool is_tcp = false;
      if (FD_ISSET(service_tcp_, &readfds)) {
        FD_CLR(service_tcp_, &readfds);
        --ready_fdnum;
        if ((sock_client = accept(service_tcp_, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
          log_error("[%s] Failed accepting TCP connection: %s", name.c_str(), get_message_error(errno).c_str());
          continue;
        }
        is_tcp = true;
        log_debug("[%s] TCP connection from %i accepted at %s", name.c_str(),
                  sock_client, bind_address_.str().c_str());
      }
      if (FD_ISSET(service_named_socket_, &readfds)) {
        FD_CLR(service_named_socket_, &readfds);
        --ready_fdnum;
        if ((sock_client = accept(service_named_socket_, (struct sockaddr *) &client_addr, &sin_size)) < 0) {
          log_error("[%s] Failed accepting socket connection: %s", name.c_str(), get_message_error(errno).c_str());
          continue;
        }
        log_debug("[%s] UNIX socket connection from %i accepted at %s", name.c_str(),
                  sock_client, bind_address_.str().c_str());
      }

      if (conn_error_counters_[in_addr_to_array(client_addr)] >= max_connect_errors_) {
        std::stringstream os;
        os << "Too many connection errors from " << get_peer_name(sock_client).first;
        protocol_->send_error(sock_client, 1129, os.str(), "HY000", name);
        log_info("%s", os.str().c_str());
        socket_operations_->close(sock_client); // no shutdown() before close()
        continue;
      }

      if (info_active_routes_.load(std::memory_order_relaxed) >= max_connections_) {
        protocol_->send_error(sock_client, 1040, "Too many connections", "HY000", name);
        socket_operations_->close(sock_client); // no shutdown() before close()
        log_warning("[%s] reached max active connections (%d max=%d)", name.c_str(),
                   info_active_routes_.load(), max_connections_);
        continue;
      }

      if (is_tcp && setsockopt(sock_client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&opt_nodelay), static_cast<socklen_t>(sizeof(int))) == -1) {
        log_error("[%s] client setsockopt error: %s", name.c_str(), get_message_error(errno).c_str());
        continue;
      }

      std::thread(&MySQLRouting::routing_select_thread, this, sock_client, client_addr).detach();
    }
  } // while (!stopping())
  log_info("[%s] stopped", name.c_str());
}

void MySQLRouting::stop() {
  stopping_.store(true);
}

void MySQLRouting::setup_tcp_service() {
  struct addrinfo *servinfo, *info, hints;
  int err;
  int option_value;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  errno = 0;

  err = getaddrinfo(bind_address_.addr.c_str(), to_string(bind_address_.port).c_str(), &hints, &servinfo);
  if (err != 0) {
    throw runtime_error(string_format("[%s] Failed getting address information (%s)",
                                      name.c_str(), gai_strerror(err)));
  }

  // Try to setup socket and bind
  for (info = servinfo; info != nullptr; info = info->ai_next) {
    if ((service_tcp_ = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
       // in windows, WSAGetLastError() will be called by get_message_error()
      std::string error = get_message_error(errno);
      freeaddrinfo(servinfo);
      throw std::runtime_error(error);
    }

#ifndef _WIN32
    option_value = 1;
    if (setsockopt(service_tcp_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&option_value),
            static_cast<socklen_t>(sizeof(int))) == -1) {
      std::string error = get_message_error(errno);
      freeaddrinfo(servinfo);
      socket_operations_->close(service_tcp_);
      throw std::runtime_error(error);
    }
#endif

    if (::bind(service_tcp_, info->ai_addr, info->ai_addrlen) == -1) {
      std::string error = get_message_error(errno);
      freeaddrinfo(servinfo);
      socket_operations_->close(service_tcp_);
      throw std::runtime_error(error);
    }
    break;
  }
  freeaddrinfo(servinfo);

  if (info == nullptr) {
    throw runtime_error(string_format("[%s] Failed to setup server socket", name.c_str()));
  }

  if (listen(service_tcp_, ::kListenQueueSize) < 0) {
    throw runtime_error(string_format("[%s] Failed to start listening for connections using TCP", name.c_str()));
  }
}

#ifndef _WIN32
void MySQLRouting::setup_named_socket_service() {
  struct sockaddr_un sock_unix;
  string socket_file = bind_named_socket_.str();
  errno = 0;

  assert(!socket_file.empty());

  std::string error_msg;
  if (!is_valid_socket_name(socket_file, error_msg)) {
    throw std::runtime_error(error_msg);
  }

  if ((service_named_socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    throw std::invalid_argument(get_strerror(errno));
  }

  sock_unix.sun_family = AF_UNIX;
  std::strncpy(sock_unix.sun_path, socket_file.c_str(), socket_file.size() + 1);

retry:
  if (::bind(service_named_socket_, (struct sockaddr *) &sock_unix, static_cast<socklen_t>(sizeof(sock_unix))) == -1) {
    int save_errno = errno;
    if (errno == EADDRINUSE) {
      // file exists, try to connect to it to see if the socket is already in use
      if (::connect(service_named_socket_,
                    (struct sockaddr *) &sock_unix, static_cast<socklen_t>(sizeof(sock_unix))) == 0) {
        log_error("Socket file %s already in use by another process", socket_file.c_str());
        throw std::runtime_error("Socket file already in use");
      } else {
        if (errno == ECONNREFUSED) {
          log_warning("Socket file %s already exists, but seems to be unused. Deleting and retrying...", socket_file.c_str());
          if (unlink(socket_file.c_str()) == -1) {
            if (errno != ENOENT) {
              log_warning(("Failed removing socket file " + socket_file + " (" + get_strerror(errno) + " (" + to_string(errno) + "))").c_str());
              throw std::runtime_error(
                  "Failed removing socket file " + socket_file + " (" + get_strerror(errno) + " (" + to_string(errno) + "))");
            }
          }
          errno = 0;
          socket_operations_->close(service_named_socket_);
          if ((service_named_socket_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw std::runtime_error(get_strerror(errno));
          }
          goto retry;
        } else {
          errno = save_errno;
        }
      }
    }
    log_error("Error binding to socket file %s: %s", socket_file.c_str(), get_strerror(errno).c_str());
    throw std::runtime_error(get_strerror(errno));
  }

  if (listen(service_named_socket_, kListenQueueSize) < 0) {
    throw runtime_error("Failed to start listening for connections using named socket");
  }
}
#endif

void MySQLRouting::set_destinations_from_uri(const URI &uri) {
  if (uri.scheme == "metadata-cache") {
    // Syntax: metadata_cache://[<metadata_cache_key(unused)>]/<replicaset_name>?role=PRIMARY|SECONDARY
    std::string replicaset_name = ::kDefaultReplicaSetName;
    std::string role;

    if (uri.path.size() > 0 && !uri.path[0].empty())
      replicaset_name = uri.path[0];
    if (uri.query.find("role") == uri.query.end())
      throw runtime_error("Missing 'role' in routing destination specification");

    destination_.reset(new DestMetadataCacheGroup(uri.host, replicaset_name,
                                                  get_access_mode_name(mode_),
                                                  uri.query, protocol_->get_type()));
  } else {
    throw runtime_error(string_format("Invalid URI scheme; expecting: 'metadata-cache' is: '%s'",
                                      uri.scheme.c_str()));
  }
}

void MySQLRouting::set_destinations_from_csv(const string &csv) {
  std::stringstream ss(csv);
  std::string part;
  std::pair<std::string, uint16_t> info;


  if (AccessMode::kReadOnly == mode_) {
    destination_.reset(new RouteDestination(protocol_->get_type(), rdma_operations_));
  } else if (AccessMode::kReadWrite == mode_) {
    destination_.reset(new DestFirstAvailable(protocol_->get_type(), rdma_operations_));
  } else {
    throw std::runtime_error("Unknown mode");
  }
  // Fall back to comma separated list of MySQL servers
  while (std::getline(ss, part, ',')) {
    info = mysqlrouter::split_addr_port(part);
    if (info.second == 0) {
      info.second = Protocol::get_default_port(protocol_->get_type());
    }
    TCPAddress addr(info.first, info.second);
    if (addr.is_valid()) {
      destination_->add(addr);
    } else {
      throw std::runtime_error(string_format("Destination address '%s' is invalid", addr.str().c_str()));
    }
  }

  // Check whether bind address is part of list of destinations
  for (auto &it: *destination_) {
    if (it == bind_address_) {
      throw std::runtime_error("Bind Address can not be part of destinations");
    }
  }

  if (destination_->size() == 0) {
    throw std::runtime_error("No destinations available");
  }
}

void MySQLRouting::set_root_password(const std::string &root_password) {
  root_password_ = root_password;
}

int MySQLRouting::set_destination_connect_timeout(int seconds) {
  if (seconds <= 0 || seconds > UINT16_MAX) {
    auto err = string_format("[%s] tried to set destination_connect_timeout using invalid value, was '%d'",
                             name.c_str(), seconds);
    throw std::invalid_argument(err);
  }
  destination_connect_timeout_ = seconds;
  return destination_connect_timeout_;
}

int MySQLRouting::set_max_connections(int maximum) {
  if (maximum <= 0 || maximum > UINT16_MAX) {
    auto err = string_format("[%s] tried to set max_connections using invalid value, was '%d'", name.c_str(),
                             maximum);
    throw std::invalid_argument(err);
  }
  max_connections_ = maximum;
  return max_connections_;
}
