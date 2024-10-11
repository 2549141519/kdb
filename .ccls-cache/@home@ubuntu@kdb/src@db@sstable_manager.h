#ifndef SRC_DB_SSTABLE_MANAGER_H_
#define SRC_DB_SSTABLE_MANAGER_H_

#include <memory>
#include <vector>

#include "sstable.h"
#include "configure.h"

namespace kdb 
{
class SSTableManager {
public:
  void Init(const DBConfig& db_config);

  auto getVecSize();

  auto getMemLevel(const uint32_t number);

  auto getSSTable(const uint32_t number,const uint32_t level);
private:
  // 有多少个 memtable
  uint32_t memtable_n_;

  // 数据库的地址
  std::string db_path_;

  // sstable 的最大层数
  uint32_t max_sstable_level_;

  std::vector<std::vector<std::shared_ptr<SSTable>>> sstable_vec_;
  // 为 mino Compaction 准备的 vec
  // 暂时存放
  std::vector<std::vector<std::shared_ptr<SSTable>>> comp_sstable_vec_;

  auto newSSTable(const uint32_t number, const uint32_t level);

  auto newCompactionSSTable(const uint32_t number,
                                          const uint32_t level);    
}; 
}

#endif