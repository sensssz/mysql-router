#ifndef MYSQL_AUTH_MYSQL_SERVER_H_
#define MYSQL_AUTH_MYSQL_SERVER_H_

#include "mysql_common.h"

int AuthWithBackendServers(MySQLSession *session, Connection *connection);

#endif // MYSQL_AUTH_MYSQL_SERVER_H_