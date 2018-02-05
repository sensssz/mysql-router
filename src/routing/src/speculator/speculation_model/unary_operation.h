#ifndef BASIC_UNARY_OPERATION_H_
#define BASIC_UNARY_OPERATION_H_

#include "operation.h"
#include "operand.h"

#include <memory>

namespace model {

class UnaryOperation : public Operation {
public:
  UnaryOperation(Operand &operand) : operand_(operand.Clone()) {}
  UnaryOperation(Operand *operand) : operand_(operand->Clone()) {}
  UnaryOperation(std::unique_ptr<Operand> &&operand) : operand_(std::move(operand)) {}

  virtual SqlValue GetValue(const Window<Query> &trx) const {
    return operand_->GetValue(trx);
  }

  virtual bool MatchesValue(const Window<Query> &trx, const SqlValue &value) const {
    return GetValue(trx) == value;
  }

  virtual std::string ToString() const {
    return operand_->ToString();
  }

private:
  std::unique_ptr<Operand> operand_;
};

} // namespace model

#endif // BASIC_UNARY_OPERATION_H_
