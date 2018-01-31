#include "edge_list.h"

namespace model {

std::shared_ptr<Edge> EdgeList::GetEdge(int query_id) {
  auto iter = edges_.find(query_id);
  if (iter == edges_.end()) {
    edges_.insert(std::make_pair(query_id, Edge(query_id)));
    return std::shared_ptr<Edge>(&edges_[query_id]);
  } else {
    return std::shared_ptr<Edge>(&iter->second);
  }
}

std::shared_ptr<Prediction> EdgeList::FindBestPrediction(const QueryPath &path) {
  std::shared_ptr<Prediction> best;

  for (auto &pair : edges_) {
    auto match = pair.second.FindBestMatchWithPath(path);
    if (match.get() == nullptr) {
      continue;
    }
    if (best == nullptr || match->hit_count() > best->hit_count()) {
      best = match;
    }
  }

  return std::move(best);
}

} // namespace model
