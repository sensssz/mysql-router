#include "mysqlrouter/connection.h"
#include "mysql_auth/mysql_common.h"

#include <iostream>
#include <sstream>

#include <cassert>
#include <cctype>

using routing::SocketOperationsBase;

Connection::Connection(int fd, SocketOperationsBase *sock_ops) : fd_(fd), packet_number_(0),
    buffer_(new uint8_t[kBufferSize]), buf_(buffer_.get()), sock_ops_(sock_ops) {}

Connection::Connection(Connection &&other) : fd_(other.fd_), packet_number_(other.packet_number_),
    buffer_(std::move(other.buffer_)), buf_(other.buf_), sock_ops_(other.sock_ops_) {
  other.fd_ = -1;
  other.packet_number_ = 0;
  other.buf_ = nullptr;
  other.sock_ops_ = nullptr;
}

Connection::~Connection() {
  if (fd_ >= 0) {
    sock_ops_->close(fd_);
  }
}

ssize_t Connection::Recv() {
  ssize_t res = sock_ops_->read(fd_, buf_, kBufferSize);
  if (res <= 0) {
    return res;
  }
  packet_number_ = buf_[kMySQLSeqOffset] + 1;
  return res;
}

ssize_t Connection::TryRecv() {
  if (sock_ops_->has_data(fd_)) {
    return Recv();
  }
  return -2;
}

ssize_t Connection::Send(size_t size) {
  return sock_ops_->write(fd_, buf_, size);
}

ssize_t Connection::Send(uint8_t *buffer, size_t size) {
  return sock_ops_->write(fd_, buffer, size);
}
