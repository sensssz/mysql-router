#ifndef PREDICTION_QUERY_WINDOW_H_
#define PREDICTION_QUERY_WINDOW_H_

#include "window.h"

#include <array>

namespace model {

static const int kLookBackLen = 7;

using QueryPath = std::array<int, kLookBackLen>;

namespace std {
  template<> struct hash<QueryPath> {
    size_t operator()(const QueryPath &path) const {
      sizs_t hash_val = 0;
      for (int i = 0; i < kLookBackLen; i++) {
        hash_val = hash_val ^ path[i];
      }
      return hash_val;
    }
  };
};

class QueryWindow : public Window<int> {
public:
  QueryWindow();
  QueryPath GenPath();
};

} // namespace model

#endif // PREDICTION_QUERY_WINDOW_H_
