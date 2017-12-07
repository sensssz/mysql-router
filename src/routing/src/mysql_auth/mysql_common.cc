#include "mysql_common.h"

#include <iostream>

#include <openssl/sha.h>

static unsigned int x = 123456789, y = 987654321, z = 43219876, c = 6543217; /* Seed variables */

static unsigned int random_jkiss()
{
    unsigned long long t;
    unsigned int result;

    x = 314527869 * x + 1234567;
    y ^= y << 5;
    y ^= y >> 7;
    y ^= y << 22;
    t = 4294584393ULL * z + c;
    c = t >> 32;
    z = t;
    result = x + y + z;
    return result;
}

/*****************************************
 * generate a random char
 *****************************************/
static char randomchar()
{
    return (char)((random_jkiss() % 78) + 30);
}

/*****************************************
 * generate a random string
 * output must be pre allocated
 *****************************************/
int generate_random_str(char *output, int len)
{
    int i;

    for (i = 0; i < len; ++i )
    {
        output[i] = randomchar();
    }

    output[len] = '\0';

    return 0;
}

/****************************************************
 * fill a preallocated buffer with XOR(str1, str2)
 * XOR between 2 equal len strings
 * note that XOR(str1, XOR(str1 CONCAT str2)) == str2
 * and that  XOR(str1, str2) == XOR(str2, str1)
 *****************************************************/
void str_xor(uint8_t *output, const uint8_t *input1, const uint8_t *input2, unsigned int len)
{
  const uint8_t *input1_end = nullptr;
  input1_end = input1 + len;

  while (input1 < input1_end) {
    *output++ = *input1++ ^ *input2++;
  }
}

/**********************************************************
 * fill a 20 bytes preallocated with SHA1 digest (160 bits)
 * for one input on in_len bytes
 **********************************************************/
void sha1_str(const uint8_t *in, int in_len, uint8_t *out)
{
  unsigned char hash[SHA_DIGEST_LENGTH];

  SHA1(in, in_len, hash);
  memcpy(out, hash, SHA_DIGEST_LENGTH);
}

/********************************************************
 * fill 20 bytes preallocated with SHA1 digest (160 bits)
 * for two inputs, in_len and in2_len bytes
 ********************************************************/
void sha1_2_str(const uint8_t *in, int in_len, const uint8_t *in2, int in2_len, uint8_t *out)
{
  SHA_CTX context;
  unsigned char hash[SHA_DIGEST_LENGTH];

  SHA1_Init(&context);
  SHA1_Update(&context, in, in_len);
  SHA1_Update(&context, in2, in2_len);
  SHA1_Final(hash, &context);

  memcpy(out, hash, SHA_DIGEST_LENGTH);
}

/**
 * @brief Computes the size of the response to the DB initial handshake
 *
 * When the connection is to be SSL, but an SSL connection has not yet been
 * established, only a basic 36 byte response is sent, including the SSL
 * capability flag.
 *
 * Otherwise, the packet size is computed, based on the minimum size and
 * increased by the optional or variable elements.
 *
 * @param user  Name of the user seeking to connect
 * @param passwd Password for the user seeking to connect
 * @param dbname Name of the database to be made default, if any
 * @return The length of the response packet
 */
static int
response_length(char *user, uint8_t *passwd, char *dbname, const char *auth_module) {
  int bytes;

  // Protocol MySQL HandshakeResponse for CLIENT_PROTOCOL_41
  // 4 bytes capabilities + 4 bytes max packet size + 1 byte charset + 23 '\0' bytes
  // 4 + 4 + 1 + 23  = 32
  bytes = 32;

  if (user) {
    bytes += strlen(user);
  }
  // the nullptr
  bytes++;

  // next will be + 1 (scramble_len) + 20 (fixed_scramble) + 1 (user nullptr term) + 1 (db nullptr term)

  if (passwd) {
    bytes += kMySQLScrambleSize;
  }
  bytes++;

  if (dbname && strlen(dbname)) {
    bytes += strlen(dbname);
    bytes++;
  }

  bytes += strlen(auth_module);
  bytes++;

  // the packet header
  bytes += 4;

  return bytes;
}

/**
 * Calculates the a hash from a scramble and a password
 *
 * The algorithm used is: `SHA1(scramble + SHA1(SHA1(password))) ^ SHA1(password)`
 *
 * @param scramble The 20 byte scramble sent by the server
 * @param passwd   The SHA1(password) sent by the client
 * @param output   Pointer where the resulting 20 byte hash is stored
 */
static void calculate_hash(uint8_t *scramble, uint8_t *passwd, uint8_t *output)
{
  uint8_t hash1[kMySQLScrambleSize] = "";
  uint8_t hash2[kMySQLScrambleSize] = "";
  uint8_t new_sha[kMySQLScrambleSize] = "";

  // hash1 is the function input, SHA1(real_password)
  sha1_str(passwd, strlen(reinterpret_cast<char *>(passwd)), hash1);
  // memcpy(hash1, passwd, kMySQLScrambleSize);

  // hash2 is the SHA1(input data), where input_data = SHA1(real_password)
  sha1_str(hash1, kMySQLScrambleSize, hash2);

  // new_sha is the SHA1(CONCAT(scramble, hash2)
  sha1_2_str(scramble, kMySQLScrambleSize, hash2, kMySQLScrambleSize, new_sha);

  // compute the xor in client_scramble
  str_xor(output, new_sha, hash1, kMySQLScrambleSize);
}

