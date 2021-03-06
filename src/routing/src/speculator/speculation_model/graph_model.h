#ifndef PREDICTION_GRAPH_MODEL_H_
#define PREDICTION_GRAPH_MODEL_H_

#include "edge_list.h"
#include "predictor.h"
#include "query.h"
#include "query_parser.h"

#include <memory>

namespace model {

class Predictor;

class GraphModel {
public:
  GraphModel(std::shared_ptr<QueryManager> manager) : manager_(manager) {}

  void Load(const std::string &query_set, const std::string &model);

  EdgeList *GetEdgeList(int query_id) {
    return &vertex_edges_[query_id];
  }

  std::unique_ptr<Predictor> CreatePredictor();

private:
  std::shared_ptr<QueryManager> manager_;
  std::unordered_map<int, EdgeList> vertex_edges_;
};

} // namespace model

#endif // PREDICTION_GRAPH_MODEL_H_
