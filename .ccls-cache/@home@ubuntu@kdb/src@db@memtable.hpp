#ifndef SRC_DB_MEMTABLE_H_
#define SRC_DB_MEMTABLE_H_

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <atomic>
#include <shared_mutex>

#include "key_format.hpp"


namespace kdb {
// 默认一个 Memtable 的最大容量是 128 MB
// 超过该容量即可变成 一个 read_only_memtable
    static constexpr uint32_t kDefaultMemtableSize = 128 * 1024 * 1024;

class Memtable {
public:

private:
  // 当前内存表所花费的内存
  uint32_t memtableSize_;
  
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
  //MemBTree memMap_;
  MemSkipTable table_;
  
};
    
} //namespace kdb

#endif