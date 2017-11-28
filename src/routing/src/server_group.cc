#include "server_group.h"

ServerGroup::ServerGroup(const std::vector<int> &server_fds) {
  for (auto fd : server_fds) {
    server_conns_.emplace_back(fd, routing::RdmaOperations::instance());
  }
}

bool ServerGroup::Authenticate(Connection *client) {

}
