#include "undoer.h"
#include "mysqlrouter/mysql_constant.h"
#include "logger.h"
#include <cstdint>

namespace {

struct Packet {
  uint8_t *payload;
  size_t size;

  Packet() : payload(nullptr), size(0) {}

  bool IsEof() {
    return payload != nullptr && *payload == 0xfe && size < 8;
  }
};

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
  hsql::SQLParserResult result;
  hsql::SQLParser::parse(query, &result);
  if (!result.isValid() || result.size() == 0) {
    return "";
  }
  auto stmt = result.getStatement(0);
  switch (stmt->type()) {
  case hsql::kStmtInsert:
    return GetInsertUndo(reinterpret_cast<const hsql::InsertStatement *>(stmt));
  case hsql::kStmtUpdate:
    return GetUpdateUndo(query, reinterpret_cast<const hsql::UpdateStatement *>(stmt));
  default:
    return "";
  }
}

std::string Undoer::GetInsertUndo(const hsql::InsertStatement *stmt) {
  log_debug("Generating undo query for insert");
  auto table_name = std::string(stmt->tableName);
  auto &values = *(stmt->values);
  auto &pkeys = kTablePkeys[table_name];
  auto undo_query = "DELETE FROM " + table_name + " WHERE " + pkeys[0] + "=" + std::to_string(values[0]->ival);
  for (size_t i = 1; i < pkeys.size(); i++) {
    auto &key = pkeys[i];
    auto val = std::to_string(values[i]->ival);
    undo_query += " AND " + key + "=" + val;
  }
  log_debug("Undo is %s", undo_query.c_str());
  return undo_query;
}

// Get a select or new update query from an update
std::string Undoer::GetQueryFromUpdate(
  const std::string &query,
  const hsql::UpdateStatement *stmt,
  const std::vector<std::string> &values) {
  auto table_name = std::string(stmt->table->name);
  auto updates = *(stmt->updates);
  std::string undo_query;
  if (values.size() > 0) {
    undo_query = "Update " + table_name + " SET " +
                 std::string(updates[0]->column) + "=" + values[0];
  } else {
    undo_query = "SELECT " + std::string(updates[0]->column);
  }

  for (size_t i = 1; i < updates.size(); i++) {
    auto column = std::string(updates[i]->column);
    if (values.size() > 0) {
      undo_query += "," + std::string(column) + "=" + values[i];
    } else {
      undo_query += "," + std::string(column);
    }
  }
  undo_query += " FROM " + table_name;
  if (stmt->where == nullptr) {
    return undo_query;
  }
  auto where_index = query.find(" WHERE ");
  auto where_clause = query.substr(where_index, query.size() - where_index);
  return undo_query + where_clause;
}

std::vector<std::string> Undoer::ParseResults(std::unique_ptr<uint8_t[]> result) {
  Packet packet;
  std::vector<std::string> values;
  uint8_t *payload = result.get();
  uint64_t field_count = 0;
  payload = ::GetFieldCount(payload, field_count);
  // The column definitions do not matter to us, so just consume them
  for (uint64_t i = 0; i < field_count; i++) {
    payload = ReadNextPacket(payload, packet);
  }
  // Consume the EOF packet
  // payload = ReadNextPacket(payload, packet);
  payload = ReadNextPacket(payload, packet);
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
  const std::string &query,
  const hsql::UpdateStatement *stmt) {
  std::vector<std::string> values;
  return GetQueryFromUpdate(query, stmt, values);
}

std::string Undoer::GetUpdateUndo(
  const std::string &query,
  const hsql::UpdateStatement *stmt) {
  log_debug("Generating undo query for update");
  auto select = GetSelectFromUpdate(query, stmt);
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
  auto undo = GetQueryFromUpdate(query, stmt, values);
  log_debug("New update for the update is %s", undo.c_str());
  return undo;
}