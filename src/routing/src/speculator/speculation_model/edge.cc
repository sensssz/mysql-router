#include "edge.h"

namespace model {

Edge::Edge(int to) : to_(to), weight_(0) {}

Edge::Edge(int to, int weight): to_(to), weight_(weight) {}

std::shared_ptr<Prediction> Edge::FindBestMatchWithPath(const QueryPath &path) {
  Prediction *best = nullptr;
  auto &predictions = predictions_[path];
  for (size_t i = 0; i < predictions.Size(); i++) {
    auto &prediction = predictions[i];
    if (best == nullptr ||
        prediction.hit_count() > best->hit_count()) {
      best = &prediction;
    }
  }
  return std::shared_ptr<Prediction>(best);
}

std::vector<Prediction *> Edge::FindMatchingPredictions(
  const Window<Query> &previous_queries,
  const Query &query, const QueryPath &path) {
  std::vector<Prediction *> matches;
  if (to_ != query.query_id()) {
    return std::move(matches);
  }

  auto &predictions = predictions_[path];
  for (size_t i = 0; i < predictions.Size(); i++) {
    auto &prediction = predictions[i];
    if (prediction.MatchesQuery(previous_queries, query)) {
      matches.push_back(&prediction);
    }
  }
  return std::move(matches);
}

void Edge::AddPredictions(const QueryPath &path,
  std::vector<Prediction> &predictions) {
  auto &predictions_of_path = predictions_[path];
  for (auto &prediction : predictions) {
    predictions_of_path.Add(std::move(prediction));
  }
}

} // namespace model
