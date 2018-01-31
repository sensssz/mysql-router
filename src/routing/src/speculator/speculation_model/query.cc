#include "query.h"

#include <fstream>

namespace model {

static std::string &Replace(std::string& str, const std::string& from, const std::string& to) {
  size_t start_pos = str.find(from);
  if (start_pos != std::string::npos) {
    str.replace(start_pos, from.length(), to);
  }
  return str;
}

void QueryManager::Load(const std::string &path) {
  std::ifstream in_file(path);
  while (!in_file.eof()) {
    int query_id = 0;
    std::string sql_template;
    in_file >> query_id;
    std::getline(in_file, sql_template);
    id_to_template_[query_id] = sql_template;
    template_to_id_[sql_template] = query_id;
  }
}

int QueryManager::GetIdForTemplate(const std::string &query_template) {
  auto iter = template_to_id_.find(query_template);
  if (iter != template_to_id_.end()) {
    return iter->second;
  }
  int id = static_cast<int>(template_to_id_.size());
  template_to_id_[query_template] = id;
  return id;
}

Query::Query() : query_id_(-1) {}

Query::Query(int query_id, std::vector<SqlValue> &&arguments,
  std::vector<std::vector<SqlValue>> &&result_set) :
    query_id_(query_id), arguments_(std::move(arguments)),
    result_set_(std::move(result_set)) {}

std::string Query::ToSql() const {
  std::string sql = QueryManager::GetInstance().GetTemplateForId(query_id_);
  for (auto &arg : arguments_) {
    Replace(sql, "%v", arg.ToString());
  }
  return sql;
}

bool Query::operator==(const Query &other) const {
  return (query_id_ == other.query_id_) && arguments_ == other.arguments_;
}

} // namespace model
