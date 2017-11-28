#ifndef MYSQL_AUTH_MYSQL_COMMON_H_
#define MYSQL_AUTH_MYSQL_COMMON_H_

#include "mysqlrouter/connection.h"
#include "mysql_constant.h"

#include <cstring>

int generate_random_str(char *output, int len);
auth_state_t send_backend_auth(MySQLSession *session, Connection *connection);
bool mysql_is_ok_packet(uint8_t *buffer);
bool mysql_is_result_set(uint8_t *buffer);

#endif // MYSQL_AUTH_MYSQL_COMMON_H_