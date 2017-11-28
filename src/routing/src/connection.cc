#include "mysqlrouter/connection.h"
#include "mysql_auth/mysql_common.h"

using routing::SocketOperationsBase;

Connection::Connection(int fd, SocketOperationsBase *sock_ops) : fd_(fd), packet_number_(0),
    buffer_(new uint8_t[kBufferSize]), buf_(buffer_.get()), sock_ops_(sock_ops) {}

Connection::Connection(const Connection &other) {
  fd_ = other.fd_;
  packet_number_ = other.pakcet_number_;
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
  packet_number_ = buf_[kMySQLSeqOffset];
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
  packet_number_++;
  buf_[kMySQLSeqOffset] = packet_number_;
  ssize_t res = sock_ops_->write(fd_, buf_, size);
  if (res <= 0) {
    packet_number_--;
    return res;
  } else {
    return res;
  }
}

ssize_t Connection::Send(uint8_t *buffer, size_t size) {
  packet_number_ = buffer[kMySQLSeqOffset];
  ssize_t res = sock_ops_->write(fd_, buffer, size);
  if (res <= 0) {
    return res;
  } else {
    return res;
  }
}
