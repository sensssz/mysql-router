#ifndef BASIC_QUERY_RESULT_OPERAND_H_
#define BASIC_QUERY_RESULT_OPERAND_H_

#include "operand.h"

#include <sstream>

#include <cassert>

namespace model {

class QueryResultOperand : public Operand {
public:
  QueryResultOperand(int query_id, int query_index, int row_index, int column_index) :
    query_id_(query_id), query_index_(query_index),
    row_index_(row_index), column_index_(column_index) {}

  virtual void SetQueryIndex(int query_index) {
    query_index_ = query_index;
  }

  virtual SqlValue GetValue(const Window<Query> &trx) const {
    assert(query_index_ < trx.Size());
    auto &query = trx[query_index_];
    assert(query_id_ == query.query_id());
    if (static_cast<size_t>(row_index_) >= query.result_set().size()) {
      return SqlValue();
    }
    return query.result_set()[row_index_][column_index_];
  }

  virtual std::string ToString() const {
    std::stringstream ss;
    ss << "Query" << query_id_ << '[' << row_index_ << ',' << column_index_ << ']';
    return ss.str();
  }

  virtual Operand *Clone() {
    return new QueryResultOperand(query_id_, query_index_, row_index_, column_index_);
  }

  virtual bool operator==(const Operand &other) const {
    auto other_op = dynamic_cast<const QueryResultOperand *>(&other);
    if (other_op == nullptr) {
      return false;
    }
    return query_id_ == other_op->query_id_ &&
           query_index_ == other_op->query_index_ &&
           row_index_ == other_op->row_index_ &&
           column_index_ == other_op->column_index_;
  }

  int query_id() {
    return query_id_;
  }

  int query_index() {
    return query_index_;
  }

  int row_index() {
    return row_index_;
  }

  int column_index() {
    return column_index_;
  }

private:
  int query_id_;
  int query_index_;
  int row_index_;
  int column_index_;
};

} // namespace model

#endif // BASIC_QUERY_RESULT_OPERAND_H_
