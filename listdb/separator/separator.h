#ifndef LISTDB_SEPARATOR_SEPARATOR_H_
#define LISTDB_SEPARATOR_SEPARATOR_H_

#include "listdb/common.h"

enum class Temperature {
  kCold,
  kHot,
};

class Separator {
 public:
  virtual ~Separator() = default;
  virtual Temperature separate(const Key& key) = 0;
};

class MockSeparator : public Separator {
 public:
  MockSeparator();
  virtual ~MockSeparator() = default;
  Temperature separate(const Key& key) override;
};

MockSeparator::MockSeparator() {}

Temperature MockSeparator::separate(const Key& key) {
  if (key % 2 == 0) {
    return Temperature::kCold;
  } else {
    return Temperature::kHot;
  }
}

#endif  // LISTDB_SEPARATOR_SEPARATOR_H_
