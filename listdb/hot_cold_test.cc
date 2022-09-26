#include <iostream>

#include "listdb/hot_cold_db_client.h"
#include "listdb/listdb.h"

int main() {
  HotColdDBClient* db = new HotColdDBClient(0);

  db->Put(1, 1);
  db->Put(5, 5);
  db->Put(10, 10);

  delete db;
  return 0;
}
