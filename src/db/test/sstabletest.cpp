#include <gtest/gtest.h>
#include "../sstable.h"
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace kdb {

void createTestFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    file << content;
    file.close();
}

// 清理文件
void removeTestFile(const std::string& filename) {
    remove(filename.c_str());
}

class SSTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试文件
        createTestFile(testFilePath, "Hello, SSTable!");
    }

    void TearDown() override {
        // 清理测试文件
        removeTestFile(testFilePath);
    }

    std::string testFilePath = "./test_sstable_file.txt";
};

// 测试 SSTable 初始化和文件打开
TEST_F(SSTableTest, InitSuccess) {
    SSTable sstable;
    bool result = sstable.Init(".", "test_sstable_file.txt", 0, 1);
    ASSERT_TRUE(result) << "Failed to initialize SSTable";

    // 测试文件描述符是否正确
    int fd = sstable.getFd();
    ASSERT_NE(fd, -1) << "File descriptor is invalid";

    // 测试文件大小
    uint32_t fileSize = sstable.getFileSize();
    ASSERT_EQ(fileSize, 15) << "File size should be 15";

    remove("./test_sstable_file.txt");
}

// 测试 mmap 初始化
TEST_F(SSTableTest, MmapSuccess) {
    SSTable sstable;
    bool result = sstable.Init(".", "test_sstable_file.txt", 0, 1);
    ASSERT_TRUE(result) << "Failed to initialize SSTable";

    // 初始化 mmap
    bool mmapInitResult = sstable.InitMmap();
    ASSERT_TRUE(mmapInitResult) << "Failed to initialize mmap";

    // 检查 mmap 指针是否为有效地址
    char* mmapPtr = sstable.getMmapPtr();
    ASSERT_NE(mmapPtr, nullptr) << "mmap pointer should not be null";

    // 验证 mmap 的内容
    std::string fileContent(mmapPtr, sstable.getFileSize());
    ASSERT_EQ(fileContent, "Hello, SSTable!") << "File content mismatch";

    // 关闭 mmap
    sstable.CloseMmap();
    
    remove("./test_sstable_file.txt");
}

// 测试文件关闭操作
TEST_F(SSTableTest, CloseSuccess) {
    SSTable sstable;
    bool result = sstable.Init(".", "test_sstable_file.txt", 0, 1);
    ASSERT_TRUE(result) << "Failed to initialize SSTable";

    // 关闭文件
    sstable.Close();
    ASSERT_EQ(sstable.getFd(), -1) << "File descriptor should be invalid after close";

    remove("./test_sstable_file.txt");
}

}  // namespace kdb
