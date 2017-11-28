#ifndef MYSQL_AUTH_MYSQL_AUTH_CLIENT_H_
#define MYSQL_AUTH_MYSQL_AUTH_CLIENT_H_

#include "mysql_common.h"

std::unique_ptr<MySQLSession> AuthenticateClient(Connection *connection);

#endif // MYSQL_AUTH_MYSQL_AUTH_CLIENT_H_