#ifndef HOT_COLD_LISTDB_DB_CLIENT_H_
#define HOT_COLD_LISTDB_DB_CLIENT_H_

#include <algorithm>
#include <vector>

#include "listdb/common.h"
#include "listdb/listdb.h"
#include "listdb/util.h"
#include "listdb/util/random.h"

#define LEVEL_CHECK_PERIOD_FACTOR 1

class HotColdDBClient {
 public:
  using MemNode = ListDB::MemNode;
  using PmemNode = ListDB::PmemNode;

  HotColdDBClient(ListDB* db, int id, int region);

  void Put(const Key& key, const Value& value);

 private:
};

#endif  // HOT_COLD_LISTDB_DB_CLIENT_H_
