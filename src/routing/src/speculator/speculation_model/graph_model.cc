#include "graph_model.h"
#include "random_operation.h"
#include "unary_operation.h"
#include "const_operand.h"
#include "query_result_operand.h"
#include "query_argument_operand.h"
#include "argument_list_operand.h"
#include "column_list_operand.h"
#include "rapidjson/document.h"

#include <fstream>

#include <cassert>

namespace rjson=rapidjson;

namespace {

model::QueryPath CreateQueryPath(const rjson::Value &obj) {
  model::QueryPath path;
  for (rjson::SizeType i = 0; i < obj.Size(); i++) {
    path[i] = obj[i].GetInt();
  }
  return std::move(path);
}

model::SqlValue GetValue(const rjson::Value &obj) {
  if (obj.IsInt()) {
    return model::SqlValue(obj.GetInt());
  } else if (obj.IsString()) {
    return model::SqlValue(obj.GetString());
  } else if (obj.IsDouble()) {
    return model::SqlValue(obj.GetDouble());
  } else if (obj.IsBool()) {
    return model::SqlValue(obj.GetBool());
  }
  return model::SqlValue();
}

std::unique_ptr<model::Operand> CreateQueryResultOperand(const rjson::Value &obj) {
  int query_id = obj["query"].GetInt();
  int query_index = obj["index"].GetInt();
  int row = obj["row"].GetInt();
  int column = obj["column"].GetInt();
  return std::unique_ptr<model::Operand>(
    new model::QueryResultOperand(query_id, query_index, row, column));
}

std::unique_ptr<model::Operand> CreateQueryArgumentOperand(const rjson::Value &obj) {
  int query_id = obj["query"].GetInt();
  int query_index = obj["index"].GetInt();
  int arg = obj["arg"].GetInt();
  return new std::unique_ptr<model::Operand>(
    model::QueryArgumentOperand(query_id, query_index, arg));
}

std::unique_ptr<model::Operand> CreateArgumentListOperand(const rjson::Value &obj) {
  int query_id = obj["query"].GetInt();
  int query_index = obj["index"].GetInt();
  int arg = obj["arg"].GetInt();
  return new std::unique_ptr<model::Operand>(
    model::ArgumentListOperand(query_id, query_index, arg));
}

std::unique_ptr<model::Operand> CreateColumnListOperand(const rjson::Value &obj) {
  int query_id = obj["query"].GetInt();
  int query_index = obj["index"].GetInt();
  int column = obj["column"].GetInt();
  return new std::unique_ptr<model::Operand>(
    model::ColumnListOperand(query_id, query_index, column));
}

std::unique_ptr<model::Operand> CreateOperand(const rjson::Value &obj) {
  assert(obj.IsObject());
  auto type = obj["type"].GetString();
  if (type == "const") {
    return new std::unique_ptr<model::Operand>(
      model::ConstOperand(GetValue(obj["value"])));
  } else if (type == "result") {
    return CreateQueryResultOperand(obj);
  } else if (type == "arg") {
    return CreateQueryArgumentOperand(obj);
  } else if (type == "arglist") {
    return CreateArgumentListOperand(obj);
  } else if (type == "columnlist") {
    return CreateColumnListOperand(obj);
  }
  return new std::unique_ptr<model::Operand>(
    model::ConstOperand(model::SqlValue()));
}

std::unique_ptr<model::Operation> CreateOperation(const rjson::Value &obj) {
  assert(obj.IsObject());
  if (obj["type"].GetString() == "rand") {
    return new std::unique_ptr<model::Operation>(
      model::RandomOperation());
  }
  return new std::unique_ptr<model::Operation>(
    model::UnaryOperation(CreateOperand(obj)));
}

std::vector<std::unique_ptr<model::Operand>> CreatePredictions(const rjson::Value &obj) {
  assert(obj.IsArray());
  std::vector<std::unique_ptr<model::Operand>> predictions;
  for (rjson::SizeType i = 0; i < obj.Size(); i++) {
    auto prediction = obj[i];
    int query = prediction["query"].GetInt();
    int hit = prediction["hit"].GetInt();
    auto ops = prediction["ops"];
    std::vector<std::unique_ptr<model::Operation>> operands;
    for (rjson::SizeType j = 0; j < ops.Size(); j++) {
      operands.push_back(std::move(CreateOperand(ops[j])));
    }
    predictions.push_back(model::Prediction(query, hit, std::move(operands)));
  }
  return std::move(predictions);
}

void FillPredictions(const rjson::Value &obj, model::Edge &edge) {
  assert(obj.IsArray());
  for (auto &mapping : obj) {
    auto path = CreateQueryPath(mapping["path"]);
    auto predictions = CreatePredictions(mapping["predictions"]);
    edge.AddPredictions(path, std::move(predictions));
  }
}

model::EdgeList CreateEdgeList(const rjson::Value &obj) {
  assert(obj.IsArray());
  model::EdgeList edge_list;
  for (rjson::SizeType i = 0; i < obj.Size(); i++) {
    auto vertex = obj["vertex"].GetInt();
    auto edge_obj = obj["edge"];
    model::Edge edge(edge_obj["to"].GetInt(), edge_obj["weight"].GetInt());
    FillPredictions(edge_obj["prediction_map"], edge);
    edge_list.AddEdge(vertex, std::move(edge));
  }
  return std::move(edge_list);
}

} // namespace

namespace model {

void GraphModel::Load(const std::string &query_set, const std::string &model) {
  manager_->Load(query_set);
  std::ifstream infile(filename);
  std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
  rjson::Document document;
  document.Parse(content);
  for (rjson::SizeType i = 0; i < document.Size(); i++) {
    auto vertex_edge = document[i];
    auto vertex = vertex_edge["vertex"].GetInt();
    auto edge_list = CreateEdgeList(vertex_edge["edgelist"]);
    vertex_edges_[vertex] = std::move(edge_list);
  }
}

} // namespace model