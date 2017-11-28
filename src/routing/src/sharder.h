#ifndef ROUTING_SRC_SHARDER_H_
#define ROUTING_SRC_SHARDER_H_

#include "mysql_auth/mysql_auth_client.h"
#include "mysql_auth/mysql_auth_server.h"
#include "mysqlrouter/connection.h"

#include <vector>

class Sharder {
public:
  Sharder(const std::vector<int> &server_fds);

  bool Authenticate(Connection *client);
  int Read(uint8_t *buffer, size_t size);
  int Write(uint8_t *buffer, size_t size);

private:
  std::vector<Connection> server_conns_;
  std::unique_ptr<MySQLSession> session_;
};

#endif // ROUTING_SRC_SHARDER_H_