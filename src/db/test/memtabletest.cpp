#include <gtest/gtest.h>
#include "../memtable.h"
#include <fmt/core.h>
#include "../../utils/coding.h"

namespace kdb {
    // 测试 Memtable::Set 函数
    TEST(MemtableTest, TestSet) {
    // 创建 Memtable 实例
    Memtable memtable;

    // 创建 SetContext，模拟插入操作
    SetContext context1(std::string_view("key1"), std::string_view("value1"));
    
    EXPECT_EQ(memtable.getMemSize(), 4104); //skiplist头节点head_占用4096 + 指针 8 (sizeof(char*)) = 4104

    // 调用 Set 方法
    memtable.Set(std::make_shared<SetContext>(context1));

    // 验证 Memtable 的大小是否增加
    EXPECT_GT(memtable.getMemSize(), 4104); //memtable.getMemSize() > 4104
}
}