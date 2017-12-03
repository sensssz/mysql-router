#ifndef SPECULATOR_FAKE_SPECULATOR_H_
#define SPECULATOR_FAKE_SPECULATOR_H_

#include "speculator.h"

#include <vector>

class FakeSpeculator : public Speculator {
public:
  FakeSpeculator(const std::string &filename);
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations=3);

private:
  static const std::string kQuery1Template;

  int ExtractKeyValue(const std::string &query);
};

#endif // SPECULATOR_FAKE_SPECULATOR_H_