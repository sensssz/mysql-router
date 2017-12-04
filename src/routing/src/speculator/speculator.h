#ifndef SPECULATOR_SPECULATOR_H_
#define SPECULATOR_SPECULATOR_H_

#include <string>
#include <vector>

class Speculator {
public:
  virtual ~Speculator() {}
  virtual void SkipQuery() = 0;
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations=3) = 0;
};

#endif // SPECULATOR_SPECULATOR_H_