#include "prediction.h"

namespace model {

Prediction::Prediction(int query_id, std::vector<Operation> &&param_ops) :
    query_id_(query_id), param_ops_(std::move(param_ops)) {}

bool Prediction::MatchesQuery(const Window<Query> &trx, const Query &query) {
  if (query_id_ != query.query_id()) {
    return false;
  }
  for (size_t i = 0; i < query.arguments().size(); i++) {
    if (!param_ops_[i].MatchesValue(trx, query.arguments[i])) {
      return false;
    }
  }
  return true;
}

} // namespace model
