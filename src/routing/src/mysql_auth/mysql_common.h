#ifndef MYSQL_AUTH_MYSQL_COMMON_H_
#define MYSQL_AUTH_MYSQL_COMMON_H_

#include "mysqlrouter/routing.h"

#include <memory>

#include <cstdint>
#include <cstring>

#include <openssl/sha.h>

#define MYSQL_VERSION "5.5.5-10.0.0 SQP"
/** Name of the default server side authentication plugin */
#define DEFAULT_MYSQL_AUTH_PLUGIN "mysql_native_password"

/** Protocol packing macros. */
#define mysql_set_byte2(__buffer, __int) do { \
  (__buffer)[0]= (uint8_t)((__int) & 0xFF); \
  (__buffer)[1]= (uint8_t)(((__int) >> 8) & 0xFF); } while (0)
#define mysql_set_byte3(__buffer, __int) do { \
  (__buffer)[0]= (uint8_t)((__int) & 0xFF); \
  (__buffer)[1]= (uint8_t)(((__int) >> 8) & 0xFF); \
  (__buffer)[2]= (uint8_t)(((__int) >> 16) & 0xFF); } while (0)
#define mysql_set_byte4(__buffer, __int) do { \
  (__buffer)[0]= (uint8_t)((__int) & 0xFF); \
  (__buffer)[1]= (uint8_t)(((__int) >> 8) & 0xFF); \
  (__buffer)[2]= (uint8_t)(((__int) >> 16) & 0xFF); \
  (__buffer)[3]= (uint8_t)(((__int) >> 24) & 0xFF); } while (0)

/** Protocol unpacking macros. */
#define mysql_get_byte2(__buffer) \
  (uint16_t)((__buffer)[0] | \
            ((__buffer)[1] << 8))
#define mysql_get_byte3(__buffer) \
  (uint32_t)((__buffer)[0] | \
            ((__buffer)[1] << 8) | \
            ((__buffer)[2] << 16))
#define mysql_get_byte4(__buffer) \
  (uint32_t)((__buffer)[0] | \
            ((__buffer)[1] << 8) | \
            ((__buffer)[2] << 16) | \
            ((__buffer)[3] << 24))
#define mysql_get_byte8(__buffer) \
  ((uint64_t)(__buffer)[0] | \
  ((uint64_t)(__buffer)[1] << 8) | \
  ((uint64_t)(__buffer)[2] << 16) | \
  ((uint64_t)(__buffer)[3] << 24) | \
  ((uint64_t)(__buffer)[4] << 32) | \
  ((uint64_t)(__buffer)[5] << 40) | \
  ((uint64_t)(__buffer)[6] << 48) | \
  ((uint64_t)(__buffer)[7] << 56))

const int kMySQLScrambleSize = 20;
const int kMySQLScrambleSize323 = 8;
const int kMySQLHeaderLen = 4;
const int kMySQLChecksumlen = 4;
const int kMySQLUserMaxLen = 128;
const int kMySQLPasswordLen = 41;
const int kMySQLHostMaxLen = 60;
const int kMySQLDatabaseMaxLen = 128;
const int kMySQLTableMaxLen = 64;
/**
 * Offsets and sizes of various parts of the client packet. If the offset is
 * defined but not the size, the size of the value is one byte.
 */
const int kMySQLSeqOffset = 3;
const int kMySQLComOffset = 3;
const int kMySQLCharsetOffset = 12;
const int kMySQLClientCapOffset = 4;
const int kMySQLClientCapSize = 4;
const int kMySQLProtocolVersion = 10;
const int kMySQLHandshakeFiller = 0x00;
const int kMySQLServerLanguage = 0x08;
const int kMySQLAuthPacketBaseSize = 36;
const long kMySQLMaxPacketLen = 0xffffffL;

const int kMySQLReplyErr         = 0xff;
const int kMySQLReplyOk          = 0x00;
const int kMySQLReplyEof         = 0xfe;
const int kMySQLReplyLocalInfile = 0xfb;


struct MySQLSession {
  uint8_t scramble[kMySQLScrambleSize];           /*< server scramble, created or received */
  uint8_t password[kMySQLScrambleSize];           /*< password */
  char user[kMySQLUserMaxLen + 1];                /*< username       */
  char db[kMySQLDatabaseMaxLen + 1];              /*< database       */
  int client_capabilities;                        /*< client capabilities */
  unsigned int charset;                           /*< MySQL character set at connect time */
  unsigned long tid;                              /*< MySQL Thread ID, in handshake */
};

