#ifndef MYSQL_AUTH_MYSQL_SERVER_H_
#define MYSQL_AUTH_MYSQL_SERVER_H_

#include "mysql_common.h"

int AuthWithBackendServers(MySQLSession *session, int fd, uint8_t *buf, size_t buf_len);

#endif // MYSQL_AUTH_MYSQL_SERVER_H_