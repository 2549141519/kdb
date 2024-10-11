#include <gtest/gtest.h>
#include "../../include/bloomfilter.h"

namespace kdb {
    TEST(BloomFilterTest, InsertAndMatchTest) {
    BloomFilter<> filter;  // 使用默认大小
    std::string key1 = "test_key_1";
    std::string key2 = "test_key_2";
    std::string key3 = "test_key_3";

    // 插入 key1 和 key2
    filter.Insert(key1);
    filter.Insert(key2);

    // key1 和 key2 应该匹配
    EXPECT_TRUE(filter.IsMatch(key1));
    EXPECT_TRUE(filter.IsMatch(key2));

    // key3 未插入，不应该匹配
    EXPECT_FALSE(filter.IsMatch(key3));
    }

    TEST(BloomFilterTest, BoundaryTest) {
    BloomFilter<> filter;  // 使用默认大小
    std::string empty_key = "";  // 空 key

    // 插入空 key
    filter.Insert(empty_key);

    // 空 key 应该匹配
    EXPECT_TRUE(filter.IsMatch(empty_key));

    // 一个很长的 key 测试
    std::string long_key(1000, 'x');  // 1000 个 'x' 组成的字符串
    filter.Insert(long_key);
    EXPECT_TRUE(filter.IsMatch(long_key));
    }

    TEST(BloomFilterTest, ConstructorWithBitsetDataTest) {
    char data[kBloomFilterDefaultSize / 8] = {};  // 模拟从磁盘或文件加载的位集合数据
    BloomFilter<> filter(data, sizeof(data), 12);  // 使用已有数据创建 BloomFilter

    // 测试在这种情况下是否能正常插入和匹配
    std::string key = "test_data_load";
    filter.Insert(key);
    EXPECT_TRUE(filter.IsMatch(key));
    }
}