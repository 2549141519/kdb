#ifndef SRC_DB_KEYFORMAT_H_

// key_design
//
//
//
//

#include <memory>
#include <set>
#include <string>
#include <string_view>

//#include "src/db/comp.h"
#include "comp.h"
#include "../include/skiplist.h"


#define SRC_DB_KEYFORMAT_H_
#endif


namespace kdb {
using MemSkipTable = SkipList<std::string_view,Comparator>; 

// 为了格式化, 1 个空格
const std::string kEmpty1Space = " ";

// for format_style
const std::string kEmpty2Space = "  ";
// for format_style
const std::string kEmpty3Space = "   ";
const std::string kEmpty4Space = "    ";
const std::string kEmpty5Space = "     ";
const std::string kEmpty6Space = "      ";
const std::string kEmpty7Space = "       ";
const std::string kEmpty8Space = "        ";
const std::string kEmpty9Space = "         ";

// 为了格式化, 8 个空格

const std::string format32_vec[] = {
    kEmpty1Space,  // 0 => 1 个空格
    kEmpty1Space,  // 1 => 1 个空格
    kEmpty2Space,  // 2 => 2 个空格
    kEmpty3Space,  // 3 => 3 个空格
    kEmpty4Space,  // 4 => 4 个空格
    kEmpty5Space,  // 5 => 5 个空格
};

const std::string format64_vec[] = {
    kEmpty1Space,  // 0 => 1 个空格
    kEmpty1Space,  // 1 => 1 个空格
    kEmpty2Space,  // 2 => 2 个空格
    kEmpty3Space,  // 3 => 3 个空格
    kEmpty4Space,  // 4 => 4 个空格
    kEmpty5Space,  // 5 => 5 个空格
    kEmpty6Space,  // 6 => 6 个空格
    kEmpty7Space,  // 7 => 7 个空格
    kEmpty8Space,  // 8 => 8 个空格
    kEmpty9Space,  // 9 => 9 个空格
};

// 为了格式化, 32 个空格
const std::string kEmpty32Space = "                                ";

/*
leveldb 使用 skiplist 来实现位于内存中的 Memtable
leveldb 将 user_key 和 user_value 打包成一个更大的 key

memtable_entry:
          | key_size | user_key | sequence_number | key_type | value_size |
value  | |  4 byte  |   ? byte |       8 byte    |   1 byte |   4 byte   | ?
byte |

*/
    
enum ValueType {
  // 删除的标志
  kTypeDeletion = 0,
  kTypeValue = 1,
};
}