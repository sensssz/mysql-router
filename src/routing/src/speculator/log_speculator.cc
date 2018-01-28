#include "log_speculator.h"
#include "logger.h"

#include <fstream>

LogSpeculator::LogSpeculator(const std::string &filename) :
    start_(false), current_query_(0), has_speculation_(false),
    rand_gen_(rd_()), dist_(1, 100), rand_index_(rd_()) {
  std::ifstream infile(filename);
  if (infile.fail()) {
    return;
  }
  std::string line;
  while (!infile.eof()) {
    std::getline(infile, line);
    queries_.push_back(line);
  }
  index_dist_ = std::uniform_int_distribution<int>(0, static_cast<int>(queries_.size() - 1));
}

void LogSpeculator::CheckBegin(const std::string &query) {
  if (!start_ && query == "BEGIN") {
    start_ = true;
  }
}

std::vector<std::string> LogSpeculator::Speculate(const std::string &query, int num_speculations) {
  auto speculations = TrySpeculate(query, num_speculations);
  current_query_++;
  has_speculation_ = false;
  speculations_.clear();
  indices_.clear();
  return std::move(speculations);
}

std::vector<std::string> LogSpeculator::TrySpeculate(const std::string &query, int num_speculations) {
  if (has_speculation_) {
    return speculations_;
  }
  // return speculations_;
  if (!start_ || current_query_ == -1) {
    return speculations_;
  }
  has_speculation_ = true;
  int rand_num = dist_(rand_gen_);
  if (rand_num <= 58) {
    auto &next_query = queries_[current_query_ + 1];
    if (next_query.find("BEGIN") == std::string::npos &&
        next_query.find("COMMIT") == std::string::npos) {
      log_debug("Will make prediction hit with %s", next_query.c_str());
      speculations_.push_back(next_query);
      indices_.push_back(current_query_ + 1);
    } else {
      log_debug("Cannot predict BEGIN or COMMIT");
    }
    num_speculations--;
  }
  while (num_speculations > 0) {
    auto index = index_dist_(rand_index_);
    auto query = queries_[index];
    if (query == "BEGIN" || query == "COMMIT") {
      continue;
    }
    speculations_.push_back(query);
    indices_.push_back(index);
    num_speculations--;
  }

  return speculations_;
}

virtual std::vector<int> LogSpeculator::GetSpeculationIndices() {
  return indices_;
}
