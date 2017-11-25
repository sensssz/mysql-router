#ifndef ROUTING_SRC_SHARDER_H_
#define ROUTING_SRC_SHARDER_H_

#include "mysql_auth/mysql_auth_client.h"
#include "mysql_auth/mysql_auth_server.h"

#include <vector>

class Sharder {
public:
  Sharder(std::vector<int> &&server_fds);
  Sharder(const std::vector<int> &server_fds);
  ~Sharder();

  int GetShard(const std::string &column, int key);
  int GetDefaultShard() {
    return server_fds_[0];
  }
  bool Authenticate(int client_fd);
  int Read(uint8_t *buffer, size_t size);
  int Write(uint8_t *buffer, size_t size);
  void DisconnectServers();

private:
  std::vector<int> server_fds_;
  std::unique_ptr<MySQLSession> session_;
};

#endif // ROUTING_SRC_SHARDER_H_