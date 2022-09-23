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

  void Close();

 private:
  PmemSection pmem_mgr_;
};

HotColdListDB::HotColdListDB() : pmem_mgr_{PmemSection()} {}

HotColdListDB::~HotColdListDB() {
  fprintf(stdout, "HotColdListDB closed\n");
  Close();
}

void HotColdListDB::Init() { pmem_mgr_.Init(); }

void HotColdListDB::Close() { pmem_mgr_.Clear(); }

#endif  // HOT_COLD_LISTDB_LISTDB_H_
