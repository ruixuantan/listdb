#ifndef LISTDB_PMEM_PMEM_REGION_H_
#define LISTDB_PMEM_PMEM_REGION_H_

#include <experimental/filesystem>
#include <libpmemobj++/pexceptions.hpp>
#include <unordered_map>

#include "listdb/common.h"
#include "listdb/core/pmem_db.h"
#include "listdb/lsm/level_list.h"
#include "listdb/pmem/pmem_dir.h"

namespace fs = std::experimental::filesystem::v1;
using PmemDbPool = pmem::obj::pool<pmem_db>;
using PmemDbPtr = pmem::obj::persistent_ptr<pmem_db>;

/*
 * Manages the folder structure and pool ids of a
 * listdb region. A listdb region is either hot or cold.
 */
class PmemRegion {
 public:
  PmemRegion(std::string_view suffix);

  PmemDbPool InitRootPool();

  void CreateRootPool();

  void CreateLogPool();

  void InitSkiplistPool();

  PmemLog* GetL0PmemLog(const int region, const int shard);

  PmemLog* GetL1PmemLog(const int region, const int shard);

  void Clear();

 private:
  std::string_view suffix_;
  HCPmem pmem_;

  std::unordered_map<int, int> pool_id_to_region_;
  std::unordered_map<int, int> log_pool_id_;
  std::unordered_map<int, int> l0_pool_id_;
  std::unordered_map<int, int> l1_pool_id_;

  PmemLog* log_[kNumRegions][kNumShards];
  PmemLog* l0_arena_[kNumRegions][kNumShards];
  PmemLog* l1_arena_[kNumRegions][kNumShards];

  int InitPath(std::string path);
};

PmemRegion::PmemRegion(std::string_view suffix) : suffix_{suffix} {}

void PmemRegion::CreateRootPool() {
  PmemDbPool db_pool = InitRootPool();
  PmemDbPtr db_root = db_pool.root();

  for (int i = 0; i < kNumShards; ++i) {
    pmem::obj::persistent_ptr<pmem_db_shard> p_shard_manifest;
    pmem::obj::make_persistent_atomic<pmem_db_shard>(db_pool, p_shard_manifest);
    pmem::obj::persistent_ptr<pmem_l0_info> p_l0_manifest;
    pmem::obj::make_persistent_atomic<pmem_l0_info>(db_pool, p_l0_manifest);
    p_shard_manifest->l0_list_head = p_l0_manifest;
    db_root->shard[i] = p_shard_manifest;
  }
}

PmemDbPool PmemRegion::InitRootPool() {
  std::string db_path = PmemDir::db_path(suffix_);
  int root_pool_id = PmemRegion::InitPath(db_path);
  return pmem_.pool<pmem_db>(root_pool_id);
}

void PmemRegion::CreateLogPool() {
  for (int i = 0; i < kNumRegions; ++i) {
    std::string log_path = PmemDir::region_log_path(i, suffix_);
    fs::remove_all(log_path);
    fs::create_directories(log_path);
    std::string poolset = PmemDir::region_log_poolset(i, suffix_);
    PmemDir::write_poolset_config(log_path, poolset);

    int pool_id = pmem_.BindPoolSet<pmem_log_root>(poolset, "");
    pool_id_to_region_[pool_id] = i;
    log_pool_id_[i] = pool_id;
    pmem::obj::pool<pmem_log_root> pool = pmem_.pool<pmem_log_root>(pool_id);

    for (int j = 0; j < kNumShards; ++j) {
      log_[i][j] = new PmemLog(pool_id, j, pmem_);
    }
  }
}

void PmemRegion::InitSkiplistPool() {
  for (int i = 0; i < kNumRegions; ++i) {
    l0_pool_id_[i] = log_pool_id_[i];
    for (int j = 0; j < kNumShards; ++j) {
      l0_arena_[i][j] = log_[i][j];
    }
  }

  for (int i = 0; i < kNumRegions; ++i) {
    for (int j = 0; j < kNumShards; ++j) {
      l1_arena_[i][j] = l0_arena_[i][j];
    }
  }

  for (int i = 0; i < kNumRegions; ++i) {
    l1_pool_id_[i] = l1_arena_[i][0]->pool_id();
  }
}

PmemLog* PmemRegion::GetL0PmemLog(const int region, const int shard) {
  return l0_arena_[region][shard];
}

PmemLog* PmemRegion::GetL1PmemLog(const int region, const int shard) {
  return l1_arena_[region][shard];
}

int PmemRegion::InitPath(std::string path) {
  fs::remove_all(path);
  int root_pool_id = pmem_.BindPool<pmem_db>(path, "", 64 * 1024 * 1024);
  if (root_pool_id != 0) {
    std::cerr << "root_pool_id of: '" << path
              << "' must be zero (current: " << root_pool_id << ")\n";
    exit(1);
  }
  return root_pool_id;
}

void PmemRegion::Clear() {
  for (int i = 0; i < kNumShards; ++i) {
    for (int j = 0; j < kNumRegions; ++j) {
      delete log_[j][i];
    }
  }
  pmem_.Clear();
}

#endif  // LISTDB_PMEM_PMEM_REGION_H_
