#ifndef BASIC_QUERY_PARSER_H_
#define BASIC_QUERY_PARSER_H_

#include "query.h"

#include <fstream>
#include <regex>

namespace model {

class QueryParser {
public:
  Query ParseQuery(const std::string &json);

private:
  std::vector<std::string> RegexFindAll(const std::regex &regex, std::string str);
  SqlValue CreateListValue(const std::vector<std::string> &values);
  std::vector<SqlValue> ConvertArguments(const std::vector<std::string> &args);
  std::string ExtractTemplate(const std::string &sql);

  static std::regex argument_pattern_;
  static std::regex num_str_pattern_;
  static std::regex string_pattern_;
  static std::regex number_pattern_;
  static std::regex str_list_pattern_;
  static std::regex num_list_pattern_;
};


} // namespace model

#endif // BASIC_QUERY_PARSER_H_
