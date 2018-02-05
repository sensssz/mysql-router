#ifndef BASIC_VALUE_H_
#define BASIC_VALUE_H_

#include <iostream>
#include <set>
#include <string>
#include <boost/variant.hpp>

namespace model {

struct Null{};

class Double {
public:
  Double();
  Double(double value);

  bool operator==(const Double &other) const;
  bool operator!=(const Double &other) const;
  bool operator<(const Double &other) const;
  bool operator>(const Double &other) const;

  bool operator==(const double &other) const;
  bool operator!=(const double &other) const;
  bool operator<(const double &other) const;
  bool operator>(const double &other) const;
  friend std::ostream &operator<<(std::ostream &out, const Double &value);

  std::string ToString() const {
    return std::to_string(value_);
  }

private:
  double value_;
};

bool operator==(const double &lhs, const Double &rhs);
bool operator!=(const double &lhs, const Double &rhs);
bool operator<(const double &lhs, const Double &rhs);
bool operator>(const double &lhs, const Double &rhs);

using Bool = bool;
using Int = int64_t;
using String = std::string;
using IntList = std::set<int64_t>;
using DoubleList = std::set<Double>;
using StringList = std::set<std::string>;
using BoostVariant = boost::variant<Null, Bool, Int, Double, String, IntList, DoubleList, StringList>;

class SqlValue : public BoostVariant {
public:
  enum Type {
    kNull = 0,
    kBool = 1,
    kInt = 2,
    kDouble = 3,
    kString = 4,
    kIntList = 5,
    kDoubleList = 6,
    kStringList = 7,
  };
  SqlValue() = default;
  template<typename T>
  SqlValue(T &&value) : BoostVariant(std::move(value)) {}

  Type type() const {
    return static_cast<Type>(which());
  }
  bool IsNull() const {
    return which() == kNull;
  }
  bool IsBool() const {
    return which() == kBool;
  }
  bool IsInt() const {
    return which() == kInt;
  }
  bool IsDouble() const {
    return which() == kDouble;
  }
  bool IsString() const {
    return which() == kString;
  }
  bool IsIntList() const {
    return which() == kIntList;
  }
  bool IsDoubleList() const {
    return which() == kDoubleList;
  }
  bool IsStringList() const {
    return which() == kStringList;
  }
  bool IsList() const {
    return IsIntList() || IsDoubleList() || IsStringList();
  }

  std::string ToString() const;
  bool operator==(const SqlValue &other) const;
  bool operator<(const SqlValue &other) const;
};

std::ostream &operator<<(std::ostream &out, const SqlValue &value);

} // namespace model

#endif // BASIC_VALUE_H_
