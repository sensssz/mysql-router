#ifndef BASIC_CONST_OPERAND_H_
#define BASIC_CONST_OPERAND_H_

#include "operand.h"

namespace model {

class ConstOperand : public Operand {
public:
  ConstOperand(SqlValue value) : value_(std::move(value)) {}
  // ConstOperand(SqlValue &&value) : value_(std::move(value)) {}

  virtual void SetQueryIndex(int query_index) {}

  virtual SqlValue GetValue(const Window<Query> &trx) const {
    return value_;
  }

  virtual std::string ToString() const {
    return value_.ToString();
  }

  virtual Operand *Clone() {
    return new ConstOperand(value_);
  }

  virtual SqlValue value() {
    return value_;
  }

  virtual bool operator==(const Operand &other) const {
    auto other_op = dynamic_cast<const ConstOperand *>(&other);
    if (other_op == nullptr) {
      return false;
    }
    return value_ == other_op->value_;
  }

private:
  SqlValue value_;
};

} // namespace model

#endif // BASIC_CONST_OPERAND_H_
