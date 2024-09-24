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
    auto context1 = std::make_shared<SetContext>(std::string_view("key1"), std::string_view("value1"));
    
    EXPECT_EQ(memtable.getMemSize(), 4104); //skiplist头节点head_占用4096 + 指针 8 (sizeof(char*)) = 4104
    // 调用 Set 方法
    memtable.Set(context1);

    // 验证 Memtable 的大小是否增加
    EXPECT_GT(memtable.getMemSize(), 4104); //memtable.getMemSize() > 4104
}

    // 测试 Memtable::Get 函数
    TEST(MemtableTest, TestGet) {
    
    Memtable memtable;
    //测试返回值是否正确
    std::shared_ptr<SetContext> context1 = std::make_shared<SetContext>(std::string_view("key1"), std::string_view("value1"));
    context1->number = 1;
    memtable.Set(context1);
    std::shared_ptr<SetContext> context4 = std::make_shared<SetContext>(std::string_view("key1"), std::string_view("value1"));
    context4->number = 2;
    memtable.Set(context4);
    std::shared_ptr<GetContext> context2 = std::make_shared<GetContext>(std::string_view("key1"));
    memtable.Get(context2);
    EXPECT_EQ(context2->value, "value1");
    EXPECT_EQ(context2->code.getCode(), StatusCode::kOk);
    
    //测试获取不存在的键
    std::shared_ptr<GetContext> context3 = std::make_shared<GetContext>(std::string_view("key2"));
    memtable.Get(context3);
    EXPECT_EQ(context3->code.getCode(), StatusCode::kNotFound);
}
/*
    TEST(MemtableTest, TestDelete) {
    Memtable memtable;

    SetContext context1(std::string_view("key1"), std::string_view("value1"));
    memtable.Set(std::make_shared<SetContext>(context1));

    DeleteContext deleteContext1(std::string_view("key1"));
    memtable.Delete(std::make_shared<DeleteContext>(deleteContext1));

    GetContext getContext1(std::string_view("key1"));
    memtable.Get(std::make_shared<GetContext>(getContext1));
    
    EXPECT_EQ(getContext1.code.getCode(), StatusCode::kDelete);
    }
*/
}