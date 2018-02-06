#include "predictor.h"

namespace model {

Predictor::Predictor(std::shared_ptr<GraphModel> model,
          std::shared_ptr<QueryManager> query_manager) :
  model_(model), current_query_(-1),
  history_(kLookBackLen),
  query_manager_(query_manager) {}

std::string Predictor::PredictNextSQL() {
  auto query = PredictNextQuery();
  if (query.get() == nullptr) {
    return "";
  }
  return query->ToSql();
}

std::unique_ptr<Query> Predictor::PredictNextQuery() {
  if (history_.Size() == 0) {
    return nullptr;
  }
  auto path = query_window_.GenPath();
  auto best_match = model_->GetEdgeList(current_query_)->FindBestPrediction(path);
  if (best_match.get() == nullptr) {
    return nullptr;
  }
  std::vector<SqlValue> arguments;
  for (auto &operation : best_match->param_ops()) {
    arguments.push_back(operation->GetValue(history_));
  }
  std::vector<std::vector<SqlValue>> result_set;
  return new Query(best_match->query_id(), std::move(arguments), std::move(result_set));
}

void Predictor::MoveToNext(Query &&query) {
  current_query_ = query.query_id();
  query_window_.Add(query.query_id());
  history_.Add(std::move(query));
}

} // namespace model
