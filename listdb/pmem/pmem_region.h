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

/*
 * Manages the folder structure and pool ids of a
 * listdb region. A listdb region is either hot or cold.
 */
class PmemRegion {
  using PmemDbPool = pmem::obj::pool<pmem_db>;
  using PmemDbPtr = pmem::obj::persistent_ptr<pmem_db>;

 public:
  PmemRegion(std::string_view suffix);

  void CreateRootPool();

  void CreateLogPool();

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
  LevelList* ll_[kNumShards];

  int InitPath(std::string path);
};

PmemRegion::PmemRegion(std::string_view suffix) : suffix_{suffix} {}

void PmemRegion::CreateRootPool() {
  std::string db_path = PmemDir::db_path(suffix_);
  int root_pool_id = PmemRegion::InitPath(db_path);
  PmemDbPool db_pool = pmem_.pool<pmem_db>(root_pool_id);
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
