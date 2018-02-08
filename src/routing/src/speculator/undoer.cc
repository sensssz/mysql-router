#include "undoer.h"
#include "mysqlrouter/mysql_constant.h"
#include "logger.h"

#include <sstream>

#include <cctype>
#include <cstdint>
#include <cstring>

namespace {

struct Packet {
  uint8_t *payload;
  size_t size;

  Packet() : payload(nullptr), size(0) {}

  bool IsEof() {
    return payload != nullptr && *payload == 0xfe && size < 8;
  }
};

size_t NameLen(const std::string &query, size_t start) {
  size_t end = start;
  while (!isspace(query[end]) && query[end] != '(') {
    end++;
  }
  return end - start;
}

std::string ExtractTableName(const std::string &query, const std::string &prefix) {
  auto table_start = prefix.size();
  return query.substr(table_start, NameLen(query, table_start));
}

size_t ValueLen(const std::string &query, size_t start) {
  size_t end = start;
  while (isdigit(query[end])) {
    end++;
  }
  return end - start;
}

size_t NextValueStart(const std::string &query, size_t cursor) {
  while (!isdigit(query[cursor])) {
    cursor++;
  }
  return cursor;
}

std::vector<std::string> ExtractInsertValues(const std::string &query, size_t num_values) {
  std::vector<std::string> values;
  auto paren_start = query.find('(');
  auto value_start = query.find('(', paren_start) + 1;
  for (size_t i = 0; i < num_values; i++) {
    auto value_len = ValueLen(query, value_start);
    values.push_back(query.substr(value_start, value_len));
    value_start = NextValueStart(query, value_start + value_len);
  }
  return std::move(values);
}

uint8_t *ReadNextPacket(uint8_t *data, Packet &packet) {
  packet.payload = data + kMySQLHeaderLen;
  packet.size = mysql_get_byte3(data);
  return data + kMySQLHeaderLen + packet.size;
}

uint64_t ReadNByteInt(uint8_t *bytes, int num_bytes) {
  uint64_t num = 0;
  for (int i = 0; i < num_bytes; i++) {
    num = (num << 8) + bytes[i];
  }
  return num;
}

uint8_t *ReadLengthEncodedInt(uint8_t *payload, uint64_t &num) {
  auto first_byte = *payload;
  payload++;
  int size_size = 0;
  if (first_byte < 0xfb) {
    payload--;
    size_size = 1;
  } else if (first_byte == 0xfc) {
    size_size = 2;
  } else if (first_byte == 0xfd) {
    size_size = 3;
  } else if (first_byte == 0xfe) {
    size_size = 4;
  }
  num = ReadNByteInt(payload, size_size);
  return payload + size_size;
}

uint8_t *ReadLengthEncodedString(uint8_t *payload, std::string &str) {
  uint64_t size;
  payload = ReadLengthEncodedInt(payload, size);
  str = std::string(reinterpret_cast<char *>(payload), size);
  return payload + size;
}

uint8_t *GetFieldCount(uint8_t *payload, uint64_t field_count) {
  Packet packet;
  payload = ReadNextPacket(payload, packet);
  ReadLengthEncodedInt(packet.payload, field_count);
  return payload;
}

size_t NextColumnStart(const std::string &query, size_t cursor, size_t where_start) {
  while (query[cursor] != ',') {
    if (cursor >= where_start) {
      return std::string::npos;
    }
    cursor++;
  }
  cursor++;
  while (isspace(query[cursor])) {
    cursor++;
  }
  return cursor;
}

std::vector<std::string> ExtractUpdateColumns(const std::string &query) {
  auto column_start = query.find(" SET ") + 5;
  auto where_start = query.find(" WHERE ");
  std::vector<std::string> columns;
  while (column_start != std::string::npos) {
    auto column_len = NameLen(query, column_start);
    columns.push_back(query.substr(column_start, column_len));
    column_start = NextColumnStart(query, column_start + column_len, where_start);
  }
  return std::move(columns);
}

} // namespace

std::unordered_map<std::string, std::vector<std::string>> Undoer::kTablePkeys {
  {"ITEM", {"i_id", "i_u_id"}},
  {"ITEM_ATTRIBUTE", {"ia_id", "ia_i_id", "ia_u_id"}},
  {"ITEM_BID", {"ib_id", "ib_i_id", "ib_u_id"}},
  {"ITEM_COMMENT", {"ic_id", "ic_i_id", "ic_u_id"}},
  {"ITEM_IMAGE", {"ii_id", "ii_i_id", "ii_u_id"}},
  {"ITEM_MAX_BID", {"imb_i_id", "imb_u_id"}},
  {"ITEM_PURCHASE", {"ip_id", "ip_ib_id", "ip_ib_i_id", "ip_ib_u_id"}},
  {"USERACCT_FEEDBACK", {"uf_u_id", "uf_i_id", "uf_i_u_id", "uf_from_id"}},
  {"USERACCT_ITEM", {"ui_u_id", "ui_i_id", "ui_i_u_id"}},
};

