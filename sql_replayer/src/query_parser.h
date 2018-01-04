#ifndef QUERY_PARSER_H_
#define QUERY_PARSER_H_

#include <fstream>
#include <regex>
#include <unordered_map>

class QueryManager {
public:
  static QueryManager &GetInstance() {
    static QueryManager manager;
    return manager;
  }
  void Dump(const std::string &filename);
  int GetIdForTemplate(const std::string &query_template);
  std::string GetTemplateForId(int query_id) const {
    return id_to_template_.at(query_id);
  }

private:
  std::unordered_map<int, std::string> id_to_template_;
  std::unordered_map<std::string, int> template_to_id_;
};

class QueryParser {
public:
  int GetQueryId(const std::string &sql);
  void DumpTemplates(const std::string &filename);

private:
  std::string ExtractTemplate(const std::string &sql);

  static std::regex argument_pattern_;
  static std::regex num_str_pattern_;
  static std::regex string_pattern_;
  static std::regex number_pattern_;
  static std::regex str_list_pattern_;
  static std::regex num_list_pattern_;
};

#endif // QUERY_PARSER_H_
