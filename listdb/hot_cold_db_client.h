#ifndef HOT_COLD_LISTDB_DB_CLIENT_H_
#define HOT_COLD_LISTDB_DB_CLIENT_H_

#include <algorithm>
#include <vector>

#include "listdb/common.h"
#include "listdb/hot_cold_listdb.h"
#include "listdb/util.h"
#include "listdb/util/random.h"

#define LEVEL_CHECK_PERIOD_FACTOR 1

class HotColdDBClient {
 public:
  using MemNode = HotColdListDB::MemNode;
  using PmemNode = HotColdListDB::PmemNode;

  HotColdDBClient(int region);

  ~HotColdDBClient();

  void Put(const Key& key, const Value& value);

 private:
  HotColdListDB* db_;
};

HotColdDBClient::HotColdDBClient(int region) : db_{new HotColdListDB(region)} {
  db_->Init();
}

HotColdDBClient::~HotColdDBClient() { delete db_; }

void HotColdDBClient::Put(const Key& key, const Value& value) {
  db_->Put(key, value);
}

#endif  // HOT_COLD_LISTDB_DB_CLIENT_H_
