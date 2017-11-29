#include "server_group.h"

ServerGroup::ServerGroup(const std::vector<int> &server_fds) {
  for (auto fd : server_fds) {
    server_conns_.emplace_back(fd, routing::RdmaOperations::instance());
    ready_for_write_.push_back(true);
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
  ssize_t size = client->Send(server_conns_[0].Buffer(), server_size);
  if (size < 0) {
    log_error("Sending authentication result to client returns negative read size");
    server_conns_.clear();
    return false;
  }
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

bool ServerGroup::SendQuery(int server_index, const std::string &query) {
  return true;
}

bool ServerGroup::ForwardToAll(const std::string &query) {
  return true;
}
