#ifndef PREDICTION_PREDICTOR_H_
#define PREDICTION_PREDICTOR_H_

#include "graph_model.h"
#include "window.h"
#include "query_parser.h"

#include <list>
#include <memory>

namespace model {

class GraphModel;

class Predictor {
public:
  Predictor(std::shared_ptr<GraphModel> model,
            std::shared_ptr<QueryManager> query_manager);

  std::string PredictNextSQL();
  std::unique_ptr<Query> PredictNextQuery();

  void MoveToNext(Query &&query);

private:
  std::shared_ptr<GraphModel> model_;
  int current_query_;
  Window<Query> history_;
  QueryWindow query_window_;
  QueryParser query_parser_;
  std::shared_ptr<QueryManager> query_manager_;
};

} // namespace model

#endif // PREDICTION_PREDICTOR_H_
