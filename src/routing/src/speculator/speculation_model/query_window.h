#ifndef PREDICTION_QUERY_WINDOW_H_
#define PREDICTION_QUERY_WINDOW_H_

#include "window.h"

#include <array>

namespace model {

static const int kLookBackLen = 7;

using QueryPath = std::array<int, kLookBackLen>;

class QueryWindow : public Window<int> {
public:
  QueryWindow();
  QueryPath GenPath();
};

} // namespace model

#endif // PREDICTION_QUERY_WINDOW_H_
