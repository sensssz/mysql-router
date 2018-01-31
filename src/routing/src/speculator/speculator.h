#ifndef SPECULATOR_SPECULATOR_H_
#define SPECULATOR_SPECULATOR_H_

#include <string>
#include <vector>

class Speculator {
public:
  virtual ~Speculator() {}
  virtual void CheckBegin(const std::string &query) = 0;
  virtual void SkipQuery() = 0;
  virtual int GetQueryIndex() = 0;
  virtual void SetQueryIndex(int query_index) = 0;
  virtual std::string GetUndo() = 0;
  virtual std::vector<std::string> Speculate(const std::string &query) = 0;
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations) = 0;
  virtual std::vector<std::string> TrySpeculate(const std::string &query, int num_speculations) = 0;
  virtual std::vector<int> GetSpeculationIndices() = 0;
};

#endif // SPECULATOR_SPECULATOR_H_