#include "query_parser.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

#include <cstdlib>

namespace model {

namespace rjson = rapidjson;

static SqlValue CreateValue(rjson::GenericValue<rjson::UTF8<>> &&value) {
  if (value.IsNull()) {
    return SqlValue(Null());
  } else if (value.IsBool()) {
    return SqlValue(value.GetBool());
  } else if (value.IsInt()) {
    return SqlValue(value.GetInt());
  } else if (value.IsDouble()) {
    return SqlValue(value.GetDouble());
  } else if (value.IsString()) {
    return SqlValue(value.GetString());
  }
  return SqlValue(Null());
}

// trim from start
static inline std::string &ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(),
          std::not1(std::ptr_fun<int, int>(std::isspace))));
  return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
          std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
  return s;
}

// trim from both ends
static inline std::string trim(std::string s) {
  ltrim(rtrim(s));
  return s;
}

static const std::string kRegexArgument = R"((?<!OFFSET )(?<!LIMIT )(IN \([^)]+\)|'[^']*'|\b\d+(\.\d+)?\b))";
static const std::string kRegexNumStr = R"((?<!OFFSET )(?<!LIMIT )('[^']*'|\b\d+(\.\d+)?\b))";
static const std::string kRegexString = R"(\'[^']*')";
static const std::string kRegexNumber = R"(\b\d+(\.\d+)?\b)";
static const std::string kRegexStrList = R"(IN \(([^)']+)\))";
static const std::string kRegexNumList = R"(IN \(([^)0-9]+)\))";

boost::regex QueryParser::argument_pattern_ = boost::regex(kRegexArgument);
boost::regex QueryParser::num_str_pattern_ = boost::regex(kRegexNumStr);
boost::regex QueryParser::string_pattern_ = boost::regex(kRegexString);
boost::regex QueryParser::number_pattern_ = boost::regex(kRegexNumber);
boost::regex QueryParser::str_list_pattern_ = boost::regex(kRegexStrList);
boost::regex QueryParser::num_list_pattern_ = boost::regex(kRegexNumList);

Query QueryParser::ParseQuery(const std::string &json) {
  rjson::Document document;
  document.Parse(json.c_str());
  std::string sql = document["sql"].GetString();
  std::vector<std::vector<SqlValue>> results;
  if (!document["results"].IsObject()) {
    for (auto &result_row : document["results"].GetArray()) {
      std::vector<SqlValue> row;
      for (auto &value : result_row.GetArray()) {
        row.push_back(std::move(CreateValue(std::move(value))));
      }
      results.push_back(std::move(row));
    }
  }
  std::string sql_template = ExtractTemplate(sql);
  int query_id = QueryManager::GetInstance().GetIdForTemplate(sql_template);
  auto args = ConvertArguments(RegexFindAll(argument_pattern_, sql));
  return Query(query_id, std::move(args), std::move(results));
}

std::vector<std::string> QueryParser::RegexFindAll(const boost::regex &regex, std::string str) {
  std::vector<std::string> all_matches;
  for (boost::sregex_iterator iter(str.begin(), str.end(), regex), end; iter != end; iter++) {
    all_matches.push_back(iter->str());
  }
  return std::move(all_matches);
}

SqlValue QueryParser::CreateListValue(const std::vector<std::string> &values) {
  if (values.size() == 0) {
    return SqlValue();
  }
  if (values[0].find('\'') != std::string::npos) {
    StringList strs;
    for (auto &value : values) {
      strs.insert(value.substr(1, value.length() - 2));
    }
    return SqlValue(std::move(strs));
  } else {
    DoubleList numbers;
    for (auto &value : values) {
      numbers.insert(::atof(value.c_str()));
    }
    return SqlValue(std::move(numbers));
  }
}

std::vector<SqlValue> QueryParser::ConvertArguments(const std::vector<std::string> &args) {
  std::vector<SqlValue> arguments;
  for (auto &arg : args) {
    if (arg.find(" IN ") != std::string::npos) {
      arguments.push_back(CreateListValue(RegexFindAll(argument_pattern_, arg)));
    } else if (arg.find('\'' != std::string::npos)) {
      arguments.push_back(arg.substr(1, arg.length() - 2));
    } else {
      arguments.push_back(SqlValue(Double(::atof(arg.c_str()))));
    }
  }
  return std::move(arguments);
}

std::string QueryParser::ExtractTemplate(const std::string &sql) {
  std::string sql_template = trim(sql);
  sql_template = boost::regex_replace(sql_template, string_pattern_, "?v", boost::match_default | boost::format_all);
  sql_template = boost::regex_replace(sql_template, number_pattern_, "?v", boost::match_default | boost::format_all);
  sql_template = boost::regex_replace(sql_template, str_list_pattern_, "?v", boost::match_default | boost::format_all);
  sql_template = boost::regex_replace(sql_template, num_list_pattern_, "?v", boost::match_default | boost::format_all);
  return std::move(sql_template);
}

} // namespace model
