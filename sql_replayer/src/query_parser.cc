#include "query_parser.h"

#include <algorithm>
#include <fstream>
#include <functional>

#include <cctype>
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
std::regex QueryParser::argument_pattern_ = std::regex(kRegexArgument, std::regex_constants::optimize);

int QueryManager::GetIdForTemplate(const std::string &query_template) {
  auto iter = template_to_id_.find(query_template);
  if (iter != template_to_id_.end()) {
    return iter->second;
  }
  int id = static_cast<int>(template_to_id_.size());
  template_to_id_[query_template] = id;
  return id;
}

void QueryManager::Dump(const std::string &filename) {
  std::ofstream template_file(filename);
  std::vector<std::pair<int, std::string>> templates;
  for (auto pair : template_to_id_) {
    templates.push_back(std::make_pair(pair.second, pair.first));
  }
  std::sort(templates.begin(), templates.end(),
    [](const std::pair<int, std::string> &t1,
       const std::pair<int, std::string> &t2) {
      return t1.first < t2.first;
    });
  for (auto &pair : templates) {
    template_file << pair.first << ',' << pair.second << std::endl;
  }
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

void QueryParser::DumpTemplates(const std::string &filename) {
  QueryManager::GetInstance().Dump(filename);
}
