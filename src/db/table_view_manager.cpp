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

bool TableViewManager::Needs_L0_to_L1_Compaction() {
    
    size_t L0_count = 0;
    
    for (const auto& sstable_view : mem_all_vec_[0]->sstable_vec_) {
        int level = sstable_view->getLevel();  

        if (level == 0) {
            // Count the number of files in L0
            L0_count++;
        }
        
        // Check if L0 needs compaction
        if (L0_count > L0_THRESHOLD) {
            return true;
        }
    }
    return false;
}

bool TableViewManager::Needs_L1_to_L2_Compaction() {
  size_t L1_count = 0;
  for (const auto& sstable_view : mem_all_vec_[1]->sstable_vec_) {
        int level = sstable_view->getLevel();  

        if (level == 1) {
            // Count the number of files in L0
            L1_count++;
        }
        
        // Check if L0 needs compaction
        if (L1_count > L1_THRESHOLD) {
            return true;
        }
   }
   return false;
}

std::vector<std::shared_ptr<SSTable>> TableViewManager::GetL0SSTable() {

    // 获取 L0 层的所有 SSTable
    auto& l0_sstables = mem_all_vec_[0]->sstable_vec_;
    if (l0_sstables.empty()) {
        return ;
    }
    std::vector<std::shared_ptr<SSTable>> earliest_l0_sstable;

    for(int i = 0;i < 3; ++i) {
        earliest_l0_sstable.push_back(l0_sstables[i]->getSStableRef());
    }

    // 返回最早的3个 SSTable 的智能指针
    return earliest_l0_sstable;
}

std::shared_ptr<SSTable> TableViewManager::GetL1SSTable() {

    // 获取 L1 层的所有 SSTable
    auto& l1_sstables = mem_all_vec_[1]->sstable_vec_;
    if (l1_sstables.empty()) {
        return nullptr;
    }

    // 假设第一个 SSTable 是最早的
    auto earliest_sstable = l1_sstables.front();

    // 遍历 L1 层的 SSTable，找到最早写入的 SSTable
    for (const auto& sstable : l1_sstables) {
        if (sstable->getNumber() < earliest_sstable->getNumber()) {
            earliest_sstable = sstable;
        }
    }

    // 返回最早的 SSTable 的智能指针
    return earliest_sstable->getSStableRef();
}

std::vector<std::shared_ptr<SSTable>> TableViewManager::GetL1SSTables() {
    // 用于存储 L1 层所有的 SSTable
    std::vector<std::shared_ptr<SSTable>> l1_sstables;

    // 获取 L1 层的所有 TableView
    auto& l1_table_views = mem_all_vec_[1]->sstable_vec_;
    if (l1_table_views.empty()) {
        return l1_sstables;  // 如果没有 L1 层文件，返回空的 vector
    }

    // 遍历 L1 层的 TableView，获取每个 TableView 关联的 SSTable
    for (const auto& table_view : l1_table_views) {
        l1_sstables.push_back(table_view->getSStableRef());  
    }

    return l1_sstables;
}

std::vector<std::shared_ptr<SSTable>> TableViewManager::GetL2SSTables() {
    // 用于存储 L2 层所有的 SSTable
    std::vector<std::shared_ptr<SSTable>> l2_sstables;

    // 获取 L2 层的所有 TableView
    auto& l2_table_views = mem_all_vec_[2]->sstable_vec_;
    if (l2_table_views.empty()) {
        return l2_sstables;  // 如果没有 L2 层文件，返回空的 vector
    }

    // 遍历 L2 层的 TableView，获取每个 TableView 关联的 SSTable
    for (const auto& table_view : l2_table_views) {
        l2_sstables.push_back(table_view->getSStableRef());  
    }

    return l2_sstables;
}

bool TableViewManager::isLevelEmpty(uint32_t level) {
  return mem_all_vec_[level]->sstable_vec_.empty();
}

void TableViewManager::PushTableView(const uint32_t level,
     const char *data, const uint32_t data_size,
    const std::shared_ptr<SSTable> &sstable,
    const std::string& min_key, const std::string& max_key) {
      assert(mem_all_vec_.size() > level);
  // btree_view_vec_[number].push_back(
  //     std::make_unique<MemTable_view>(data, data_size));
  auto cur_memtable_vec_size = mem_all_vec_[level]->sstable_vec_.size();
  auto memtable_view = std::make_shared<TableView>(
      data, data_size, level, /*cur_memtable_vec_size,*/ sstable, min_key, max_key);
  mem_all_vec_[level]->mtx_.lock();
  mem_all_vec_[level]->sstable_vec_.push_back(memtable_view);
  mem_all_vec_[level]->mtx_.unlock();
}

void TableViewManager::PushTableMergeView(const uint32_t number,
    const char *data, const uint32_t data_size,
    const uint32_t level_1, const uint32_t level_2,
    const std::shared_ptr<SSTable> &sstable,
    const std::string& min_key, const std::string& max_key) {
  auto cur_memtable_merged_view = std::make_shared<TableView>(
      data, data_size, number, level_1, level_2, sstable, min_key, max_key);
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

void TableViewManager::RemoveTableFromLevel(uint32_t level, uint32_t sstable_number) {
  // 确保该层存在
  if (level >= mem_all_vec_.size()) {
      std::cerr << "Invalid level: " << level << std::endl;
      return;
  }

  // 锁定该层，确保线程安全
  std::lock_guard<std::mutex> lock(mem_all_vec_[level]->mtx_);

  auto& sstable_vec = mem_all_vec_[level]->sstable_vec_;
  auto it = std::find_if(sstable_vec.begin(), sstable_vec.end(),
                          [sstable_number](const std::shared_ptr<TableView>& table_view) {
                              return table_view->getNumber() == sstable_number;
                          });

  // 如果找到对应的 SSTable
  if (it != sstable_vec.end()) {
      // 获取 SSTable 的路径
      std::shared_ptr<SSTable> sstable = (*it)->getSStableRef();

      // 从内存中移除 SSTable
      sstable_vec.erase(it);
      std::cout << "SSTable " << sstable_number << " removed from level " << level << std::endl;
    
      std::string file_name = sstable->getfileName();
      // 删除磁盘上的文件
      if (std::remove(file_name.c_str()) == 0) {
          std::cout << "File " << file_name << " successfully deleted." << std::endl;
      } else {
          std::cerr << "Failed to delete file: " << file_name << std::endl;
      }
  } else {
      std::cerr << "SSTable number " << sstable_number << " not found in level " << level << std::endl;
  }
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