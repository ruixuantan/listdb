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

  using HotColdLog = std::unordered_map<Temperature, PmemLog* [kNumShards]>;
  using Allocators = std::unordered_map<Temperature, PmemAllocator*>;

  HotColdListDB(int region);
  ~HotColdListDB();

  void Init();

  void Open();

  void Put(const Key& key, const Value& value);

  void Close();

 private:
  int DramRandomHeight();

  int PmemRandomHeight();

  static int KeyShard(const Key& key);

  MemTable* GetWritableMemTable(size_t kv_size, int shard,
                                PmemAllocator* allocator);

  MemTable* GetMemTable(int shard);

  PmemSection pmem_;
  int region_;
  Random rnd_;
  Separator* separator_;
  Allocators allocators_;
  HotColdLog log_;
  LevelList* ll_[kNumShards];
};

HotColdListDB::HotColdListDB(int region)
    : pmem_{PmemSection()},
      region_{region},
      rnd_{Random(0)},
      separator_{new MockSeparator()} {}

HotColdListDB::~HotColdListDB() {
  delete separator_;
  fprintf(stdout, "HotColdListDB closed\n");
  Close();
}

void HotColdListDB::Init() {
  pmem_.Init(ll_);
  for (int i = 0; i < kNumShards; ++i) {
    log_[Temperature::kCold][i] = pmem_.cold_log(region_, i);
    log_[Temperature::kHot][i] = pmem_.hot_log(region_, i);
    allocators_[Temperature::kHot] = pmem_.hot_allocator();
    allocators_[Temperature::kCold] = pmem_.cold_allocator();
  }
}

void HotColdListDB::Open() {}

void HotColdListDB::Close() { pmem_.Clear(); }

void HotColdListDB::Put(const Key& key, const Value& value) {
  Temperature temp = separator_->separate(key);
  PmemAllocator* allocator = allocators_[temp];
  int shard = KeyShard(key);
  uint64_t pmem_height = PmemRandomHeight();
  size_t iul_entry_size =
      sizeof(PmemNode) + (pmem_height - 1) * sizeof(uint64_t);
  size_t kv_size = key.size() + sizeof(Value);

  // Determine l0 id
  MemTable* mem = GetWritableMemTable(kv_size, shard, allocator);
  uint64_t l0_id = mem->l0_id();

  // Write log
  PmemPtr log_paddr = log_[temp][shard]->Allocate(iul_entry_size);
  PmemNode* iul_entry = static_cast<PmemNode*>(log_paddr.get(allocator));

  // clwb things
  iul_entry->tag = (l0_id << 32) | pmem_height;
  iul_entry->value = value;
  clwb(&iul_entry->tag, 16);
  _mm_sfence();
  iul_entry->key = key;
  clwb(iul_entry, 8);

  // Create skiplist node
  uint64_t dram_height = DramRandomHeight();
  MemNode* node = static_cast<MemNode*>(
      malloc(sizeof(MemNode) + (dram_height - 1) * sizeof(uint64_t)));
  node->key = key;
  node->tag = (l0_id << 32) | dram_height;
  node->value = value;
  memset(static_cast<void*>(&node->next[0]), 0, dram_height * sizeof(uint64_t));

  lockfree_skiplist* skiplist = mem->skiplist();
  skiplist->Insert(node);
  mem->w_UnRef();
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
inline MemTable* HotColdListDB::GetWritableMemTable(size_t kv_size, int shard,
                                                    PmemAllocator* allocator) {
  TableList* tl = ll_[shard]->GetTableList(0);
  Table* mem = tl->GetMutable(kv_size, allocator);
  return static_cast<MemTable*>(mem);
}

inline MemTable* HotColdListDB::GetMemTable(int shard) {
  TableList* tl = ll_[shard]->GetTableList(0);
  Table* mem = tl->GetFront();
  return static_cast<MemTable*>(mem);
}

#endif  // HOT_COLD_LISTDB_LISTDB_H_
