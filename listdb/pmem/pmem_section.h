#ifndef LISTDB_PMEM_PMEM_MGR_H_
#define LISTDB_PMEM_PMEM_MGR_H_

#include "listdb/common.h"
#include "listdb/lsm/level_list.h"
#include "listdb/pmem/pmem_dir.h"
#include "listdb/pmem/pmem_region.h"
#include "listdb/tasks/Task.h"

namespace fs = std::experimental::filesystem::v1;

/*
 * Interface for managing a section of the NVM.
 * A section refers to 1 hot + 1 cold region.
 */
class PmemSection {
 public:
  PmemSection(int sect_id);

  void Init(LevelList** ll);

  void Open();

  void Clear();

  PmemLog* hot_log(const int region, const int shard);

  PmemLog* cold_log(const int region, const int shard);

  PmemAllocator* hot_allocator() { return hot_region_.allocator(); };

  PmemAllocator* cold_allocator() { return cold_region_.allocator(); };

 private:
  int sect_id_;
  PmemRegion hot_region_;
  PmemRegion cold_region_;

  std::deque<Task*> work_request_queue_;
  std::deque<Task*> work_completion_queue_;
  std::mutex wq_mu_;
  std::condition_variable wq_cv_;

  std::thread bg_thread_;
  bool stop_ = false;
  ServiceStatus l0_compaction_scheduler_status_ = ServiceStatus::kActive;

  CompactionWorkerData worker_data_[kNumWorkers];
  std::thread worker_threads_[kNumWorkers];

  void InitTableLists(LevelList** ll);

  void StartThreads();

  void BackgroundThreadLoop();

  void CompactionWorkerThreadLoop(CompactionWorkerData* td);
};

PmemSection::PmemSection(int sect_id)
    : sect_id_{sect_id},
      hot_region_{PmemRegion(sect_id, PmemDir::kHotSuffix)},
      cold_region_{PmemRegion(sect_id, PmemDir::kColdSuffix)} {}

void PmemSection::Init(LevelList** ll) {
  std::string section_path = PmemDir::section_path(sect_id_);
  fs::remove_all(section_path);
  fs::create_directories(section_path);
  hot_region_.CreateRootPool();
  cold_region_.CreateRootPool();
  hot_region_.CreateLogPool();
  cold_region_.CreateLogPool();
  hot_region_.InitSkiplistPool();
  cold_region_.InitSkiplistPool();
  InitTableLists(ll);
  StartThreads();
}

void PmemSection::Open() {
  hot_region_.InitRootPool();
  cold_region_.InitRootPool();
}

void PmemSection::InitTableLists(LevelList** ll) {
  for (int i = 0; i < kNumShards; ++i) {
    ll[i] = new LevelList();
    {
      auto tl = new MemTableList(kMemTableCapacity / kNumShards, i);
      tl->BindEnqueueFunction([&, tl, i](MemTable* mem) {
        auto task = new MemTableFlushTask();
        task->type = TaskType::kMemTableFlush;
        task->shard = i;
        task->imm = mem;
        task->memtable_list = tl;
        std::unique_lock<std::mutex> lk(wq_mu_);
        work_request_queue_.push_back(task);
        lk.unlock();
        wq_cv_.notify_one();
      });

      for (int j = 0; j < kNumRegions; ++j) {
        // Bind table list to hot region first
        tl->BindArena(j, hot_region_.GetL0PmemLog(j, i));
      }
      ll[i]->SetTableList(0, tl);
    }

    {
      auto tl = new PmemTableList(std::numeric_limits<size_t>::max(),
                                  hot_region_.GetL0PmemLog(0, 0)->pool_id());
      for (int j = 0; j < kNumRegions; ++j) {
        // Bind l1 table list to hot region first as well
        PmemLog* l1_pmem_log = hot_region_.GetL1PmemLog(j, i);
        tl->BindArena(l1_pmem_log->pool_id(), l1_pmem_log);
      }
      ll[i]->SetTableList(1, tl);
    }
  }
}

void PmemSection::StartThreads() {
  bg_thread_ = std::thread(std::bind(&PmemSection::BackgroundThreadLoop, this));

  for (int i = 0; i < kNumWorkers; ++i) {
    worker_data_[i].id = i;
    worker_data_[i].stop = false;
    worker_threads_[i] = std::thread(std::bind(
        &PmemSection::CompactionWorkerThreadLoop, this, &worker_data_[i]));
  }
}

void PmemSection::BackgroundThreadLoop() {
  std::cout << "Background thread loop" << std::endl;
}

void PmemSection::CompactionWorkerThreadLoop(CompactionWorkerData* td) {
  std::cout << "Compaction worker thread loop" << std::endl;
}

void PmemSection::Clear() {
  stop_ = true;
  if (bg_thread_.joinable()) {
    bg_thread_.join();
  }

  for (int i = 0; i < kNumWorkers; ++i) {
    worker_data_[i].stop = true;
    worker_data_[i].cv.notify_one();
    if (worker_threads_[i].joinable()) {
      worker_threads_[i].join();
    }
  }

  hot_region_.Clear();
  cold_region_.Clear();
}

PmemLog* PmemSection::hot_log(const int region, const int shard) {
  return hot_region_.log(region, shard);
}

PmemLog* PmemSection::cold_log(const int region, const int shard) {
  return cold_region_.log(region, shard);
}

#endif  // LISTDB_PMEM_PMEM_MGR_H_
