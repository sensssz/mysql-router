#include "mysql_auth_client.h"
#include "logger.h"

#include <sys/types.h>
#include <unistd.h>

static std::unique_ptr<MySQLSession> MySQLHandshake(Connection *connection) {
  uint8_t *outbuf = nullptr;
  size_t mysql_payload_size = 0;
  /* uint8_t mysql_filler = GW_MYSQL_HANDSHAKE_FILLER; not needed*/
  uint8_t mysql_protocol_version = kMySQLProtocolVersion;
  uint8_t *mysql_handshake_payload = nullptr;
  uint8_t mysql_thread_id_num[4];
  uint8_t mysql_scramble_buf[9] = "";
  uint8_t mysql_plugin_data[13] = "";
  uint8_t mysql_server_capabilities_one[2];
  uint8_t mysql_server_capabilities_two[2];
  uint8_t mysql_server_language = 8;
  uint8_t mysql_server_status[2];
  uint8_t mysql_scramble_len = 21;
  uint8_t mysql_filler_ten[10] = {};
  /* uint8_t mysql_last_byte = 0x00; not needed */
  char server_scramble[kMySQLScrambleSize + 1] = "";
  const char *version_string;
  size_t len_version_string = 0;
  int id_num;
  static int id = 0;

  auto session = std::unique_ptr<MySQLSession>(new MySQLSession);

  version_string = MYSQL_VERSION;
  len_version_string = strlen(MYSQL_VERSION);

  generate_random_str(server_scramble, kMySQLScrambleSize);

  // copy back to the caller
  memcpy(session->scramble, server_scramble, kMySQLScrambleSize);

  // thread id, now put thePID
  id_num = getpid() + id;
  mysql_set_byte4(mysql_thread_id_num, id_num);

  memcpy(mysql_scramble_buf, server_scramble, 8);

  memcpy(mysql_plugin_data, server_scramble + 8, 12);

  /**
   * Use the default authentication plugin name in the initial handshake. If the
   * authenticator needs to change the authentication method, it should send
   * an AuthSwitchRequest packet to the client.
   */
  const char* plugin_name = DEFAULT_MYSQL_AUTH_PLUGIN;
  size_t plugin_name_len = strlen(plugin_name);

  mysql_payload_size =
      sizeof(mysql_protocol_version) + (len_version_string + 1) + sizeof(mysql_thread_id_num) + 8 +
      sizeof(/* mysql_filler */ uint8_t) + sizeof(mysql_server_capabilities_one) + sizeof(mysql_server_language) +
      sizeof(mysql_server_status) + sizeof(mysql_server_capabilities_two) + sizeof(mysql_scramble_len) +
      sizeof(mysql_filler_ten) + 12 + sizeof(/* mysql_last_byte */ uint8_t) + plugin_name_len +
      sizeof(/* mysql_last_byte */ uint8_t);

  outbuf = connection->Payload();

  // current buffer pointer
  mysql_handshake_payload = outbuf;

  // write protocol version
  memcpy(mysql_handshake_payload, &mysql_protocol_version, sizeof(mysql_protocol_version));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_protocol_version);

  // write server version plus 0 filler
  strcpy((char *)mysql_handshake_payload, version_string);
  mysql_handshake_payload = mysql_handshake_payload + len_version_string;

  *mysql_handshake_payload = 0x00;

  mysql_handshake_payload++;

  // write thread id
  memcpy(mysql_handshake_payload, mysql_thread_id_num, sizeof(mysql_thread_id_num));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_thread_id_num);

  // write scramble buf
  memcpy(mysql_handshake_payload, mysql_scramble_buf, 8);
  mysql_handshake_payload = mysql_handshake_payload + 8;
  *mysql_handshake_payload = kMySQLHandshakeFiller;
  mysql_handshake_payload++;

  // write server capabilities part one
  mysql_server_capabilities_one[0] = (uint8_t)MYSQL_CAPABILITIES_SERVER;
  mysql_server_capabilities_one[1] = (uint8_t)(MYSQL_CAPABILITIES_SERVER >> 8);

  memcpy(mysql_handshake_payload, mysql_server_capabilities_one, sizeof(mysql_server_capabilities_one));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_capabilities_one);

  // write server language
  memcpy(mysql_handshake_payload, &mysql_server_language, sizeof(mysql_server_language));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_language);

  //write server status
  mysql_server_status[0] = 2;
  mysql_server_status[1] = 0;
  memcpy(mysql_handshake_payload, mysql_server_status, sizeof(mysql_server_status));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_status);

  //write server capabilities part two
  mysql_server_capabilities_two[0] = (uint8_t)(MYSQL_CAPABILITIES_SERVER >> 16);
  mysql_server_capabilities_two[1] = (uint8_t)(MYSQL_CAPABILITIES_SERVER >> 24);

  /** NOTE: pre-2.1 versions sent the fourth byte of the capabilities as
   the value 128 even though there's no such capability. */

  memcpy(mysql_handshake_payload, mysql_server_capabilities_two, sizeof(mysql_server_capabilities_two));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_capabilities_two);

  // write scramble_len
  memcpy(mysql_handshake_payload, &mysql_scramble_len, sizeof(mysql_scramble_len));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_scramble_len);

  //write 10 filler
  memcpy(mysql_handshake_payload, mysql_filler_ten, sizeof(mysql_filler_ten));
  mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_filler_ten);

  // write plugin data
  memcpy(mysql_handshake_payload, mysql_plugin_data, 12);
  mysql_handshake_payload = mysql_handshake_payload + 12;

  //write last byte, 0
  *mysql_handshake_payload = 0x00;
  mysql_handshake_payload++;

  // to be understanded ????
  memcpy(mysql_handshake_payload, plugin_name, plugin_name_len);
  mysql_handshake_payload = mysql_handshake_payload + plugin_name_len;

  //write last byte, 0
  *mysql_handshake_payload = 0x00;

  // writing data in the Client buffer queue
  if (connection->Send(mysql_payload_size) <= 0) {
    return nullptr;
  } else {
    return std::move(session);
  }
}

/**
 * @brief Store client connection information into the DCB
 * @param dcb Client DCB
 * @param buffer Buffer containing the handshake response packet
 */
static void store_client_information(MySQLSession *session, uint8_t *data, size_t len) {
  session->client_capabilities = mysql_get_byte4(data + kMySQLClientCapOffset);
  session->charset = data[kMySQLCharsetOffset];

  if (len > kMySQLAuthPacketBaseSize) {
    strcpy(session->user, (char*)data + kMySQLAuthPacketBaseSize);
    log_debug("User is %s", session->user);

    if (session->client_capabilities & MYSQL_CAPABILITIES_CONNECT_WITH_DB) {

      size_t userlen = strlen(session->user) + 1;

      uint8_t authlen = data[kMySQLAuthPacketBaseSize + userlen];

      size_t dboffset = kMySQLAuthPacketBaseSize + userlen + authlen + 1;

      if (data[dboffset]) {
        /** Client is connecting with a default database */
        strcpy(session->db, (char*)data + dboffset);
      }
    }
  }
}

std::unique_ptr<MySQLSession> AuthenticateClient(Connection *connection) {
  log_debug("Sending authenticate packet ");
  auto session = MySQLHandshake(connection);
  if (session.get() == nullptr) {
    log_error("Error sending authenticate packet");
    return nullptr;
  }
  log_debug("Reading client response");
  ssize_t size = connection->Recv();
  if (size <= 0) {
    log_error("Error receiving client response");
    return nullptr;
  }
  store_client_information(session.get(), connection->Buffer(), size);
  return std::move(session);
}