#ifndef PREDICTION_EDGE_H_
#define PREDICTION_EDGE_H_

#include "prediction.h"
#include "query_window.h"

#include <list>
#include <memory>
#include <unordered_map>

namespace model {

class Edge {
public:
  Edge(int to);
  Edge(int to, int weight);
  void IncWeight() {
    weight_++;
  }

  std::shared_ptr<Prediction> FindBestMatchWithPath(const QueryPath &path);

  std::vector<Prediction *> FindMatchingPredictions(
    const Window<Query> &previous_queries,
    const Query &query, const QueryPath &path);

  void AddPredictions(const QueryPath &path,
    const std::vector<Prediction> &predictions);

private:
  int to_;
  int weight_;
  std::unordered_map<QueryPath, Window<Prediction>> predictions_;
};

} // namespace model

#endif // PREDICTION_EDGE_H_
