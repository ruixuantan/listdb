#include <iostream>

#include "listdb/db_client.h"
#include "listdb/hot_cold_listdb.h"
#include "listdb/listdb.h"

int main() {
  HotColdListDB* db = new HotColdListDB();
  db->Init();

  delete db;
  return 0;
}
