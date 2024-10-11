#ifndef SRC_DB_MEMTABLE_VIEW_MANAGER_H_
#define SRC_DB_MEMTABLE_VIEW_MANAGER_H_

#include "key_format.h"
#include "sstable_view.h"
#include "request.h"

#include <mutex>

namespace kdb {

class TableView;
//单层level的sstable
struct SSTableViewVec {
  std::vector<std::shared_ptr<TableView>> sstable_vec_;
  // std::mutex 只会在 push and get 之间上锁
  // 当需要改变 MemTable_view 的时候, 还是由单一的 MemTaskWorker 进行改变
  std::mutex mtx_;
  // 是否处于被Compaction 过程中
  std::atomic<bool> isCompaction_;
  // 尝试获得 MemTableViewVec 的锁
  bool getCompactLock() {
    bool expectedVal = false;
    return isCompaction_.compare_exchange_strong(expectedVal, true);
  }
  // 完成 Compaction 后标记当前level已经完成
  void finishCompact() { isCompaction_ = true; }
};

class SSTable;

class TableViewManager {
public:
  TableViewManager() = default;
  ~TableViewManager() = default;

  void Init(int level);

  /**
   * @number: 需要 push 的序列号
   * @data: mmap 文件起始地址
   * @data_size:
   * 当 compactor 完成 read_only_memtable => memtable 之后
   * 将视图放入这里.
   */
  void PushTableView(const uint32_t number,const char* data,
                     const uint32_t data_size,
                     const std::shared_ptr<SSTable>& sstable,
                     const std::string& min_key, const std::string& max_key);

  /**
   * @number: memtable的编号
   * @data: 
   * @data_size
   * 当 compactor 完成任意两层之间的 level 之间的合并时,
   * 都需要将合并之后的数据 Push进来.
   */
  void PushTableMergeView(const uint32_t number,const char* data,
                          const uint32_t data_size,
                          const uint32_t level_1,
                          const uint32_t level_2,
                          const std::shared_ptr<SSTable>& sstable,
                          const std::string& min_key, const std::string& max_key);
                          
  void RemoveTableFromLevel(uint32_t level, uint32_t sstable_number);

  /**
   * @number: 内存表的序列号
   * @get_context: 获取请求
   */
  void getRequest(
                  const std::shared_ptr<GetContext>& get_context);

  uint32_t getViewVecSize(const uint32_t level);

  bool Needs_L0_to_L1_Compaction();

  bool Needs_L1_to_L2_Compaction();

  std::shared_ptr<SSTableViewVec> getMemNVec(const uint32_t level) {
    return mem_all_vec_[level];
  }

  std::vector<std::shared_ptr<SSTable>> GetL0SSTable();
  std::shared_ptr<SSTable> GetL1SSTable();
 std::vector<std::shared_ptr<SSTable>> GetL1SSTables();
  std::vector<std::shared_ptr<SSTable>> GetL2SSTables();
  std::vector<std::shared_ptr<SSTable>> GetL1SSTable_() { return l1_sstables; }
  std::vector<std::shared_ptr<SSTable>> GetL2SSTable_() { return l2_sstables; }


  bool isLevelEmpty(uint32_t level);


private:
  std::vector<std::shared_ptr<SSTableViewVec>> mem_all_vec_;
  
  int levels;

  static const size_t L0_THRESHOLD = 4; // L0 layer max file count
  
  static const size_t L1_THRESHOLD = 10; 

  // 用于存储 L1 层所有的 SSTable
  std::vector<std::shared_ptr<SSTable>> l1_sstables;

  // 用于存储 L2 层所有的 SSTable
  std::vector<std::shared_ptr<SSTable>> l2_sstables;

};

}


#endif