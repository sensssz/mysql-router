#ifndef BASIC_COLUMN_LIST_OPERAND_H_
#define BASIC_COLUMN_LIST_OPERAND_H_

#include "operand.h"

#include <sstream>

#include <cassert>

namespace model {

class ColumnListOperand : public Operand {
public:
  ColumnListOperand(int query_id, int query_index, int column_index) :
    query_id_(query_id), query_index_(query_index), column_index_(column_index) {}

  virtual void SetQueryIndex(int query_index) {
    query_index_ = query_index;
  }

  virtual SqlValue GetValue(const Window<Query> &trx) const {
    assert(query_index_ < trx.Size());
    auto &query = trx[query_index_];
    assert(query_id_ == query.query_id());
    assert(column_index_ < query.arguments().size());
    return query.arguments()[column_index_];
  }

  virtual std::string ToString() const {
    std::stringstream ss;
    ss << "Query" << query_id_ << '[' << column_index_ << 'l';
    return ss.str();
  }

  virtual Operand *Clone() {
    return new ColumnListOperand(query_id_, query_index_, column_index_);
  }

  virtual bool operator==(const Operand &other) const {
    auto other_op = dynamic_cast<const ColumnListOperand *>(&other);
    if (other_op == nullptr) {
      return false;
    }
    return query_id_ == other_op->query_id_ &&
           query_index_ == other_op->query_index_ &&
           column_index_ == other_op->column_index_;
  }

  int query_id() {
    return query_id_;
  }

  int query_index() {
    return query_index_;
  }

  int column_index() {
    return column_index_;
  }

private:
  int query_id_;
  int query_index_;
  int column_index_;
};

} // namespace model

#endif // BASIC_COLUMN_LIST_OPERAND_H_