/** MySQL protocol constants */
typedef enum
{
  MYSQL_CAPABILITIES_NONE =                   0,
  /** This is sent by pre-10.2 clients */
  MYSQL_CAPABILITIES_CLIENT_MYSQL =           (1 << 0),
  MYSQL_CAPABILITIES_FOUND_ROWS =             (1 << 1),
  MYSQL_CAPABILITIES_LONG_FLAG =              (1 << 2),
  MYSQL_CAPABILITIES_CONNECT_WITH_DB =        (1 << 3),
  MYSQL_CAPABILITIES_NO_SCHEMA =              (1 << 4),
  MYSQL_CAPABILITIES_COMPRESS =               (1 << 5),
  MYSQL_CAPABILITIES_ODBC =                   (1 << 6),
  MYSQL_CAPABILITIES_LOCAL_FILES =            (1 << 7),
  MYSQL_CAPABILITIES_IGNORE_SPACE =           (1 << 8),
  MYSQL_CAPABILITIES_PROTOCOL_41 =            (1 << 9),
  MYSQL_CAPABILITIES_INTERACTIVE =            (1 << 10),
  MYSQL_CAPABILITIES_SSL =                    (1 << 11),
  MYSQL_CAPABILITIES_IGNORE_SIGPIPE =         (1 << 12),
  MYSQL_CAPABILITIES_TRANSACTIONS =           (1 << 13),
  MYSQL_CAPABILITIES_RESERVED =               (1 << 14),
  MYSQL_CAPABILITIES_SECURE_CONNECTION =      (1 << 15),
  MYSQL_CAPABILITIES_MULTI_STATEMENTS =       (1 << 16),
  MYSQL_CAPABILITIES_MULTI_RESULTS =          (1 << 17),
  MYSQL_CAPABILITIES_PS_MULTI_RESULTS =       (1 << 18),
  MYSQL_CAPABILITIES_PLUGIN_AUTH =            (1 << 19),
  MYSQL_CAPABILITIES_CONNECT_ATTRS =          (1 << 20),
  MYSQL_CAPABILITIES_AUTH_LENENC_DATA =       (1 << 21),
  MYSQL_CAPABILITIES_EXPIRE_PASSWORD =        (1 << 22),
  MYSQL_CAPABILITIES_SESSION_TRACK =          (1 << 23),
  MYSQL_CAPABILITIES_DEPRECATE_EOF =          (1 << 24),
  MYSQL_CAPABILITIES_SSL_VERIFY_SERVER_CERT = (1 << 30),
  MYSQL_CAPABILITIES_REMEMBER_OPTIONS =       (1 << 31),
  MYSQL_CAPABILITIES_CLIENT = (
    MYSQL_CAPABILITIES_CLIENT_MYSQL |
    MYSQL_CAPABILITIES_FOUND_ROWS |
    MYSQL_CAPABILITIES_LONG_FLAG |
    MYSQL_CAPABILITIES_CONNECT_WITH_DB |
    MYSQL_CAPABILITIES_LOCAL_FILES |
    MYSQL_CAPABILITIES_PLUGIN_AUTH |
    MYSQL_CAPABILITIES_TRANSACTIONS |
    MYSQL_CAPABILITIES_PROTOCOL_41 |
    MYSQL_CAPABILITIES_MULTI_STATEMENTS |
    MYSQL_CAPABILITIES_MULTI_RESULTS |
    MYSQL_CAPABILITIES_PS_MULTI_RESULTS |
    MYSQL_CAPABILITIES_SECURE_CONNECTION),
  MYSQL_CAPABILITIES_SERVER = (
    MYSQL_CAPABILITIES_CLIENT_MYSQL |
    MYSQL_CAPABILITIES_FOUND_ROWS |
    MYSQL_CAPABILITIES_LONG_FLAG |
    MYSQL_CAPABILITIES_CONNECT_WITH_DB |
    MYSQL_CAPABILITIES_NO_SCHEMA |
    MYSQL_CAPABILITIES_ODBC |
    MYSQL_CAPABILITIES_LOCAL_FILES |
    MYSQL_CAPABILITIES_IGNORE_SPACE |
    MYSQL_CAPABILITIES_PROTOCOL_41 |
    MYSQL_CAPABILITIES_INTERACTIVE |
    MYSQL_CAPABILITIES_IGNORE_SIGPIPE |
    MYSQL_CAPABILITIES_TRANSACTIONS |
    MYSQL_CAPABILITIES_RESERVED |
    MYSQL_CAPABILITIES_SECURE_CONNECTION |
    MYSQL_CAPABILITIES_MULTI_STATEMENTS |
    MYSQL_CAPABILITIES_MULTI_RESULTS |
    MYSQL_CAPABILITIES_PS_MULTI_RESULTS |
    MYSQL_CAPABILITIES_PLUGIN_AUTH),
} mysql_capabilities_t;

/**
 * Authentication states
 *
 * The state usually goes from INIT to CONNECTED and alternates between
 * MESSAGE_READ and RESPONSE_SENT until ending up in either FAILED or COMPLETE.
 *
 * If the server immediately rejects the connection, the state ends up in
 * HANDSHAKE_FAILED. If the connection creation would block, instead of going to
 * the CONNECTED state, the connection will be in PENDING_CONNECT state until
 * the connection can be created.
 */
typedef enum
{
  AUTH_STATE_INIT, /**< Initial authentication state */
  AUTH_STATE_PENDING_CONNECT,/**< Connection creation is underway */
  AUTH_STATE_CONNECTED, /**< Network connection to server created */
  AUTH_STATE_MESSAGE_READ, /**< Read a authentication message from the server */
  AUTH_STATE_RESPONSE_SENT, /**< Responded to the read authentication message */
  AUTH_STATE_FAILED, /**< Authentication failed */
  AUTH_STATE_HANDSHAKE_FAILED, /**< Authentication failed immediately */
  AUTH_STATE_COMPLETE /**< Authentication is complete */
} auth_state_t;

int generate_random_str(char *output, int len);
auth_state_t send_backend_auth(MySQLSession *session, int fd);
bool mysql_is_ok_packet(uint8_t *buffer);
bool mysql_is_result_set(uint8_t *buffer);

#endif // MYSQL_AUTH_MYSQL_COMMON_H_