#ifndef ROUTING_SRC_SERVER_GROUP_H_
#define ROUTING_SRC_SERVER_GROUP_H_

#include "mysql_auth/mysql_auth_client.h"
#include "mysql_auth/mysql_auth_server.h"
#include "mysqlrouter/connection.h"

#include <vector>

class ServerGroup {
public:
  ServerGroup(const std::vector<int> &server_fds);

  bool Authenticate(Connection *client);
  int Read(uint8_t *buffer, size_t size);
  int Write(uint8_t *buffer, size_t size);

  bool SendQuery(int server_index, const std::string &query);
  bool ForwardToAll(const std::string &query);

private:
  std::vector<Connection> server_conns_;
  std::vector<bool> ready_for_write_;
  std::unique_ptr<MySQLSession> session_;
};

#endif // ROUTING_SRC_SERVER_GROUP_H_