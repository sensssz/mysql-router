#ifndef SPECULATOR_LOG_SPECULATOR_H_
#define SPECULATOR_LOG_SPECULATOR_H_

#include "speculator.h"
#include "undoer.h"

#include <random>
#include <unordered_map>
#include <vector>

class LogSpeculator : public Speculator {
public:
  LogSpeculator(Undoer &&undoer, const std::string &filename);
  virtual void CheckBegin(const std::string &query) override;
  virtual void SkipQuery() override {
    if (start_) {
      current_query_++;
    }
  }
  virtual int GetQueryIndex() override {
    return current_query_;
  }
  virtual void SetQueryIndex(int query_index) override {
    current_query_ = query_index;
  }
  virtual void BackupFor(const std::string &query) override;
  virtual std::string GetUndo() override;
  virtual std::vector<std::string> Speculate(const std::string &query) override {
    return Speculate(query, 1);
  }
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations=1) override;
  virtual std::vector<std::string> TrySpeculate(const std::string &query, int num_speculations) override;

private:
  std::vector<std::string> queries_;
  Undoer undoer_;
  std::string next_undo_;
  bool start_;
  int current_query_;
  int previous_write_;
  bool has_speculation_;
  std::vector<int> indices_;
  std::vector<std::string> speculations_;
  std::random_device rd_;
  std::mt19937 rand_gen_;
  std::uniform_int_distribution<int> dist_;
  std::mt19937 rand_index_;
  std::uniform_int_distribution<int> index_dist_;

  int ExtractKeyValue(const std::string &query);
};

#endif // SPECULATOR_LOG_SPECULATOR_H_