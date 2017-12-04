#include "fake_speculator.h"

#include <fstream>

static std::vector<std::string> fake_speculations{
  "SELECT  `keystores`.* FROM `keystores`  WHERE `keystores`.`key` = 'traffic:date'  ORDER BY `keystores`.`key` ASC LIMIT 1",
  "SELECT  `keystores`.* FROM `keystores`  WHERE `keystores`.`key` = 'traffic:hits'  ORDER BY `keystores`.`key` ASC LIMIT 1",
  "SELECT  `keystores`.* FROM `keystores`  WHERE `keystores`.`key` = 'user:2:unread_messages'  ORDER BY `keystores`.`key` ASC LIMIT 1"
};

FakeSpeculator::FakeSpeculator(const std::string &filename) : current_query_(0), dist_(1, 100) {
  std::ifstream infile(filename);
  if (infile.fail()) {
    return;
  }
  std::string line;
  while (!infile.eof()) {
    std::getline(infile, line);
    queries_.push_back(line);
  }
}

std::vector<std::string> FakeSpeculator::Speculate(const std::string &query, int num_speculations) {
  std::vector<std::string> speculations;
  int rand_num = dist_(rand_gen_);
  current_query_++;
  if (rand_num <= 57) {
    speculations.push_back(queries_[current_query_]);
    num_speculations--;
  }
  speculations.insert(speculations.end(), fake_speculations.begin(), fake_speculations.begin() + num_speculations);

  return std::move(speculations);
}
