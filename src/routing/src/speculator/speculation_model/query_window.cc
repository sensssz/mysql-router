#include "query_window.h"

#include <algorithm>

namespace model {

QueryWindow::QueryWindow() : Window<int>(kLookBackLen) {}

QueryPath QueryWindow::GenPath() {
  QueryPath path;
  path.fill(-1);
  for (std::size_t i = 0; i < Size(); i++) {
    path[i] = (*this)[i];
  }
  return std::move(path);
}

} // namespace model
