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

#include "src/db/comp.hpp"
#include "../include/skiplist.h"


#define SRC_DB_KEYFORMAT_H_
#endif


namespace kdb {
  using MemSkipTable = SkipList<std::string_view,Comparator>; 

    
}