#include "log_speculator.h"
#include "logger.h"

#include <fstream>

static std::vector<std::string> fake_speculations{
  "SELECT  `keystores`.* FROM `keystores`  WHERE `keystores`.`key` = 'traffic:date'  ORDER BY `keystores`.`key` ASC LIMIT 1",
  "SELECT  `keystores`.* FROM `keystores`  WHERE `keystores`.`key` = 'traffic:hits'  ORDER BY `keystores`.`key` ASC LIMIT 1",
  "SELECT  `keystores`.* FROM `keystores`  WHERE `keystores`.`key` = 'user:2:unread_messages'  ORDER BY `keystores`.`key` ASC LIMIT 1",
  "SELECT COUNT(*) FROM `invitation_requests`  WHERE `invitation_requests`.`is_verified` = 1",
  "SELECT `users`.* FROM `users`  WHERE `users`.`id` IN (9, 8, 6, 5, 4, 3, 2, 276, 274, 273, 272, 271, 265, 263, 262, 260, 255, 251, 247, 245)"
};

LogSpeculator::LogSpeculator(const std::string &filename) : start_(false), current_query_(0), dist_(1, 100) {
  std::ifstream infile(filename);
  if (infile.fail()) {
    return;
  }
  std::string line;
  while (!infile.eof()) {
    std::getline(infile, line);
    queries_.push_back(line);
    /*
    if (line.find("SELECT") == 0 ||
        line.find("BEGIN") == 0 ||
        line.find("COMMIT") == 0) {
      queries_.push_back(line);
    }
    */
  }
  index_dist_ = std::uniform_int_distribution<int>(0, static_cast<int>(queries_.size() - 1));
}

void LogSpeculator::CheckBegin(const std::string &query) {
  if (!start_ && query == "BEGIN") {
    start_ = true;
  }
}

std::vector<std::string> LogSpeculator::Speculate(const std::string &query, int num_speculations) {
  std::vector<std::string> speculations;
  // return std::move(speculations);
  if (!start_ || current_query_ == -1) {
    return speculations;
  }
  current_query_++;
  int rand_num = dist_(rand_gen_);
  if (rand_num <= 58) {
    auto &next_query = queries_[current_query_];
    if (next_query.find("SELECT") == 0) {
      log_debug("Will make prediction hit with %s", next_query.c_str());
      speculations.push_back(next_query);
    } else {
      log_debug("Cannot predict writes");
    }
    num_speculations--;
  }
  while (num_speculations > 0) {
    auto query = queries_[index_dist_(rand_index_)];
    if (query == "BEGIN" || query == "COMMIT") {
      continue;
    }
    speculations.push_back(query);
    num_speculations--;
  }

  return std::move(speculations);
}
