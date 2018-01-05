#ifndef SPECULATOR_SYNTHETIC_SPECULATOR_H_
#define SPECULATOR_SYNTHETIC_SPECULATOR_H_

#include "speculator.h"

#include <vector>

class SyntheticSpeculator : public Speculator {
public:
  SyntheticSpeculator();
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
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations) override;

private:
  bool start_;
  int current_query_;
};

#endif // SPECULATOR_SYNTHETIC_SPECULATOR_H_