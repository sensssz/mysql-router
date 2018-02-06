#include "value.h"

#include <numeric>
#include <sstream>

#include <cmath>

namespace model {

template<typename T>
std::string ListToString(T &values, const std::string &separator) {
  if (values.size() == 0) {
    return "";
  }
  std::stringstream ss;
  auto iter = values.begin();
  ss << *iter;
  for (iter++; iter != values.end(); iter++) {
    ss << separator << *iter;
  }
  return ss.str();
}

Double::Double() : value_(0.0) {}
Double::Double(double value) : value_(value) {}

bool Double::operator==(const Double &other) const {
  return fabs(value_ - other.value_) < std::numeric_limits<double>::epsilon();
}

bool Double::operator!=(const Double &other) const {
  return !(*this == other);
}

bool Double::operator<(const Double &other) const {
  return *this != other && value_ < other.value_;
}

bool Double::operator>(const Double &other) const {
  return *this != other && value_ > other.value_;
}

bool Double::operator==(const double &other) const {
  return fabs(value_ - other) < std::numeric_limits<double>::epsilon();
}

bool Double::operator!=(const double &other) const {
  return !(*this == other);
}

bool Double::operator<(const double &other) const {
  return *this != other && value_ < other;
}

bool Double::operator>(const double &other) const {
  return *this != other && value_ > other;
}

bool operator==(const double &lhs, const Double &rhs) {
  return rhs == lhs;
}

bool operator!=(const double &lhs, const Double &rhs) {
  return rhs != lhs;
}

bool operator<(const double &lhs, const Double &rhs) {
  return rhs > lhs;
}

bool operator>(const double &lhs, const Double &rhs) {
  return rhs < lhs;
}

std::ostream &operator<<(std::ostream &out, const Double &value) {
  out << value.value_;
  return out;
}

std::string SqlValue::ToString() const {
  if (IsNull()) {
    return "null";
  } else if (IsString()) {
    return "'" + boost::get<String>(*this) + "'";
  } else if (IsBool()) {
    return std::to_string(boost::get<Bool>(*this));
  } else if (IsInt()) {
    return std::to_string(boost::get<Int>(*this));
  } else if (IsDouble()) {
    return boost::get<Double>(*this).ToString();
  } else if (IsIntList()) {
    return ListToString(boost::get<IntList>(*this), ",");
  } else if (IsDoubleList()) {
    return ListToString(boost::get<DoubleList>(*this), ",");
  } else if (IsStringList()) {
    return ListToString(boost::get<StringList>(*this), ",");
  }
  return "";
}

bool SqlValue::operator==(const SqlValue &other) const {
  const BoostVariant &other_variant = other;
  return *this == other_variant;
}

bool SqlValue::operator<(const SqlValue &other) const {
  const BoostVariant &other_variant = other;
  return *this < other_variant;
}

std::ostream &operator<<(std::ostream &out, const SqlValue &value) {
  out << value.ToString();
  return out;
}

} // namespace model
