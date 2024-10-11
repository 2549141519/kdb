#include "key_format.h"

namespace kdb {
absl::weak_ordering BComparator::operator()(std::string_view const& left, 
                            std::string_view const& right) const {
  uint32_t left_key_len = 0;
  auto left_key_len_ptr = GetVarint32Ptr(left.data(), left.data() + 5, &left_key_len);
  assert(left_key_len_ptr != nullptr);
  
  uint32_t right_key_len = 0;
  auto right_key_len_ptr = GetVarint32Ptr(right.data(), right.data() + 5, &right_key_len);
  assert(right_key_len_ptr != nullptr);
  
  std::string_view left_key_value(left_key_len_ptr,left_key_len);
  std::string_view right_key_value(right_key_len_ptr,right_key_len);
  // 比较键值
  if (left_key_value < right_key_value) {
    return absl::weak_ordering::less;
  }
  else if (left_key_value > right_key_value) {
    return absl::weak_ordering::greater;
  }
  return absl::weak_ordering::equivalent;
}
}