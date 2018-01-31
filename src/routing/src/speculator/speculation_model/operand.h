#ifndef BASIC_OPERAND_H_
#define BASIC_OPERAND_H_

#include "query.h"
#include "window.h"

namespace model {

class Operand {
public:
  virtual ~Operand() {}
  virtual void SetQueryIndex(int query_index) = 0;
  virtual SqlValue GetValue(const Window<Query> &trx) const = 0;
  virtual std::string ToString() const = 0;
  virtual Operand *Clone() = 0;
  virtual bool operator==(const Operand &other) const = 0;
};

} // namespace model

#endif // BASIC_OPERAND_H_
