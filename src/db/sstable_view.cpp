#include "sstable_view.h"

#include <memory>

extern "C" {
#include <assert.h>
}

#include "request.h"
#include "status.h"

/*
 | membloom_seed_ | membloom_size_ | bloom_filter_data |
 | kv_data|
*/

namespace kdb {


//这个构造函数是读取sstable并合并用的  另一个是读取sstable并get用的
TableView::TableView(const char* data,
                             const uint32_t data_size,
                             const uint32_t number,
                             const uint32_t level_1,
                             const uint32_t level_2,
                             const std::shared_ptr<SSTable>& sstable,
                             const std::string& min_key, const std::string& max_key)
              : min_key_(min_key), max_key_(max_key){
  cur_number_ = number;
  sstable_ref_ = sstable;
  // 该数据还没有被 MemTaskWorker 替换
  this->info_.firstLevel = level_1;
  this->info_.secondLevel = level_2;
  this->info_.IsInstead = false;
  // level_merged_memtable 的数据比较特殊.
 // mmap 映射的指针
  tableViewPtr_ = data;

  // mmap 映射的大小
  tableViewSize_ = data_size;

  // version
  auto end_ptr = GetVarint32Ptr(data, data + 5, &version_);
  uint64_t createTime;
  // create time
  end_ptr = GetVarint64Ptr(end_ptr, end_ptr + 9, &createTime);
  uint32_t cur_lev_1;
  // current version
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &cur_lev_1);
  //
  uint32_t cur_lev_2;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &cur_lev_2);
  // 验证
  assert(cur_lev_2 == level_1);
  //
  uint32_t all_key_size;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &all_key_size);
  uint32_t bloom_filter_size;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &bloom_filter_size);
  uint64_t bloom_filter_seed;
  //
  bloomFilter_ = std::make_unique<BloomFilter<>>(end_ptr,
    bloom_filter_size * 8, bloom_filter_seed);
  //
  end_ptr += bloom_filter_size;
  // 作为视图压入到 TableView 中.
  uint32_t key_size;
  uint32_t value_size;
  auto memViewEndPtr = tableViewPtr_ + tableViewSize_;
  while (end_ptr < memViewEndPtr) {
    auto begin_ptr = end_ptr;
    end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &key_size);
    auto end_2_ptr = end_ptr;
    assert(end_ptr);
    end_ptr += key_size;
    end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &value_size);
    // 表示没有 value
    if (end_ptr == nullptr) {
      std::string kv_str_view(end_2_ptr, key_size);
      memMapView_.insert(kv_str_view);
      end_ptr = end_2_ptr;
      continue;
    }
    end_ptr += value_size;
    std::string kv_str_view(begin_ptr, end_ptr - begin_ptr);
    memMapView_.insert(kv_str_view);
  }
  assert(all_key_size == memMapView_.size());
}

TableView::TableView(const char* data, const uint32_t data_size,
                             const uint32_t lev,
                             const std::shared_ptr<SSTable>& sstable,
                             const std::string& min_key, const std::string& max_key)
    : cur_level_(lev), min_key_(min_key), max_key_(max_key) {
  sstable_ref_ = sstable;
  this->Init(std::string_view(data, data_size));

}

bool TableView::Init(std::string_view mmap_view) {
  if(mmap_view.size() == 0){
    return false;
  }
  tableViewPtr_ = mmap_view.data();
  tableViewSize_ = mmap_view.size();
  auto end_ptr =
      GetVarint32Ptr(mmap_view.data(), (mmap_view.data() + 5), &version_);
  // TODO: check it
  uint64_t create_time;
  end_ptr = GetVarint64Ptr(end_ptr, end_ptr + 9, &create_time);
  uint32_t cur_lev;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &cur_lev);
  uint32_t cur_number;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &cur_number);
  uint32_t all_key_size;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &all_key_size);
  assert(cur_lev == cur_level_);
  //assert(cur_number == cur_number_);
  cur_number_ = cur_number;
  uint64_t bloomfilter_seed;
  //end_ptr = GetVarint64Ptr(end_ptr, mmap_view.data() + 5, &bloomfilter_seed);
  end_ptr = GetVarint64Ptr(end_ptr, end_ptr + 5, &bloomfilter_seed);
  uint32_t bloomfilter_size;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 4, &bloomfilter_size);
  bloomFilter_ = std::make_unique<BloomFilter<>>(
      end_ptr, bloomfilter_size * 8, bloomfilter_seed);
  end_ptr += bloomfilter_size;
  
  // 读取最小键
  uint32_t min_key_size;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &min_key_size);
  std::string_view min_key(end_ptr, min_key_size);
  end_ptr += min_key_size;
  min_key_ = min_key; 

  // 读取最大键
  uint32_t max_key_size;
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &max_key_size);
  std::string_view max_key(end_ptr, max_key_size);
  end_ptr += max_key_size;
  max_key_ = max_key; 
  
  // 作为视图压入到 TableView 中
  uint32_t key_size;
  uint32_t value_size;
  auto mmapViewEndPtr = (mmap_view.data() + mmap_view.size());
  while (end_ptr < mmapViewEndPtr) {
    auto begin_ptr = end_ptr;
    end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &key_size);
    assert(end_ptr);
    end_ptr += key_size;

    uint64_t sequence_number;
    end_ptr = GetVarint64Ptr(end_ptr, end_ptr + 9, &sequence_number);
    
    uint8_t type = DecodeFixed8(end_ptr);
    end_ptr += 1;
    if(type == ValueType::kTypeDeletion)
      continue;
    end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &value_size);
    end_ptr += value_size;
    std::string_view kv_str_view(begin_ptr, end_ptr - begin_ptr);
    std::string str(kv_str_view);
    memMapView_.insert(str);
  }
  //assert(all_key_size == memMapView_.size());
  return true;
}

void TableView::setLevel(const uint32_t n) { cur_level_ = n; }

uint32_t TableView::getLevel() { return cur_level_; }

void TableView::Get(const std::shared_ptr<GetContext>& get_context) {
  if (false == bloomFilter_->IsMatch(get_context->key)) {
    get_context->code.setCode(StatusCode::kNotFound);
    return;
  }
  auto pre_key_var_size = VarintLength(get_context->key.size());
  std::string simple_get_str =
      fmt::format("{}{}", format32_vec[pre_key_var_size], get_context->key);
  EncodeVarint32(simple_get_str.data(), get_context->key.size());

  auto find_iter = memMapView_.find(simple_get_str);
  if (find_iter == memMapView_.end()) {
    get_context->code.setCode(StatusCode::kNotFound);
    return;
  }
  if (find_iter->size() == simple_get_str.size()) {
    get_context->code.setCode(StatusCode::kDelete);
    return;
  }
  uint32_t key_size;
  auto end_ptr =
      GetVarint32Ptr(find_iter->data(), find_iter->data() + 5, &key_size);
  assert(end_ptr != nullptr);
  end_ptr += key_size;
  uint32_t value_size;
  //
  end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &value_size);
  get_context->value = std::string_view(end_ptr, value_size);
  get_context->code.setCode(StatusCode::kOk);
}
    
TableView::~TableView() {
  
}
};