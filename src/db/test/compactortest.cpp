#include <gtest/gtest.h>
#include <memory>
#include <fstream>
#include <cstdio>
#include "../table_view_manager.h"
#include "../compactor.h"
#include "../sstable.h"
#include "../../include/bloomfilter.h"

namespace kdb {

void GenerateSSTableFile(SSTable* sstable, 
                         uint32_t sstable_number, 
                         uint32_t sstable_level, 
                         const std::vector<std::pair<std::string, std::string>>& key_value_pairs) {
    // 使用 SSTable 的 mmapBasePtr_ 进行写入
    char* mmap_ptr = sstable->getMmapPtr();
    if (mmap_ptr == nullptr) {
        std::cerr << "Failed to get mmap pointer for SSTable" << std::endl;
        return;
    }
    char* end_ptr = mmap_ptr;

    // 创建 BloomFilter 实例
    BloomFilter<> bloom_filter;

    // 插入每个键到 BloomFilter 中
    for (const auto& [key, value] : key_value_pairs) {
        bloom_filter.Insert(key);
    }

    // 写入版本号
    uint32_t version = 1;
    end_ptr = EncodeVarint32(end_ptr, version);

    // 写入文件创建时间
    auto current_time = std::chrono::system_clock::now();
    uint64_t time_as_uint64 = std::chrono::system_clock::to_time_t(current_time);
    end_ptr = EncodeVarint64(end_ptr, time_as_uint64);

    // 写入层级
    end_ptr = EncodeVarint32(end_ptr, sstable_level);

    // 写入文件编号
    end_ptr = EncodeVarint32(end_ptr, sstable_number);

    // 写入键值对数量
    uint32_t key_count = key_value_pairs.size();
    end_ptr = EncodeVarint32(end_ptr, key_count);

    // 写入 BloomFilter 种子和大小
    uint64_t bloom_filter_seed = bloom_filter.getFilterSeed();
    end_ptr = EncodeVarint64(end_ptr, bloom_filter_seed);

    uint32_t bloom_filter_size = bloom_filter.getBitSetSize();  // 获取 BloomFilter 大小
    end_ptr = EncodeVarint32(end_ptr, bloom_filter_size);

    // 写入 BloomFilter 数据
    auto* bloom_filter_data = bloom_filter.getFilterData();
    std::memcpy(end_ptr, bloom_filter_data->getData(), bloom_filter_size);
    end_ptr += bloom_filter_size;

    // 写入最小键和最大键
    if (!key_value_pairs.empty()) {
        std::string min_key = key_value_pairs.front().first;
        std::string max_key = key_value_pairs.back().first;

        uint32_t min_key_length = min_key.size();
        end_ptr = EncodeVarint32(end_ptr, min_key_length);
        std::memcpy(end_ptr, min_key.data(), min_key_length);
        end_ptr += min_key_length;

        uint32_t max_key_length = max_key.size();
        end_ptr = EncodeVarint32(end_ptr, max_key_length);
        std::memcpy(end_ptr, max_key.data(), max_key_length);
        end_ptr += max_key_length;
    }

    // 写入键值对数据
    for (const auto& [key, value] : key_value_pairs) {
        uint32_t key_size = key.size();
        uint32_t value_size = value.size();
        uint64_t sequence_number = 1;  // 示例序列号
        uint8_t value_type = static_cast<uint8_t>(ValueType::kTypeValue);  // 示例值类型

        // 写入键的大小
        end_ptr = EncodeVarint32(end_ptr, key_size);

        // 写入键内容
        std::memcpy(end_ptr, key.data(), key_size);
        end_ptr += key_size;

        // 写入序列号
        end_ptr = EncodeVarint64(end_ptr, sequence_number);

        // 写入值类型
        EncodeFixed8(end_ptr, value_type);
        end_ptr += 1;

        // 写入值的大小
        end_ptr = EncodeVarint32(end_ptr, value_size);

        // 写入值内容
        std::memcpy(end_ptr, value.data(), value_size);
        end_ptr += value_size;
    }
}

class CompactorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化 TableViewManager，并模拟插入 L0 层的 SSTable
        table_view_manager_.Init(3);  // 初始化 3 层
        compactor_ = std::make_unique<Compactor>(table_view_manager_);

