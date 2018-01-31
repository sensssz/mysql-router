#ifndef BASIC_RANDOM_OPERATION_H_
#define BASIC_RANDOM_OPERATION_H_

#include "operation.h"

namespace model {

class RandomOperation : public Operation {
public:
  virtual SqlValue GetValue(const Window<Query> &trx) const {
    return SqlValue();
  }

  virtual bool MatchesValue(const Window<Query> &trx, const SqlValue &value) const {
    return true;
  }

  virtual std::string ToString() const {
    return R"(
{
  "type": "random"
})";
  }

};

} // namespace model

#endif // BASIC_RANDOM_OPERATION_H_
