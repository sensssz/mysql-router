#include "log_speculator.h"
#include "logger.h"

#include <fstream>
#include <unordered_map>

#include <cassert>

LogSpeculator::LogSpeculator(Undoer &&undoer, const std::string &filename) :
    undoer_(undoer), start_(false), current_query_(0), previous_write_(-1),
    has_speculation_(false), rand_gen_(rd_()), dist_(1, 100), rand_index_(rd_()) {
  std::ifstream sql_file(filename);
  if (sql_file.fail()) {
    return;
  }
  std::string line;
  while (!sql_file.eof()) {
    std::getline(sql_file, line);
    queries_.push_back(line);
  }
  index_dist_ = std::uniform_int_distribution<int>(0, static_cast<int>(queries_.size() - 1));
}

void LogSpeculator::BackupFor(const std::string &query) {
  next_undo_ = undoer_.GetUndoQuery(query);
}

std::string LogSpeculator::GetUndo() {
  return next_undo_;
  // if (previous_write_ == -1) {
  //   return "";
  // }
  // auto &speculation = queries_[previous_write_];
  // return undoer_.GetUndoQuery(speculation);
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
  if (indices_.size() > 0 &&
      (speculations_[0].find("INSERT") == 0 ||
       speculations_[0].find("UPDATE") == 0)) {
    previous_write_ = indices_[0];
  }
  has_speculation_ = true;
  speculations_.clear();
  indices_.clear();
  int rand_num = dist_(rand_gen_);
  auto &next_query = queries_[current_query_ + 1];
  if (next_query == "BEGIN" || rand_num <= 58) {
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
    auto speculation = queries_[index];
    if (speculation == "BEGIN" || speculation == "COMMIT" || speculation.find("DELETE") == 0) {
      continue;
    }
    speculations_.push_back(speculation);
    indices_.push_back(index);
    num_speculations--;
  }

  return speculations_;
}

std::vector<int> LogSpeculator::GetSpeculationIndices() {
  return indices_;
}
