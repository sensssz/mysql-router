#ifndef BASIC_ARGUMENT_LIST_OPERAND_H_
#define BASIC_ARGUMENT_LIST_OPERAND_H_

#include "operand.h"

#include <sstream>

#include <cassert>

namespace model {

class ArgumentListOperand : public Operand {
public:
  ArgumentListOperand(int query_id, int query_index, int arg_index) :
    query_id_(query_id), query_index_(query_index), arg_index_(arg_index) {}

  virtual void SetQueryIndex(int query_index) {
    query_index_ = query_index;
  }

  virtual SqlValue GetValue(const Window<Query> &trx) const {
    assert(query_index_ < trx.Size());
    auto &query = trx[query_index_];
    assert(query_id_ == query.query_id());
    assert(arg_index_ < query.arguments().size());
    return query.arguments()[arg_index_];
  }

  virtual std::string ToString() const {
    std::stringstream ss;
    ss << "Query" << query_id_ << '(' << arg_index_ << "l)";
    return ss.str();
  }

  virtual Operand *Clone() {
    return new ArgumentListOperand(query_id_, query_index_, arg_index_);
  }

  virtual bool operator==(const Operand &other) const {
    auto other_op = dynamic_cast<const ArgumentListOperand *>(&other);
    if (other_op == nullptr) {
      return false;
    }
    return query_id_ == other_op->query_id_ &&
           query_index_ == other_op->query_index_ &&
           arg_index_ == other_op->arg_index_;
  }

  int query_id() {
    return query_id_;
  }

  int query_index() {
    return query_index_;
  }

  int arg_index() {
    return arg_index_;
  }

private:
  int query_id_;
  int query_index_;
  int arg_index_;
};

} // namespace model

#endif // BASIC_ARGUMENT_LIST_OPERAND_H_
