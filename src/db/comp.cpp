#include "comp.h"
#include "../utils/coding.h"

#include <assert.h>

namespace kdb {
int Comparator::operator()(std::string_view const& left, 
                            std::string_view const& right) const {
  uint32_t left_key_len = 0;
  auto left_key_len_ptr = GetVarint32Ptr(left.data(), left.data() + 5, &left_key_len);
  assert(left_key_len_ptr != nullptr);
  
  uint32_t right_key_len = 0;
  auto right_key_len_ptr = GetVarint32Ptr(right.data(), right.data() + 5, &right_key_len);
  assert(right_key_len_ptr != nullptr);
  
  std::string_view left_key_value(left_key_len_ptr,left_key_len);
  std::string_view right_key_value(right_key_len_ptr,right_key_len);
  // 首先比较键值
  if (left_key_value < right_key_value) {
    return -1;
  }
  else if (left_key_value > right_key_value) {
    return 1;
  }

  // 如果键值相等，解析并比较 Varint 编码的序列号
  // 使用 GetVarint64Ptr 来解析序列号
  const char* left_seq_ptr = left_key_len_ptr + left_key_len;
  const char* right_seq_ptr = right_key_len_ptr + right_key_len;
  
  uint64_t left_seq = 0;
  uint64_t right_seq = 0;
  
  // 获取左序列号的 Varint 编码
  auto left_seq_end_ptr = GetVarint64Ptr(left_seq_ptr, left.data() + 9, &left_seq);
  assert(left_seq_end_ptr != nullptr);

  // 获取右序列号的 Varint 编码
  auto right_seq_end_ptr = GetVarint64Ptr(right_seq_ptr, right.data() + 9, &right_seq);
  assert(right_seq_end_ptr != nullptr);
  
  if(left_seq > right_seq) {
    return 1;
  } else if(left_seq < right_seq){
    return -1;
  }
  else return 0;
}
    

}