Undoer::Undoer(ServerGroup *server_group) : server_group_(server_group) {}

std::string Undoer::GetUndoQuery(const std::string &query) {
  log_debug("Generating undo for query %s", query.c_str());
  if (strncmp(query.c_str(), "INSERT", 6) == 0) {
    return GetInsertUndo(query);
  } else if (strncmp(query.c_str(), "UPDATE", 6) == 0) {
    return GetUpdateUndo(query);
  }
  return "";
}

std::string Undoer::GetInsertUndo(const std::string &query) {
  log_debug("Generating undo query for insert");
  auto table_name = ::ExtractTableName(query, "INSERT INTO ");
  auto &pkeys = kTablePkeys[table_name];
  auto values = ::ExtractInsertValues(query, pkeys.size());
  std::stringstream ss;
  ss << "DELETE FROM " << table_name << " WHERE " << pkeys[0] << "=" << values[0];
  for (size_t i = 1; i < pkeys.size(); i++) {
    auto &key = pkeys[i];
    auto &val = values[i];
    ss << " AND " << key << "=" << val;
  }
  auto undo_query = ss.str();
  log_debug("Undo is %s", undo_query.c_str());
  return undo_query;
}

// Get a select or new update query from an update
std::string Undoer::GetQueryFromUpdate(
  const std::string &query,
  const std::vector<std::string> &values) {
  auto table_name = ::ExtractTableName(query, "UPDATE ");
  auto columns = ::ExtractUpdateColumns(query);
  std::stringstream ss;
  std::string undo_query;
  if (values.size() > 0) {
    ss << "UPDATE " << table_name << " SET " << columns[0] << "=" << values[0];
  } else {
    ss << "SELECT " << columns[0];
  }

  for (size_t i = 1; i < columns.size(); i++) {
    auto column = columns[i];
    if (values.size() > 0) {
      ss << "," << column << "=" << values[i];
    } else {
      ss << "," << column;
    }
  }
  ss << " FROM " << table_name;
  auto where_index = query.find(" WHERE ");
  if (where_index == std::string::npos) {
    return ss.str();
  }
  auto where_clause = query.substr(where_index, query.size() - where_index);
  return ss.str() + where_clause;
}

std::vector<std::string> Undoer:(std::unique_ptr<uint8_t[]> result) {
  Packet packet;
  std::vector<std::string> values;
  uint8_t *payload = result.get();
  uint64_t field_count = 0;
  payload = ::GetFieldCount(payload, field_count);
  // The column definitions do not matter to us, so just consume them
  for (uint64_t i = 0; i < field_count; i++) {
    payload = ::ReadNextPacket(payload, packet);
  }
  // Consume the EOF packet
  // payload = ::ReadNextPacket(payload, packet);
  payload = ::ReadNextPacket(payload, packet);
  if (packet.IsEof()) {
    return std::move(values);
  }
  std::string str;
  uint8_t *cur = packet.payload;
  for (uint64_t i = 0; i < field_count; i++) {
    cur = ::ReadLengthEncodedString(cur, str);
    values.push_back(str);
  }
  return std::move(values);
}

std::string Undoer::GetSelectFromUpdate(
  const std::string &query) {
  std::vector<std::string> values;
  return GetQueryFromUpdate(query, values);
}

std::string Undoer::GetUpdateUndo(
  const std::string &query) {
  log_debug("Generating undo query for update");
  auto select = GetSelectFromUpdate(query);
  log_debug("Select for update is %s", select.c_str());
  int server = server_group_->GetAvailableServer();
  if (!server_group_->SendQuery(server, select)) {
    log_error("Error sending select for update");
    return "";
  }
  server_group_->WaitForServer(server);
  auto res = server_group_->GetResult(server);
  auto res_packet = std::unique_ptr<uint8_t[]>(new uint8_t[res.second]);
  memcpy(res_packet.get(), res.first, res.second);
  auto values = ParseResults(std::move(res_packet));
  if (values.size() == 0) {
    return "";
  }
  auto undo = GetQueryFromUpdate(query, values);
  log_debug("New update for the update is %s", undo.c_str());
  return undo;
}