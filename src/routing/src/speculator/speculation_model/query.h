#ifndef BASIC_QUERY_H_
#define BASIC_QUERY_H_

#include "value.h"

#include <unordered_map>
#include <vector>

namespace model {

class QueryManager {
public:
  static QueryManager &GetInstance() {
    static QueryManager manager;
    return manager;
  }
  void Load(const std::string &path);
  int GetIdForTemplate(const std::string &query_template);
  std::string GetTemplateForId(int query_id) const {
    return id_to_template_.at(query_id);
  }

private:
  std::unordered_map<int, std::string> id_to_template_;
  std::unordered_map<std::string, int> template_to_id_;
};

class Query {
public:
  Query();
  Query(int query_id, std::vector<SqlValue> &&arguments,
    std::vector<std::vector<SqlValue>> &&result_set);

  std::string ToSql() const;

  bool operator==(const Query &other) const;

  int query_id() const {
    return query_id_;
  }
  const std::vector<SqlValue> &arguments() const {
    return arguments_;
  }
  const std::vector<std::vector<SqlValue>> &result_set() const {
    return result_set_;
  }
  std::vector<SqlValue> &arguments() {
    return arguments_;
  }
  std::vector<std::vector<SqlValue>> &result_set() {
    return result_set_;
  }

private:
  int query_id_;
  std::vector<SqlValue> arguments_;
  std::vector<std::vector<SqlValue>> result_set_;
};

} // namespace model

#endif // BASIC_QUERY_H_
