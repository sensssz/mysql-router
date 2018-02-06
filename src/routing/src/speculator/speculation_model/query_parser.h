#ifndef BASIC_QUERY_PARSER_H_
#define BASIC_QUERY_PARSER_H_

#include "query.h"

#include <fstream>
#include <boost/regex.hpp>

namespace model {

class QueryParser {
public:
  Query ParseQuery(const std::string &json);

private:
  std::vector<std::string> RegexFindAll(const boost::regex &regex, std::string str);
  SqlValue CreateListValue(const std::vector<std::string> &values);
  std::vector<SqlValue> ConvertArguments(const std::vector<std::string> &args);
  std::string ExtractTemplate(const std::string &sql);

  static boost::regex argument_pattern_;
  static boost::regex num_str_pattern_;
  static boost::regex string_pattern_;
  static boost::regex number_pattern_;
  static boost::regex str_list_pattern_;
  static boost::regex num_list_pattern_;
};


} // namespace model

#endif // BASIC_QUERY_PARSER_H_
