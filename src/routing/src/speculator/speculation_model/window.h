#ifndef PREDICTION_WINDOW_H_
#define PREDICTION_WINDOW_H_

#include <algorithm>
#include <memory>

#include <cstddef>

namespace model {

template<typename T>
class Window {
public:
  Window(std::size_t size);
  virtual void Add(T &&val);
  virtual void Add(const T &val);

  virtual std::size_t CumulativeSize() const {
    return cumulative_size_;
  }

  virtual std::size_t Size() const {
    return std::min(size_, cumulative_size_);
  }

  T &operator[](std::size_t index);
  const T &operator[](std::size_t index) const;

private:
  std::size_t current_index_;
  std::size_t cumulative_size_;
  std::size_t size_;
  std::unique_ptr<T[]> elements_;
};

template<typename T>
Window<T>::Window(std::size_t size) :
  current_index_(0), cumulative_size_(0),
  size_(size), elements_(nullptr) {}

template<typename T>
void Window<T>::Add(T &&val) {
  if (elements_.get() == nullptr) {
    elements_.reset(new T[size_]);
  }
  cumulative_size_++;
  current_index_ = (current_index_ + 1) % size_;
  elements_.get()[current_index_] = std::move(val);
}

template<typename T>
void Window<T>::Add(const T &val) {
  if (elements_.get() == nullptr) {
    elements_.reset(new T[size_]);
  }
  cumulative_size_++;
  current_index_ = (current_index_ + 1) % size_;
  elements_.get()[current_index_] = val;
}

template<typename T>
T &Window<T>::operator[](std::size_t index) {
  index = (index + current_index_) % size_;
  return elements_.get()[current_index_];
}

template<typename T>
const T &Window<T>::operator[](std::size_t index) const {
  index = (index + current_index_) % size_;
  return elements_.get()[current_index_];
}

} // namespace model

#endif // PREDICTION_WINDOW_H_