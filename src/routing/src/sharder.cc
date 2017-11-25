#include "sharder.h"

Sharder::Sharder(std::vector<int> &&server_fds) : server_fds_(std::move(server_fds)) {}

Sharder::Sharder(const std::vector<int> &server_fds) : server_fds_(server_fds) {}

Sharder::~Sharder() {
  DisconnectServers();
}

int Sharder::GetShard(const std::string &column, int key) {
  return server_fds_[0];
}

bool Sharder::Authenticate(int client_fd) {
  session_ = std::move(AuthenticateClient(client_fd));
  auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[kMySQLMaxPacketLen]);
  auto buf = buffer.get();
  int server_size = AuthWithBackendServers(session_.get(), server_fds_[0], buf, kMySQLMaxPacketLen);
  if (server_size < 0) {
    log_error("Authentication fails with negative read size");
    DisconnectServers();
    return false;
  }
  for (auto it = server_fds_.begin() + 1; it != server_fds_.end(); it++) {
    int size = AuthWithBackendServers(session_.get(), *it, nullptr, 0);
    if (size < 0) {
      log_error("Authentication fails with negative read size");
      DisconnectServers();
      return false;
    }
  }
  ssize_t size = routing::SocketOperations::instance()->write(client_fd, buf, server_size);
  if (size < 0) {
    log_error("Sending authentication result to client returns negative read size");
    DisconnectServers();
    return false;
  }
  return true;
}

int Sharder::Read(uint8_t *buffer, size_t size) {
  auto rdma_operations = routing::RdmaOperations::instance();
  bool error = false;
  // We do a read on all servers, whether there's error or not
  for (auto it = server_fds_.begin() + 1; it != server_fds_.end(); it++) {
    if (rdma_operations->read(*it, buffer, size)) {
      error = true;
    }
  }
  int read_size = static_cast<int>(rdma_operations->read(server_fds_[0], buffer, size));
  if (read_size < 0) {
    error = true;
  }
  if (error) {
    return -1;
  } else {
    return read_size;
  }
}

int Sharder::Write(uint8_t *buffer, size_t size) {
  auto rdma_operations = routing::RdmaOperations::instance();
  bool error = false;
  for (auto fd : server_fds_) {
    if (rdma_operations->write(fd, buffer, size) < 0) {
      error = true;
    }
  }
  if (error) {
    return -1;
  } else {
    return static_cast<int>(size);
  }
}

void Sharder::DisconnectServers() {
  auto rdma_operations = routing::RdmaOperations::instance();
  for (auto fd : server_fds_) {
    rdma_operations->close(fd);
  }
  server_fds_.clear();
}
