#include "synthetic_speculator.h"

#include <fstream>

const int kMinTime = 1000;
const int kMaxTime = 10000;
const int kAvgTime = (kMinTime + kMaxTime) / 2;
const int kQueryCount = 100;
const int kStep = (kMaxTime - kMinTime) / kQueryCount;

static std::string GetQueryAllEqual() {
  auto sleep_time = std::to_string(static_cast<double>(kAvgTime) * 1e-6);
  return "SELECT SLEEP(" + sleep_time + ");";
}

static std::string GetQueryBad(int i) {
  double sleep_time = 0;
  sleep_time = (i % 2 == 0) ? kMinTime : kAvgTime + i / 2 * kStep;
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
