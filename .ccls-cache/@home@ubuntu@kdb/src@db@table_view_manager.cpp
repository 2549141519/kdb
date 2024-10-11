#include "key_format.h"
#include "sstable_view.h"
#include "sstable.h"
#include "status.h"
#include "table_view_manager.h"
#include <assert.h>
namespace kdb {
void TableViewManager::Init(int levels) {
  this->levels = levels;
  mem_all_vec_.resize(levels);
  for (auto &iter : mem_all_vec_) {
    iter = std::make_shared<SSTableViewVec>();
  }
}

uint32_t TableViewManager::getViewVecSize(const uint32_t level) {
  return mem_all_vec_[level]->sstable_vec_.size();
}

void TableViewManager::PushTableView(const uint32_t level,
     const char *data, const uint32_t data_size,
    const std::shared_ptr<SSTable> &sstable) {
      assert(mem_all_vec_.size() > level);
  // btree_view_vec_[number].push_back(
  //     std::make_unique<MemTable_view>(data, data_size));
  auto cur_memtable_vec_size = mem_all_vec_[level]->sstable_vec_.size();
  auto memtable_view = std::make_shared<TableView>(
      data, data_size, level, cur_memtable_vec_size, sstable);
  mem_all_vec_[level]->mtx_.lock();
  mem_all_vec_[level]->sstable_vec_.push_back(memtable_view);
  mem_all_vec_[level]->mtx_.unlock();
}

void TableViewManager::PushTableMergeView(const uint32_t number,
    const char *data, const uint32_t data_size,
    const uint32_t level_1, const uint32_t level_2,
    const std::shared_ptr<SSTable> &sstable) {
  auto cur_memtable_merged_view = std::make_shared<TableView>(
      data, data_size, level_1, level_2, sstable);
  assert(level_1 < mem_all_vec_[levels]->sstable_vec_.size());
  assert(level_2 < mem_all_vec_[levels]->sstable_vec_.size());
  mem_all_vec_[level_1]->mtx_.lock();
  mem_all_vec_[level_2]->mtx_.lock();
  //
  // 锁住, 将 level_1 和 level_2 之间的memtable_view 释放掉.
  mem_all_vec_[level_1]->sstable_vec_.erase(
      mem_all_vec_[level_1]->sstable_vec_.begin() + number);
  mem_all_vec_[level_2]->sstable_vec_.clear();
  //TODO: should change the merged info?
  mem_all_vec_[level_1]->mtx_.unlock();
  mem_all_vec_[level_2]->mtx_.unlock();
}

void TableViewManager::getRequest(
  const std::shared_ptr<GetContext>& get_context) {
  
  for (int i = 0; i < mem_all_vec_.size(); ++i) {
    std::unique_lock<std::mutex> lock(mem_all_vec_[i]->mtx_);
    
    for (auto r_iter = mem_all_vec_[i]->sstable_vec_.rbegin();
          r_iter != mem_all_vec_[i]->sstable_vec_.rend(); ++r_iter) {
        
        (*r_iter)->Get(get_context);

        // 如果找到了数据，输出并返回
        if (get_context->code.getCode() == StatusCode::kOk ||
            get_context->code.getCode() == StatusCode::kDelete) {
            std::cout << "Found a match! Status code: "
                      << (get_context->code.getCode() == StatusCode::kOk ? "Ok" : "Delete")
                      << std::endl;
            return;
        }
    }
  }
}



}