/**
 * @brief Helper function to load hashed password
 *
 * @param conn DCB Protocol object
 * @param payload Destination where hashed password is written
 * @param passwd Client's double SHA1 password
 *
 * @return Address of the next byte after the end of the stored password
 */
static uint8_t *
load_hashed_password(uint8_t *scramble, uint8_t *payload, uint8_t *passwd)
{
  *payload++ = kMySQLScrambleSize;
  calculate_hash(scramble, passwd, payload);
  return payload + kMySQLScrambleSize;
}

/**
 * @brief Computes the capabilities bit mask for connecting to backend DB
 *
 * We start by taking the default bitmask and removing any bits not set in
 * the bitmask contained in the connection structure. Then add SSL flag if
 * the connection requires SSL (set from the MaxScale configuration). The
 * compression flag may be set, although compression is NOT SUPPORTED. If a
 * database name has been specified in the function call, the relevant flag
 * is set.
 *
 * @param conn  The MySQLSession structure for the connection
 * @param db_specified Whether the connection request specified a databas
 * @param compress Whether compression is requested - NOT SUPPORTED
 * @return Bit mask (32 bits)
 */
static uint32_t
create_capabilities(MySQLSession *session, bool db_specified, bool compress)
{
  uint32_t final_capabilities;

  /** Copy client's flags to backend but with the known capabilities mask */
  final_capabilities = (session->client_capabilities & static_cast<uint32_t>(MYSQL_CAPABILITIES_CLIENT));

  /* Compression is not currently supported */
  if (compress) {
    final_capabilities |= static_cast<uint32_t>(MYSQL_CAPABILITIES_COMPRESS);
  }

  if (db_specified) {
    /* With database specified */
    final_capabilities |= static_cast<int>(MYSQL_CAPABILITIES_CONNECT_WITH_DB);
  }
  else
  {
    /* Without database specified */
    final_capabilities &= ~static_cast<int>(MYSQL_CAPABILITIES_CONNECT_WITH_DB);
  }

  final_capabilities |= static_cast<int>(MYSQL_CAPABILITIES_PLUGIN_AUTH);

  return final_capabilities;
}

/**
 * Write MySQL authentication packet to backend server
 *
 * @param dcb  Backend DCB
 * @return True on success, false on failure
 */
auth_state_t send_backend_auth(MySQLSession *session, Connection *connection)
{
  uint8_t client_capabilities[4] = {0, 0, 0, 0};
  uint8_t *curr_passwd = session->password;
  curr_passwd = nullptr;

  // std::cerr << "Logging in as " << session->user << " with password " << session->password << std::endl;

  /**
   * If session is stopping or has failed return with error.
   */
  if (session == nullptr) {
    return AUTH_STATE_FAILED;
  }

  uint32_t capabilities = create_capabilities(session, (session->db && strlen(session->db)), false);
  mysql_set_byte4(client_capabilities, capabilities);

  /**
   * Use the default authentication plugin name. If the server is using a
   * different authentication mechanism, it will send an AuthSwitchRequest
   * packet.
   */
  const char* auth_plugin_name = DEFAULT_MYSQL_AUTH_PLUGIN;

  long bytes = response_length(session->user, curr_passwd,
                               session->db, auth_plugin_name);

  uint8_t *buffer = connection->Buffer();
  mysql_set_byte3(buffer, bytes - kMySQLHeaderLen);
  buffer[kMySQLSeqOffset] = 1;
  uint8_t *payload = buffer + kMySQLHeaderLen;

  // clearing data
  memset(payload, '\0', bytes);

  // set client capabilities
  memcpy(payload, client_capabilities, 4);

  // set now the max-packet size
  payload += 4;
  mysql_set_byte4(payload, kMySQLMaxPacketLen);

  // set the charset
  payload += 4;
  *payload = static_cast<uint8_t>(session->charset);

  payload++;

  // 19 filler bytes of 0
  payload += 19;

  // 4 bytes filler for extra capabilities
  payload += 4;

  // 4 + 4 + 4 + 1 + 23 = 36, this includes the 4 bytes packet header
  memcpy(payload, session->user, strlen(session->user));
  payload += strlen(session->user);
  payload++;

  if (curr_passwd != nullptr) {
    payload = load_hashed_password(session->scramble, payload, curr_passwd);
  }
  else
  {
    payload++;
  }

  // if the db is not nullptr append it
  if (session->db[0]) {
    memcpy(payload, session->db, strlen(session->db));
    payload += strlen(session->db);
    payload++;
  }

  memcpy(payload, auth_plugin_name, strlen(auth_plugin_name));

  if (connection->Send(bytes) > 0) {
    return AUTH_STATE_RESPONSE_SENT;
  } else {
    return AUTH_STATE_FAILED;
  }
}

/**
 * @brief Check if the buffer contains an OK packet
 *
 * @param buffer Buffer containing a complete MySQL packet
 * @return True if the buffer contains an OK packet
 */
bool mysql_is_ok_packet(uint8_t *buffer)
{
    bool rval = false;
    uint8_t cmd = buffer[kMySQLHeaderLen];

    if (cmd == kMySQLReplyOk)
    {
        rval = true;
    }

    return rval;
}

bool mysql_is_result_set(uint8_t *buffer)
{
    bool rval = false;
    uint8_t cmd = buffer[kMySQLHeaderLen];

    switch (cmd)
    {
    case kMySQLReplyOk:
    case kMySQLReplyErr:
    case kMySQLReplyLocalInfile:
    case kMySQLReplyEof:
        /** Not a result set */
        break;

    default:
        rval = true;
        break;
    }

    return rval;
}
