#include "synthetic_speculator.h"

#include <fstream>

const int kMinTime = 1001;
const int kMaxTime = 1100;
const int kAvgTime = 1050;
const int kQueryCount = kMaxTime - kMinTime + 1;
const int kThinkTime = 100;

static std::string GetQueryAllEqual() {
  auto sleep_time = std::to_string(static_cast<double>(kAvgTime) * 1e-6);
  return "SELECT SLEEP(" + sleep_time + ");";
}

static std::string GetQueryBad(int i) {
  double sleep_time = 0;
  sleep_time = (i % 2 == 0) ? kMinTime : kAvgTime + i / 2;
  return "SELECT SLEEP(" + std::to_string(sleep_time * 1e-6) + ");";
}

SyntheticSpeculator::SyntheticSpeculator() : start_(false), current_query_(0) {}

void SyntheticSpeculator::CheckBegin(const std::string &query) {
  if (!start_ && query.find("SLEEP") != std::string::npos) {
    start_ = true;
  }
}

std::vector<std::string> SyntheticSpeculator::Speculate(const std::string &query, int num_speculations) {
  std::vector<std::string> speculations;
  // return std::move(speculations);
  if (!start_ || current_query_ == -1) {
    return speculations;
  }
  current_query_++;
  speculations.push_back(GetQueryAllEqual());
  return std::move(speculations);
}