        // 创建 L0 层文件夹路径
        l0_path_ = "/home/ubuntu/kdb/file/level0";
        system(("mkdir -p " + l0_path_).c_str());

        // 模拟在 L0 层插入 5 个 SSTable，创建文件并将其插入 TableViewManager
        for (int i = 0; i < 5; ++i) {
            std::string min_key = "key" + std::to_string(i);
            std::string min_key_copy = "key" + std::to_string(i);
            std::string max_key = "key" + std::to_string(i + 10);

            // 创建 SSTable 文件路径
            std::string filename = l0_path_ + "sstable_" + std::to_string(i + 1) + ".sst";

            // 创建 SSTable 并插入 TableViewManager
            std::shared_ptr<SSTable> sstable = std::make_shared<SSTable>();
            uint32_t sstable_number = i + 1;  // 编号从1开始
            uint32_t sstable_level = 0;       // L0层

            // 初始化 SSTable
            sstable->Init(l0_path_, "sstable_" + std::to_string(sstable_number) + ".sst", sstable_level, sstable_number);
            sstable->InitMmap();
            sstable->setMinKey(min_key);
            sstable->setMaxKey(max_key);

            // 使用 GenerateSSTableFile 创建 SSTable 并写入 mmap 区域
            std::vector<std::pair<std::string, std::string>> key_value_pairs = {
                {min_key, "value" + std::to_string(i)},
                {min_key_copy, "value" + std::to_string(i + 5)},
                {max_key, "value" + std::to_string(i + 10)}
            };
            GenerateSSTableFile(sstable.get(), i + 1, 0, key_value_pairs);

            // 将文件插入到 TableViewManager 的 L0 层
            table_view_manager_.PushTableView(sstable_level, sstable->getMmapPtr(), sstable->getFileSize(), sstable, min_key, max_key);
        }


        // 验证 L0 层中有 5 个 SSTable
        ASSERT_EQ(table_view_manager_.getViewVecSize(0), 5);
    }

    void TearDown() override {
        // 清理压缩任务
        compactor_->StopCompaction();
    }

    TableViewManager table_view_manager_;
    std::unique_ptr<Compactor> compactor_;
    std::string l0_path_;  // L0 层文件路径
};

// 测试 minorcompaction_task_L0_to_L1 方法（L1 层为空时合并）
TEST_F(CompactorTest, TestMinorCompactionL0ToL1_L1Empty) {
    // 在测试中，确保 L1 层为空
    ASSERT_EQ(table_view_manager_.getViewVecSize(1), 0);

    auto len = table_view_manager_.getViewVecSize(0);

    // 调用 minorcompaction_task_L0_to_L1 方法进行压缩
    compactor_->minorcompaction_task_L0_to_L1();

    // 验证 L0 层中 SSTable 数量是否减少到 4 个
    EXPECT_EQ(table_view_manager_.getViewVecSize(0), 4);

    // 验证 L1 层是否有 1 个 SSTable
    EXPECT_EQ(table_view_manager_.getViewVecSize(1), 1);

    // 获取 L1 层 SSTable 并检查其元信息
    auto l1_sstables = table_view_manager_.GetL1SSTables();
    ASSERT_FALSE(l1_sstables.empty());

    // 检查 SSTable 的层级是否为 1
    int level = l1_sstables.front()->getLevel();
    EXPECT_EQ(level, 1);  // 确保 SSTable 在 L1
}

/*TEST_F(CompactorTest, TestMinorCompactionL0ToL1_L1NOTEMPTY) {
    //
}*/

}  // namespace kdb

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}