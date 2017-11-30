#ifndef CONNECTION_H_
#define CONNECTION_H_

#include "routing.h"
#include "mysql_constant.h"

#include <memory>

class Connection {
public:
  static const size_t kBufferSize = kMySQLMaxPacketLen + kMySQLHeaderLen;

  Connection(int fd, routing::SocketOperationsBase *sock_ops);
  Connection(const Connection &other);
  ~Connection();
  uint8_t *Payload() {
    return buf_ + kMySQLHeaderLen;
  }
  uint8_t *Buffer() {
    return buf_;
  }
  int FileDescriptor() {
    return fd_;
  }
  ssize_t Recv();
  ssize_t TryRecv();
  ssize_t Send(size_t size);
  ssize_t Send(uint8_t *buffer, size_t size);
  bool SendQuery(const std::string &query);

private:
  int fd_;
  uint8_t packet_number_;
  std::unique_ptr<uint8_t[]> buffer_;
  uint8_t *buf_;
  routing::SocketOperationsBase *sock_ops_;
};

#endif // CONNECTION_H_