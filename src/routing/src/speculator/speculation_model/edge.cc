#include "edge.h"

namespace model {

Edge::Edge(int to) : to_(to), weight_(0) {}

std::shared_ptr<Prediction> Edge::FindBestMatchWithPath(const QueryPath &path) {
  Prediction *best = nullptr;
  auto &predictions = predictions_[path];
  for (auto &prediction : predictions) {
    if (best == nullptr ||
        prediction.hit_count() > best->hit_count()) {
      best = &prediction;
    }
  }
  return std::shared_ptr<Prediction>(best);
}

std::vector<Prediction *> Edge::FindMatchingPredictions(
  const std::vector<Query> &previous_queries,
  const Query &query, const QueryPath &path) {
  std::vector<Prediction *> matches;
  if (to_ != query.query_id()) {
    return std::move(matches);
  }

  auto &predictions = predictions_[path];
  for (auto &prediction : predictions) {
    if (prediction.MatchesQuery(previous_queries, query)) {
      matches.push_back(&prediction);
    }
  }
  return std::move(matches);
}

void Edge::AddPredictions(const Query &query, const QueryPath &path,
  std::vector<Prediction> &predictions) {
  auto &predictions_of_path = predictions_[path];
  for (auto &prediction : predictions) {
    predictions_of_path.push_back(std::move(prediction));
  }
}

} // namespace model
