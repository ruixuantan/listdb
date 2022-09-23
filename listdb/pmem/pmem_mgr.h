#ifndef LISTDB_PMEM_PMEM_MGR_H_
#define LISTDB_PMEM_PMEM_MGR_H_

#include "listdb/common.h"
#include "listdb/pmem/pmem_dir.h"
#include "listdb/pmem/pmem_region.h"

/*
 * Interface for managing the folder structure of the NVM
 */
class PmemMgr {
 public:
  PmemMgr();

  void Init();

  void Clear();

 private:
  PmemRegion hot_region_;
  PmemRegion cold_region_;
};

PmemMgr::PmemMgr()
    : hot_region_{PmemRegion(PmemDir::kHotSuffix)},
      cold_region_{PmemRegion(PmemDir::kColdSuffix)} {}

void PmemMgr::Init() {
  hot_region_.CreateRootPool();
  cold_region_.CreateRootPool();
  hot_region_.CreateLogPool();
  cold_region_.CreateLogPool();
}

void PmemMgr::Clear() {
  hot_region_.Clear();
  cold_region_.Clear();
}

#endif  // LISTDB_PMEM_PMEM_MGR_H_
