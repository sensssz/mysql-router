#ifndef SRC_SPECULATOR_H_
#define SRC_SPECULATOR_H_

#include "speculator.h"
#include "speculation_model/graph_model.h"

class ModelSpeculator : public Speculator {
public:
  virtual void CheckBegin(const std::string &query) override;
  virtual void SkipQuery() override;
  virtual int GetQueryIndex() override;
  virtual void SetQueryIndex(int query_index) override;
  virtual void BackupFor(const std::string &query) override;
  virtual std::string GetUndo() override;
  virtual std::vector<std::string> Speculate(const std::string &query) override;
  virtual std::vector<std::string> Speculate(const std::string &query, int num_speculations) override;
  virtual std::vector<std::string> TrySpeculate(const std::string &query, int num_speculations) override;

private:

};

#endif // SRC_SPECULATOR_H_