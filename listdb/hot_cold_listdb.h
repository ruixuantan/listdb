#ifndef HOT_COLD_LISTDB_LISTDB_H_
#define HOT_COLD_LISTDB_LISTDB_H_

#include <numa.h>

#include <deque>
#include <experimental/filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <libpmemobj++/pexceptions.hpp>
#include <queue>
#include <sstream>
#include <stack>
#include <thread>
#include <unordered_map>

#include "listdb/common.h"
#include "listdb/core/double_hashing_cache.h"
#include "listdb/core/linear_probing_hashtable_cache.h"
#include "listdb/core/pmem_blob.h"
#include "listdb/core/pmem_db.h"
#include "listdb/core/pmem_log.h"
#include "listdb/core/static_hashtable_cache.h"
#include "listdb/index/braided_pmem_skiplist.h"
#include "listdb/index/lockfree_skiplist.h"
#include "listdb/index/simple_hash_table.h"
#include "listdb/lsm/level_list.h"
#include "listdb/lsm/memtable_list.h"
#include "listdb/lsm/pmemtable.h"
#include "listdb/lsm/pmemtable_list.h"
#include "listdb/pmem/pmem_dir.h"
#include "listdb/pmem/pmem_section.h"
#include "listdb/separator/separator.h"
#include "listdb/util/clock.h"
#include "listdb/util/random.h"
#include "listdb/util/reporter.h"
#include "listdb/util/reporter_client.h"

#define L0_COMPACTION_ON_IDLE
#define L0_COMPACTION_YIELD

//#define REPORT_BACKGROUND_WORKS
#ifdef REPORT_BACKGROUND_WORKS
#define INIT_REPORTER_CLIENT \
  auto reporter_client = new ReporterClient(reporter_)
#define REPORT_FLUSH_OPS(x) \
  reporter_client->ReportFinishedOps(Reporter::OpType::kFlush, x)
#define REPORT_COMPACTION_OPS(x) \
  reporter_client->ReportFinishedOps(Reporter::OpType::kCompaction, x)
#define REPORT_DONE delete reporter_client
#else
#define INIT_REPORTER_CLIENT
#define REPORT_FLUSH_OPS(x)
#define REPORT_COMPACTION_OPS(x)
#define REPORT_DONE
#endif

namespace fs = std::experimental::filesystem::v1;

class HotColdListDB {
 public:
  using MemNode = lockfree_skiplist::Node;
  using PmemNode = BraidedPmemSkipList::Node;

  using PmemDbPool = pmem::obj::pool<pmem_db>;
  using PmemDbPtr = pmem::obj::persistent_ptr<pmem_db>;

  HotColdListDB();
  ~HotColdListDB();

  void Init();

  void Open();

  void Put(const Key& key, const Value& value);

  void Close();

 private:
  int DramRandomHeight();

  int PmemRandomHeight();

  static int KeyShard(const Key& key);

  MemTable* GetWritableMemTable(size_t kv_size, int shard);

  MemTable* GetMemTable(int shard);

  std::array<PmemSection, kNumSections> pmems_;
  Random rnd_;
  Separator* separator_;

  LevelList* ll_[kNumShards];
};

HotColdListDB::HotColdListDB()
    : pmems_{{PmemSection(0), PmemSection(1)}},
      rnd_{Random(0)},
      separator_{new MockSeparator()} {}

HotColdListDB::~HotColdListDB() {
  fprintf(stdout, "HotColdListDB closed\n");
  Close();
}

void HotColdListDB::Init() {
  for (auto& pmem : pmems_) {
    pmem.Init(ll_);
  }
}

void HotColdListDB::Open() {}

void HotColdListDB::Close() {
  for (auto& pmem : pmems_) {
    pmem.Clear();
  }
}

void HotColdListDB::Put(const Key& key, const Value& value) {
  int shard = KeyShard(key);

  uint64_t pmem_height = PmemRandomHeight();
  size_t iul_entry_size =
      sizeof(PmemNode) + (pmem_height - 1) * sizeof(uint64_t);
  size_t kv_size = key.size() + sizeof(Value);

  // Determind l0 id
  MemTable* mem = GetWritableMemTable(kv_size, shard);
  uint64_t l0_id = mem->l0_id();

  Temperature temp = separator_->separate(key);
  // Write log
  // PmemPtr log_paddr = log_[shard]->Allocate(iul_entry_size);
  // PmemNode* iul_entry = static_cast<PmemNode*>(log_paddr.get());
}

inline int HotColdListDB::DramRandomHeight() {
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < kMaxHeight && ((rnd_.Next() % kBranching) == 0)) {
    height++;
  }
  return height;
}

inline int HotColdListDB::PmemRandomHeight() {
  static const unsigned int kBranching = 4;
  int height = 1;
  if (rnd_.Next() % std::max<int>(1, (kBranching / kNumRegions)) == 0) {
    height++;
    while (height < kMaxHeight && ((rnd_.Next() % kBranching) == 0)) {
      height++;
    }
  }
  return height;
}

inline int HotColdListDB::KeyShard(const Key& key) {
  return key.key_num() % kNumShards;
}

// Any table this function returns must be unreferenced manually
inline MemTable* HotColdListDB::GetWritableMemTable(size_t kv_size, int shard) {
  TableList* tl = ll_[shard]->GetTableList(0);
  Table* mem = tl->GetMutable(kv_size);
  return static_cast<MemTable*>(mem);
}

inline MemTable* HotColdListDB::GetMemTable(int shard) {
  TableList* tl = ll_[shard]->GetTableList(0);
  Table* mem = tl->GetFront();
  return static_cast<MemTable*>(mem);
}

#endif  // HOT_COLD_LISTDB_LISTDB_H_
