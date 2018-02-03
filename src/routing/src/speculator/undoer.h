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
  static std::unordered_map<std::string, std::vector<std::string>> kTablePkeys;

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