#include "server_group.h"

#include <cstring>

static const size_t kExitPacketSize = 5;
static const uint8_t kExitPacket[] = {1, 0, 0, 0, 1};

ServerGroup::ServerGroup(const std::vector<int> &server_fds) {
  for (auto fd : server_fds) {
    server_conns_.emplace_back(fd, routing::RdmaOperations::instance());
    // Any positive number means ready
    has_outstanding_request_.push_back(false);
    read_results_.push_back(0);
  }
}

bool ServerGroup::Authenticate(Connection *client) {
  session_ = std::move(AuthenticateClient(client));
  if (session_.get() == nullptr) {
    return false;
  }
  int server_size = AuthWithBackendServers(session_.get(), &server_conns_[0]);
  if (server_size < 0) {
    log_error("Authentication fails with negative read size");
    server_conns_.clear();
    return false;
  }
  if (!mysql_is_ok_packet(server_conns_[0].Buffer())) {
    log_error("Server response is not OK");
  }
  for (auto it = server_conns_.begin() + 1; it != server_conns_.end(); it++) {
    int size = AuthWithBackendServers(session_.get(), &*it);
    if (size < 0) {
      log_error("Authentication fails with negative read size");
      server_conns_.clear();
      return false;
    }
  }
  log_debug("Done with authentication with all servers, sending first response back to client");
  ssize_t size = client->Send(server_conns_[0].Buffer(), server_size);
  if (size < 0) {
    log_error("Sending authentication result to client returns negative read size");
    server_conns_.clear();
    return false;
  }
  log_debug("Result sent back to client. Authentication done");
  return true;
}

int ServerGroup::Read(uint8_t *buffer, size_t size) {
  bool error = false;
  // We do a read on all servers, whether there's error or not
  for (size_t i = 1; i < server_conns_.size(); i++) {
    read_results_[i] = server_conns_[i].Recv();
    if (read_results_[i] <= 0) {
      error = true;
    } else {
      has_outstanding_request_[i] = false;
    }
  }
  int read_size = static_cast<int>(server_conns_[0].Recv());
  if (read_size < 0) {
    error = true;
  }
  has_outstanding_request_[0] = false;
  read_results_[0] = read_size;
  if (error) {
    return -1;
  } else {
    memcpy(buffer, server_conns_[0].Buffer(), read_size);
    return read_size;
  }
}

int ServerGroup::Write(uint8_t *buffer, size_t size) {
  bool error = false;
  for (size_t i = 0; i < server_conns_.size(); i++) {
    WaitForServer(i);
    if (server_conns_[i].Send(buffer, size) < 0) {
      error = true;
    } else {
      has_outstanding_request_[i] = true;
    }
  }
  if (error || IsExitPacket(buffer, size)) {
    return -1;
  } else {
    return static_cast<int>(size);
  }
}

std::pair<uint8_t*, size_t> ServerGroup::GetResult(size_t server_index) {
  if (has_outstanding_request_[server_index] || read_results_[server_index] <= 0) {
    return std::make_pair(nullptr, 0);
  }
  return std::make_pair(server_conns_[server_index].Buffer(),
                        static_cast<size_t>(read_results_[server_index]));
}

bool ServerGroup::SendQuery(size_t server_index, const std::string &query) {
  size_t payload_size = 1 + query.length();
  size_t packet_size = kMySQLHeaderLen + payload_size;
  uint8_t *buffer = server_conns_[server_index].Buffer();
  mysql_set_byte3(buffer, payload_size);
  buffer[kMySQLSeqOffset] = 0;
  uint8_t *payload = buffer + kMySQLHeaderLen;
  payload[0] = static_cast<uint8_t>(COM_QUERY);
  payload++;
  memcpy(payload, query.c_str(), query.length());
  payload[query.length()] = 0;
  has_outstanding_request_[server_index] = true;
  return server_conns_[server_index].Send(packet_size) > 0;
}

bool ServerGroup::Propagate(const std::string &query) {
  return Propagate(query, server_conns_.size());
}

bool ServerGroup::Propagate(const std::string &query, size_t source_write_server) {
  bool error = false;
  for (size_t i = 0; i < server_conns_.size(); i++) {
    if (i == source_write_server) {
      continue;
    }
    WaitForServer(i);
    if (!SendQuery(i, query)) {
      error = true;
    } else {
      has_outstanding_request_[i] = true;
    }
  }
  return !error;
}

bool ServerGroup::IsReadyForQuery(size_t server_index) {
  if (!has_outstanding_request_[server_index]) {
    return true;
  }
  auto res = server_conns_[server_index].TryRecv();
  read_results_[server_index] = res;
  if (res != -2) {
    has_outstanding_request_[server_index] = false;
    return true;
  } else {
    return false;
  }
}

bool ServerGroup::ForwardToAll(const std::string &query) {
  return Propagate(query, server_conns_.size());
}

int ServerGroup::GetAvailableServer() {
  for (size_t i = 0; i < server_conns_.size(); i++) {
    if (!has_outstanding_request_[i]) {
      return static_cast<int>(i);
    }
  }
  bool response = false;
  int responded_server = -1;
  while (!response) {
    for (size_t i = 0; i < server_conns_.size(); i++) {
      ssize_t read_res = server_conns_[i].TryRecv();
      read_results_[i] = read_res;
      if (read_res > 0) {
        response = true;
        has_outstanding_request_[i] = false;
        responded_server = static_cast<int>(i);
        break;
      } else if (read_res != -2) {
        response = true;
        has_outstanding_request_[i] = false;
        responded_server = -1;
        break;
      }
    }
  }
  return responded_server;
}

void ServerGroup::WaitForServer(size_t server_index) {
  auto &conn = server_conns_[server_index];
  while (has_outstanding_request_[server_index]) {
    auto read_res = conn.TryRecv();
    read_results_[server_index] = read_res;
    if (read_res != -2) {
      has_outstanding_request_[server_index] = false;
      return;
    }
  }
}

bool ServerGroup::IsExitPacket(uint8_t *buffer, size_t size) {
  if (size != kExitPacketSize) {
    return false;
  }
  return memcmp(buffer, kExitPacket, kExitPacketSize) == 0;
}
