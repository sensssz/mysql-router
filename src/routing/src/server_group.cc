#include "server_group.h"

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
  for (auto it = server_conns_.begin() + 1; it != server_conns_.end(); it++) {
    if (it->Recv() <= 0) {
      error = true;
    }
  }
  int read_size = static_cast<int>(server_conns_[0].Recv());
  if (read_size < 0) {
    error = true;
  }
  if (error) {
    return -1;
  } else {
    memcpy(buffer, server_conns_[0].Buffer(), read_size);
    return read_size;
  }
}

int ServerGroup::Write(uint8_t *buffer, size_t size) {
  bool error = false;
  for (auto &conn : server_conns_) {
    if (conn.Send(buffer, size) < 0) {
      error = true;
    }
  }
  if (error) {
    return -1;
  } else {
    return static_cast<int>(size);
  }
}

uint8_t *ServerGroup::GetResult(size_t server_index) {
  if (has_outstanding_request_[server_index] || read_results_[server_index] <= 0) {
    return nullptr;
  }
  return server_conns_[server_index].Buffer();
}

bool ServerGroup::SendQuery(size_t server_index, const std::string &query) {
  uint8_t *payload = server_conns_[server_index].Payload();
  payload[0] = static_cast<uint8_t>(COM_QUERY);
  payload++;
  memcpy(payload, query.c_str(), query.length());
  has_outstanding_request_[server_index] = true;
  return server_conns_[server_index].Send(query.length() + 1) > 0;
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
  bool error = false;
  for (size_t i = 0; i < server_conns_.size(); i++) {
    if (!SendQuery(i, query)) {
      error = true;
    } else {
      has_outstanding_request_[i] = true;
    }
  }
  return !error;
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
