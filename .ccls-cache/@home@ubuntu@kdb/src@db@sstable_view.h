#ifndef SRC_DB_MEMTABLE_VIEW_H_
#define SRC_DB_MEMTABLE_VIEW_H_

#include "key_format.h"
#include "request.h"
#include "../include/bloomfilter.h"
#include "../utils/coding.h"
#include "sstable.h"

namespace kdb {

class SSTable;
struct CompactInfo {
  // 第一层 merge 的层 number
  uint32_t firstLevel;
  // 第二层 merge 的层 number
  uint32_t secondLevel;
  // 当 Compactor 完成 mino Compaction 之后
  // MemTaskWorker 需要将 合并之后的结果
  // 替换掉原有的2个 memtable_view
  bool IsInstead;
};

class TableView {
public:
    TableView() : isReadable_(false) {}
  ~TableView();
  //

  TableView(const char* data, const uint32_t data_size,
                const uint32_t lev, const std::shared_ptr<SSTable>& sstable,
                const std::string& min_key, const std::string& max_key);

  //
  TableView(const char* data,
                const uint32_t data_size,
                const uint32_t number,
                const uint32_t level_1,
                const uint32_t level_2,
                const std::shared_ptr<SSTable>& sstable,
                const std::string& min_key, const std::string& max_key);

  bool Init(std::string_view mmap_view);

  //////////////////////////////////////////////////void Reset(const char* data, const uint32_t data_size);

  void Get(const std::shared_ptr<GetContext>& get_context);

  bool isReadAble() { return isReadable_; }

  // 当前 memTable_view 只读
  void setReadOnly() { isReadable_ = true; }

  void setNumber(const uint32_t n);

  uint32_t getNumber() { return cur_number_; };

  void setLevel(const uint32_t n);

  uint32_t getLevel();

  auto getMemViewPtr() {
    return &memMapView_;
  }

  auto getCurVersion() {
    return version_;
  }

  auto getBloomFilterPtr() {
    return bloomFilter_.get();
  }

  auto getBloomFilterSize() {
    return bloomFilter_->getBitSetSize();
  }

  uint32_t getSize() {
    return sstable_ref_->getFileSize();
  }

  const std::string& GetMinKey() const { return min_key_; }

  const std::string& GetMaxKey() const { return max_key_; }

private:
  std::string min_key_;  // 最小键
  std::string max_key_;  // 最大键
  
private:
  std::shared_ptr<SSTable> sstable_ref_;
  
  // 当前映射的 version
  uint32_t version_;

  // mmap 映射的指针
  const char* tableViewPtr_;

  // mmap 映射的大小
  uint32_t tableViewSize_;

  uint32_t cur_level_;
  
  uint32_t cur_number_;
  // 当前是否可读.
  bool isReadable_;

  // std::set<std::string_view>
  MemBTreeView memMapView_;

  std::unique_ptr<BloomFilter<>> bloomFilter_;

  // compaction 的信息.
  CompactInfo info_;
  
};
  

    
}


#endif