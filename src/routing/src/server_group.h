#ifndef SERVER_GROUP_H_
#define SERVER_GROUP_H_

#include "mysqlrouter/connection.h"

#include <vector>

class ServerGroup {
public:
  ServerGroup(const std::vector<int> &server_fds);
  bool Authenticate(Connection *client);

private:
  std::vector<Connection> server_conns_;
};

#endif // SERVER_GROUP_H_