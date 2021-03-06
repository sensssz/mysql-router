#include "mysql_auth_server.h"
#include "logger.h"

static char *strend(register const char *s)
{
  while (*s++)
  {
    ;
  }
  return (char*) (s - 1);
}

/**
 * Decode mysql server handshake
 *
 * @param session The MySQLSession structure
 * @param payload The bytes just read from the net
 * @return 0 on success, < 0 on failure
 *
 */
// static int
// decode_mysql_server_handshake(MySQLSession *session, uint8_t *payload)
// {
//   uint8_t *server_version_end = nullptr;
//   uint16_t mysql_server_capabilities_one = 0;
//   uint16_t mysql_server_capabilities_two = 0;
//   unsigned long tid = 0;
//   uint8_t scramble_data_1[kMySQLScrambleSize323] = "";
//   uint8_t scramble_data_2[kMySQLScrambleSize - kMySQLScrambleSize323] = "";
//   uint8_t capab_ptr[4] = "";
//   int scramble_len = 0;
//   uint8_t mxs_scramble[kMySQLScrambleSize] = "";

//   payload++;

//   // Get server version (string)
//   server_version_end = (uint8_t *) strend((char*) payload);

//   payload = server_version_end + 1;

//   // get ThreadID: 4 bytes
//   tid = mysql_get_byte4(payload);
//   memcpy(&session->tid, &tid, 4);

//   payload += 4;

//   // scramble_part 1
//   memcpy(scramble_data_1, payload, kMySQLScrambleSize323);
//   payload += kMySQLScrambleSize323;

//   // 1 filler
//   payload++;

//   mysql_server_capabilities_one = mysql_get_byte2(payload);

//   //Get capabilities_part 1 (2 bytes) + 1 language + 2 server_status
//   payload += 5;

//   mysql_server_capabilities_two = mysql_get_byte2(payload);

//   memcpy(capab_ptr, &mysql_server_capabilities_one, 2);

//   // get capabilities part 2 (2 bytes)
//   memcpy(&capab_ptr[2], &mysql_server_capabilities_two, 2);

//   // 2 bytes shift
//   payload += 2;

//   // get scramble len
//   if (payload[0] > 0) {
//     scramble_len = payload[0] - 1;

//     if ((scramble_len < kMySQLScrambleSize323) ||
//         scramble_len > kMySQLScrambleSize) {
//       /* log this */
//       log_error("Incorrect scramble size");
//       return -2;
//     }
//   }
//   else
//   {
//     scramble_len = kMySQLScrambleSize;
//   }
//   // skip 10 zero bytes
//   payload += 11;

//   // copy the second part of the scramble
//   memcpy(scramble_data_2, payload, scramble_len - kMySQLScrambleSize323);

//   memcpy(mxs_scramble, scramble_data_1, kMySQLScrambleSize323);
//   memcpy(mxs_scramble + kMySQLScrambleSize323, scramble_data_2, scramble_len - kMySQLScrambleSize323);

//   // full 20 bytes scramble is ready
//   memcpy(session->scramble, mxs_scramble, kMySQLScrambleSize);

//   return 0;
// }

int AuthWithBackendServers(MySQLSession *session, Connection *connection) {
  log_debug("Authenticating with server %d", connection->FileDescriptor());
  ssize_t size = 0;
  log_debug("Reading first packet from server");
  if ((size = connection->Recv()) < 0) {
    log_error("Failed to read auth packet from server");
    return -1;
  }
  // log_debug("Decoding server response");
  // decode_mysql_server_handshake(session, connection->Buffer());
  strcpy(session->user, "root");
  if(send_backend_auth(session, connection) == AUTH_STATE_FAILED) {
    log_error("Authentication returns failure");
    return 0;
  }
  return static_cast<int>(connection->Recv());
}
