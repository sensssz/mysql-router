#ifndef PREDICTION_PREDICTION_H_
#define PREDICTION_PREDICTION_H_

#include "operation.h"

#include <memory>

namespace model {

class Prediction {
public:
  Prediction(int query_id, std::vector<std::unique_ptr<Operation>> &&param_ops);
  Prediction(int query_id, int hit, std::vector<std::unique_ptr<Operation>> &&param_ops);

  bool MatchesQuery(const Window<Query> &trx, const Query &query);

  void Hit() {
    hit_count_++;
  }

  int query_id() const {
    return query_id_;
  }

  const std::vector<std::unique_ptr<Operation>> &param_ops() const {
    return param_ops_;
  }

  int hit_count() const {
    return hit_count_;
  }

private:
  int query_id_;
  std::vector<std::unique_ptr<Operation>> param_ops_;
  int hit_count_;
};

} // namespace model

#endif // PREDICTION_PREDICTION_H_
