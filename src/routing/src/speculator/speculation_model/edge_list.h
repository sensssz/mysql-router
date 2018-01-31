#ifndef PREDICTION_EDGE_LIST_H_
#define PREDICTION_EDGE_LIST_H_

#include "edge.h"

namespace model {

class EdgeList {
public:
  std::shared_ptr<Edge> GetEdge(int query_id);
  std::shared_ptr<Prediction> FindBestPrediction(const QueryPath &path);

private:
  std::unordered_map<int, Edge> edges_;
};


} // namespace model

#endif // PREDICTION_EDGE_LIST_H_
