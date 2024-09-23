#ifndef SRC_DB_MEMTABLE_H_
#define SRC_DB_MEMTABLE_H_

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <atomic>
#include <shared_mutex>

#include "key_format.h"
//#include "src/utils/arena.h"
#include "../utils/arena.h"
#include "request.h"


namespace kdb {
// 默认一个 Memtable 的最大容量是 128 MB
// 超过该容量即可变成 一个 read_only_memtable
static constexpr uint32_t kDefaultMemtableSize = 128 * 1024 * 1024;

class Memtable {
public:
  Memtable();
  ~Memtable() = default;

  void Set(const std::shared_ptr<SetContext>& set_context);

  void Get(const std::shared_ptr<GetContext>& get_context);

  void Delete(const std::shared_ptr<DeleteContext>& del_context);
  
  uint32_t getMemSize();

  MemSkipTable& getMemTableRef();

  // 让当前 Memtable 只读
  void setReadOnly();

  // 设置 Memtable 的编号
  void setNumber(const uint32_t number);

  // 获取当前内存表的编号
  uint32_t getMemNumber() const;

  // 增加一次引用计数
  void addRefs();

  // 获取引用计数
  uint32_t getRefs();

  // 减少引用计数
  void decreaseRefs();

  auto getCompactionN() { return compaction_number_; }

  auto setCompactionN(const uint32_t n) { compaction_number_ = n; }

  
private:
  // 是否只是可读
  // 当一个 Memtable 写到一定容量之时, 便应该成为一个
  // ImmuTable, 等待后台线程做 Minor_Compaction.
  // false 当前还没有写满
  // true 即可变成
  bool isReadonly_;

  // 当前 memtable 的编号
  uint64_t memtable_number_;

  // 当前 memtable 第几次被 compaction;
  uint32_t compaction_number_;
    
    // memtable 当写到固定阈值的时候
  // 会成为一个 read_only_memtable
  // 但是查找的时候, 可能会去从 read_only_memtable
  // 为了在合适的时候刷入, 需要自动维护一个引用计数
  std::atomic<uint32_t> refs_;
  
  // 存储数据的内存表

  Arena arena_; 
  //MemBTree memMap_;
  
  MemSkipTable table_;

  
};
    
} //namespace kdb

#endif