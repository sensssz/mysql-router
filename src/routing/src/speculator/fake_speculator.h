#ifndef SPECULATOR_FAKE_SPECULATOR_H_
#define SPECULATOR_FAKE_SPECULATOR_H_

#include "speculator.h"

#include <random>
#include <vector>

class FakeSpeculator : public Speculator {
public:
  FakeSpeculator(const std::string &filename);
  virtual void CheckBegin(const std::string &query) override;
  virtual void SkipQuery() override {
    if (start_) {
      current_query_++;
    }
  }
  virtual void SetQueryIndex(int query_index) override {
    current_query_ = query_index;
  }
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations=1) override;

private:
  std::vector<std::string> queries_;
  bool start_;
  int current_query_;
  std::default_random_engine rand_gen_;
  std::uniform_int_distribution<int> dist_;

  int ExtractKeyValue(const std::string &query);
};

#endif // SPECULATOR_FAKE_SPECULATOR_H_