#ifndef BASIC_OPERATION_H_
#define BASIC_OPERATION_H_

#include "query.h"
#include "operand.h"

namespace model {

class Operation {
public:
  virtual ~Operation() {}
  virtual SqlValue GetValue(const Window<Query> &trx) const = 0;
  virtual bool MatchesValue(const Window<Query> &trx, const SqlValue &value) const = 0;
  virtual std::string ToString() const = 0;
};

} // namespace model

#endif // BASIC_OPERATION_H_
