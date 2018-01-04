#include "query_parser.h"

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

#include <cstdlib>

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

static const std::string kRegexArgument = R"((IN \([^)]+\)|'[^']*'|\b\d+(\.\d+)?\b))";
std::regex QueryParser::argument_pattern_ = std::regex(kRegexArgument, std::regex_constants::extended | std::regex_constants::optimize);

int QueryManager::GetIdForTemplate(const std::string &query_template) {
  auto iter = template_to_id_.find(query_template);
  if (iter != template_to_id_.end()) {
    return iter->second;
  }
  int id = static_cast<int>(template_to_id_.size());
  template_to_id_[query_template] = id;
  return id;
}

int QueryParser::GetQueryId(const std::string &sql) {
  std::string sql_template = ExtractTemplate(sql);
  return QueryManager::GetInstance().GetIdForTemplate(sql_template);
}

std::string QueryParser::ExtractTemplate(const std::string &sql) {
  std::string sql_template = trim(sql);
  sql_template = std::regex_replace(sql_template, argument_pattern_, "?v");
  return std::move(sql_template);
}
