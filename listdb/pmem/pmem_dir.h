#ifndef LISTDB_PMEM_PMEM_DIR_H_
#define LISTDB_PMEM_PMEM_DIR_H_

#include <fstream>
#include <sstream>

#include "listdb/common.h"
#include "listdb/pmem/pmem.h"

namespace PmemDir {

constexpr std::string_view kHotSuffix = "_hot";
constexpr std::string_view kColdSuffix = "_cold";

std::string db_path(std::string_view suffix) {
  std::stringstream pss;
  pss << kPathPrefix << "listdb" << suffix;
  return pss.str();
};

std::string region_log_path(int region_number, std::string_view suffix) {
  std::stringstream pss;
  pss << kPathPrefix << region_number << suffix << "/listdb_log";
  return pss.str();
};

std::string region_log_poolset(int region_number, std::string_view suffix) {
  return region_log_path(region_number, suffix) + ".set";
};

void write_poolset_config(const std::string path, const std::string poolset) {
  std::fstream strm(poolset, strm.out);
  strm << "PMEMPOOLSET" << std::endl;
  strm << "OPTION SINGLEHDR" << std::endl;
  strm << "400G " << path << "/" << std::endl;
  strm.close();
};

};  // namespace PmemDir

#endif  // LISTDB_PMEM_PMEM_DIR_H_
