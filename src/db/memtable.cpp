#include <fmt/core.h> //sudo apt install libfmt-dev


#include "memtable.h"
#include "comp.h"
#include "status.h"
#include "../utils/coding.h"


namespace kdb {

// reference 引用计数
Memtable::Memtable()
    : isReadonly_(false), compaction_number_(0), refs_(0), arena_(Arena()),
      table_(Comparator(), &arena_)  // 传递 arena_ 的地址
{}

/*
  | key_size      | key_value | sequence_number | value_type | value_size | value_val |
  | varint byte   |  ? byte   |    8 byte       |   1 byte   |   4 byte   | ? byte    |
1 <= varient_size <= 5
*/

void Memtable::Set(const std::shared_ptr<SetContext>& set_context) {
  auto key_size = VarintLength(set_context->key.size());
  auto value_size = VarintLength(set_context->value.size());
  auto sequence_number = VarintLength(set_context->number);

  std::string simple_set_str = fmt::format(
      "{}{}{}{}{}{}", format32_vec[key_size], set_context->key,
      format64_vec[sequence_number], kEmpty1Space,
      format32_vec[value_size], set_context->value);
  //这里是预填充 一个空格就是一个字节 最大存储128

  //
  // 赋值 key_size;
  // formatEncodeFixed32(set_context->key.size(), simple_set_str.data());
  //
  char* start_ptr = simple_set_str.data();
  start_ptr = EncodeVarint32(start_ptr, set_context->key.size());

  //
  // 赋值 sequence_number
  // formatEncodeFixed64(set_context->key.size(),
  //                     simple_set_str.data() + 4 + set_context->key.size());
  //
  start_ptr += set_context->key.size();
  start_ptr = EncodeVarint64(start_ptr, set_context->number);

  // 赋值 value_type, 表示key 还活着,
  // formatEncodeFixed8(ValueType::kTypeValue,
  //                    simple_set_str.data() + 4 + set_context->key.size() +
  //                    8);
  //
  EncodeFixed8(start_ptr,ValueType::kTypeValue);

  // 赋值 value.size(),
  // formatEncodeFixed32(set_context->value.size(),
  //                     simple_set_str.data() + 4 + set_context->key.size() +
  //                     9);
  //
  start_ptr += 1;
  EncodeVarint32(start_ptr, set_context->value.size());
  
  // 如果 key 已经存在, 那么根据自定义比较器应该会覆盖
  auto origin_memMap_size = arena_.MemoryUsage();
  auto origin_str_size = simple_set_str.size();
  table_.Insert(std::move(simple_set_str));
  // set 请求有可能覆盖原有的值
  // 只有当 memMap_ 的值发生改变, 才会去增加 memMap 的size
}

void Memtable::Get(const std::shared_ptr<GetContext>& get_context) {
  SkipList<std::string_view,Comparator>::Iterator iter(&table_);
  iter.SeekToFirst();
  while(iter.Valid()) {
    std::string_view key = iter.key();

    uint32_t mem_key_size = 0;
    auto end_ptr = GetVarint32Ptr(key.data(), key.data() + 5, &mem_key_size);
    assert(end_ptr != nullptr);
   
    std::string user_key(end_ptr,mem_key_size);
    if(user_key == get_context->key) {
      uint64_t iter_number;
      end_ptr = GetVarint64Ptr(end_ptr + mem_key_size, end_ptr + mem_key_size + 9,
                            &iter_number);
      // 获取 key, value 当前状态
      uint8_t key_type = DecodeFixed8(end_ptr);
      if (key_type == ValueType::kTypeDeletion) {
        get_context->code.setCode(StatusCode::kNotFound);
        return ;
      } else {
        end_ptr++;
        uint32_t mem_value_size = 0;
        end_ptr = GetVarint32Ptr(end_ptr, end_ptr + 5, &mem_value_size);
        std::string user_value(end_ptr,mem_value_size);
        get_context->value = user_value;
        get_context->code.setCode(StatusCode::kOk);
        return ;
      }
    }
    else if (user_key < get_context->key) {
      iter.Next();
      }
    else break;
  }
  get_context->code.setCode(StatusCode::kNotFound);
  return ;
}

void Memtable::Delete(const std::shared_ptr<DeleteContext>& del_context) {
  //
  auto key_size = VarintLength(del_context->key.size());
  //
  //
  auto sequence_number = VarintLength(del_context->number);

  std::string simple_del_str = fmt::format(
      "{}{}{}{}", format32_vec[key_size], del_context->key,
      format64_vec[sequence_number], kEmpty1Space);
  char* start_ptr = simple_del_str.data();
  start_ptr = EncodeVarint32(start_ptr, del_context->key.size());
  start_ptr += del_context->key.size();
  start_ptr = EncodeVarint32(start_ptr, del_context->number);
  EncodeFixed8(start_ptr,StatusCode::kDelete);
  // 直接插入
  table_.Insert(simple_del_str);
}

uint32_t Memtable::getMemSize() { return arena_.MemoryUsage(); }

void Memtable::setReadOnly() { isReadonly_ = true; }

void Memtable::setNumber(const uint32_t number) { memtable_number_ = number; }

uint32_t Memtable::getMemNumber() const { return memtable_number_; }

void Memtable::addRefs() { refs_++; }

uint32_t Memtable::getRefs() { return refs_.load(); }

void Memtable::decreaseRefs() { refs_--; }

auto Memtable::getMemTableRef() -> MemSkipTable& { return table_; }

};