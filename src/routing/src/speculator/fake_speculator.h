#ifndef SPECULATOR_FAKE_SPECULATOR_H_
#define SPECULATOR_FAKE_SPECULATOR_H_

#include "speculator.h"

#include <random>
#include <vector>

class FakeSpeculator : public Speculator {
public:
  FakeSpeculator(const std::string &filename);
  void SkipQuery() {
    current_query_++;
  }
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations=3);

private:
  std::vector<std::string> queries_;
  size_t current_query_;
  std::default_random_engine rand_gen_;
  std::uniform_int_distribution<int> dist_;

  int ExtractKeyValue(const std::string &query);
};

#endif // SPECULATOR_FAKE_SPECULATOR_H_