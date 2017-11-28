#include "mysqlrouter/connection.h"
#include "mysql_auth/mysql_common.h"

#include <iostream>
#include <sstream>

#include <cctype>

using routing::SocketOperationsBase;

static void ShowBinaryData(const char *data, size_t len) {
  std::stringstream ss;
  for (size_t i = 0; i < len; i++) {
    if (isprint(data[i])) {
      ss << (char) data[i];
    } else {
      ss << '\\' << (int) data[i];
    }
  }
  std::cerr << "Data is " << ss.str() << std::endl;
}

Connection::Connection(int fd, SocketOperationsBase *sock_ops) : fd_(fd), packet_number_(0),
    buffer_(new uint8_t[kBufferSize]), buf_(buffer_.get()), sock_ops_(sock_ops) {}

Connection::Connection(const Connection &other) {
  fd_ = other.fd_;
  packet_number_ = other.packet_number_;
  buffer_.reset(new uint8_t[kBufferSize]);
  buf_ = buffer_.get();
  sock_ops_ = other.sock_ops_;
  memcpy(buf_, other.buf_, kBufferSize);
}

Connection::~Connection() {
  sock_ops_->close(fd_);
}

ssize_t Connection::Recv() {
  ssize_t res = sock_ops_->read(fd_, buf_, kBufferSize);
  if (res <= 0) {
    return res;
  }
  packet_number_ = buf_[kMySQLSeqOffset]++;
  return res;
}

ssize_t Connection::TryRecv() {
  if (sock_ops_->has_data(fd_)) {
    return Recv();
  }
  return -2;
}

ssize_t Connection::Send(size_t size) {
  mysql_set_byte3(buf_, size);
  buf_[kMySQLSeqOffset] = packet_number_;
  std::cerr << "Sending " << size << " bytes" << std::endl;
  ShowBinaryData(reinterpret_cast<const char *>(buf_), size);
  return sock_ops_->write(fd_, buf_, size);
}

ssize_t Connection::Send(uint8_t *buffer, size_t size) {
  return sock_ops_->write(fd_, buffer, size);
}
