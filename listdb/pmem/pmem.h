#ifndef LISTDB_PMEM_PMEM_H_
#define LISTDB_PMEM_PMEM_H_

#include <iostream>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/pool.hpp>
#include <vector>

class Pmem {
 public:
  template <typename T>
  static int BindPool(const std::string& path, const std::string& layout,
                      const size_t size);

  static int BindPoolSet(const std::string& path, const std::string& layout);

  template <typename T>
  static int BindPoolSet(const std::string& path, const std::string& layout);

  static void Clear();

  static pmem::obj::pool_base pool(const int pool_base_id) {
    return *pool_bases_[pool_base_id];
  }
  template <typename T>
  static pmem::obj::pool<T> pool(const int pool_base_id) {
    // return *dynamic_cast<pmem::obj::pool<T>*>(pool_bases_[pool_base_id]);
    return *((pmem::obj::pool<T>*)pool_bases_[pool_base_id]);
  }

 private:
  inline static std::vector<pmem::obj::pool_base*> pool_bases_;
};

template <typename T>
int Pmem::BindPool(const std::string& path, const std::string& layout,
                   const size_t size) {
  pmem::obj::pool<T> pop;
  if (pmem::obj::pool_base::check(path, layout) == 1) {
    pop = pmem::obj::pool<T>::open(path, layout);
  } else {
    pop = pmem::obj::pool<T>::create(path, layout, size, 0666);
  }
  auto it = pool_bases_.insert(pool_bases_.end(), new pmem::obj::pool<T>(pop));
  return std::distance(pool_bases_.begin(), it);
}

int Pmem::BindPoolSet(const std::string& path, const std::string& layout) {
  pmem::obj::pool_base pop;
  if (pmem::obj::pool_base::check(path, layout) == 1) {
    pop = pmem::obj::pool_base::open(path, layout);
  } else {
    pop = pmem::obj::pool_base::create(path, layout, 0, 0666);
  }
  auto it =
      pool_bases_.insert(pool_bases_.end(), new pmem::obj::pool_base(pop));
  return std::distance(pool_bases_.begin(), it);
}

template <typename T>
int Pmem::BindPoolSet(const std::string& path, const std::string& layout) {
  pmem::obj::pool<T> pop;
  if (pmem::obj::pool_base::check(path, layout) == 1) {
    pop = pmem::obj::pool<T>::open(path, layout);
  } else {
    pop = pmem::obj::pool<T>::create(path, layout, 0, 0666);
  }
  auto it = pool_bases_.insert(pool_bases_.end(), new pmem::obj::pool<T>(pop));
  return std::distance(pool_bases_.begin(), it);
}

void Pmem::Clear() {
  for (auto& pool_base : pool_bases_) {
    pool_base->close();
  }
  pool_bases_.clear();
}

/*
 * Keeps track of the pool/pool_base objects instantiated
 * for binding in NVM, within a vector
 * Methods return index of the objects in the vector.
 */
class PmemAllocator {
 public:
  PmemAllocator() : pool_bases_{} {};

  ~PmemAllocator() { Clear(); };

  template <typename T>
  int BindPool(const std::string& path, const std::string& layout,
               const size_t size) {
    pmem::obj::pool<T> pop;
    if (pmem::obj::pool_base::check(path, layout) == 1) {
      pop = pmem::obj::pool<T>::open(path, layout);
    } else {
      pop = pmem::obj::pool<T>::create(path, layout, size, 0666);
    }
    auto it =
        pool_bases_.insert(pool_bases_.end(), new pmem::obj::pool<T>(pop));
    return std::distance(pool_bases_.begin(), it);
  }

  int BindPoolSet(const std::string& path, const std::string& layout) {
    pmem::obj::pool_base pop;
    if (pmem::obj::pool_base::check(path, layout) == 1) {
      pop = pmem::obj::pool_base::open(path, layout);
    } else {
      pop = pmem::obj::pool_base::create(path, layout, 0, 0666);
    }
    auto it =
        pool_bases_.insert(pool_bases_.end(), new pmem::obj::pool_base(pop));
    return std::distance(pool_bases_.begin(), it);
  };

  template <typename T>
  int BindPoolSet(const std::string& path, const std::string& layout) {
    pmem::obj::pool<T> pop;
    if (pmem::obj::pool_base::check(path, layout) == 1) {
      pop = pmem::obj::pool<T>::open(path, layout);
    } else {
      pop = pmem::obj::pool<T>::create(path, layout, 0, 0666);
    }
    auto it =
        pool_bases_.insert(pool_bases_.end(), new pmem::obj::pool<T>(pop));
    return std::distance(pool_bases_.begin(), it);
  };

  void Clear() {
    for (auto& pool_base : pool_bases_) {
      pool_base->close();
    }
    pool_bases_.clear();
  };

  pmem::obj::pool_base pool(const int pool_base_id) {
    return *pool_bases_[pool_base_id];
  }

  template <typename T>
  pmem::obj::pool<T> pool(const int pool_base_id) {
    return *((pmem::obj::pool<T>*)pool_bases_[pool_base_id]);
  }

 private:
  std::vector<pmem::obj::pool_base*> pool_bases_;
};

#endif  // LISTDB_PMEM_PMEM_H_
