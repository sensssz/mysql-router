#ifndef ROUTING_SRC_SERVER_GROUP_H_
#define ROUTING_SRC_SERVER_GROUP_H_

#include "mysql_auth/mysql_auth_client.h"
#include "mysql_auth/mysql_auth_server.h"
#include "mysqlrouter/connection.h"

#include <vector>
#include <utility>

class ServerGroup {
public:
  ServerGroup(const std::vector<int> &server_fds);

  bool Authenticate(Connection *client);
  size_t Size() {
    return server_conns_.size();
  }
  int Read(uint8_t *buffer, size_t size);
  int Write(uint8_t *buffer, size_t size);
  std::pair<uint8_t*, size_t> GetResult(size_t server_index);

  bool SendQuery(size_t server_index, const std::string &query, int num_queries=1);
  bool Propagate(const std::string &query, size_t source_write_server, int num_queries);
  bool IsReadyForQuery(size_t server_index);
  void WaitForServer(size_t server_index);
  void WaitForAll();
  bool ForwardToAll(const std::string &query, int num_queries=1);
  int GetAvailableServer();
  bool IsExitPacket(uint8_t *buffer, size_t size);

private:

  std::vector<Connection> server_conns_;
  std::vector<bool> has_outstanding_request_;
  std::vector<ssize_t> read_results_;
  std::unique_ptr<MySQLSession> session_;
};

#endif // ROUTING_SRC_SERVER_GROUP_H_