#ifndef SRC_DB_COMPARATOR_H_
#define SRC_DB_COMPARATOR_H_
#include <string>

namespace kdb{

// for memtable key comparate
// 对于单个 Memtable 来说,
// 一个 key 的格式
// clang-format off
/*
  | key_size | key_value | sequence_number | value_type | value_size | value_val |
  | 4 byte   |  ? byte   |    8 byte       |   1 byte   |   4 byte   | ? byte    |
*/

struct Comparator {
    bool operator()(std::string_view const& left,
                    std::string_view const& right) const;
};

};




#endif