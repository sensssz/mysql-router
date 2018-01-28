#ifndef SPECULATOR_LOG_SPECULATOR_H_
#define SPECULATOR_LOG_SPECULATOR_H_

#include "speculator.h"

#include <random>
#include <vector>

class LogSpeculator : public Speculator {
public:
  LogSpeculator(const std::string &filename);
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
  virtual std::vector<std::string> Speculate(const std::string &query) override {
    return Speculate(query, 1);
  }
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations=1) override;
  virtual std::vector<std::string> TrySpeculate(const std::string &query, int num_speculations) override;
  virtual std::vector<int> GetSpeculationIndices() override;

private:
  std::vector<std::string> queries_;
  bool start_;
  int current_query_;
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