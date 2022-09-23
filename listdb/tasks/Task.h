#ifndef LISTDB_TASKS_TASK_H_
#define LISTDB_TASKS_TASK_H_

#include "listdb/lsm/memtable_list.h"
#include "listdb/lsm/pmemtable.h"
#include "listdb/lsm/pmemtable_list.h"
#include "listdb/util/random.h"

struct Task {
  TaskType type;
  int shard;
};

struct MemTableFlushTask : Task {
  MemTable* imm;
  MemTableList* memtable_list;
};

struct L0CompactionTask : Task {
  PmemTable* l0;
  MemTableList* memtable_list;
};

struct alignas(64) CompactionWorkerData {
  int id;
  bool stop;
  Random rnd = Random(0);
  std::queue<Task*> q;
  std::mutex mu;
  std::condition_variable cv;
  Task* current_task;
  uint64_t flush_cnt = 0;
  uint64_t flush_time_usec = 0;
  // uint64_t compaction_cnt = 0;
  // uint64_t compaction_time_usec = 0;
};

enum class ServiceStatus {
  kActive,
  kStop,
};

#endif  //  LISTDB_TASKS_TASK_H_
