#ifndef SRC_SPECULATOR_UNDOER_H_
#define SRC_SPECULATOR_UNDOER_H_

#include "../server_group.h"

#include "hsql/SQLParser.h"

#include <vector>
#include <unordered_map>

class Undoer {
public:
  Undoer(ServerGroup *server_group);
  std::string GetUndoQuery(const std::string &query);

private:
  static std::unordered_map<std::string, std::vector<std::string>> kTablePkeys {
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

  ServerGroup *server_group_;

  std::string GetInsertUndo(const hsql::InsertStatement *stmt);
  // Get a select or new update query from an update
  std::string GetQueryFromUpdate(
    const std::string &query,
    const hsql::UpdateStatement *stmt,
    const std::vector<std::string> &values);
  std::string ParseResults(std::unique_ptr<uint8_t[]> result, size_t size);
  std::string GetSelectFromUpdate(const std::string &query,
                                  const hsql::UpdateStatement *stmt);
  std::string GetUpdateUndo(const std::string &query,
                            const hsql::UpdateStatement *update);
};

#endif // SRC_SPECULATOR_UNDOER_H_