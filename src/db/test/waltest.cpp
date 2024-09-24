#include <gtest/gtest.h>
#include "../wal_write.h"
#include "../../utils/file/wal_log.h"

namespace kdb {
TEST(WalWriterTest, InitTest) {
    WalWriter writer;
    Status status;
    
    // 测试初始化WalWriter日志文件
    bool result = writer.Init("/tmp/", "test_wal.log", &status);
    EXPECT_TRUE(result);  // 期望初始化成功
    EXPECT_EQ(status.getCode(), StatusCode::kOk);  // 状态码为kOk
}

TEST(WalWriterTest, AddRecordTest) {
    WalWriter writer;
    Status status;
    
    // 初始化WalWriter
    writer.Init("/tmp/", "test_wal.log", &status);
    
    // 追加日志记录
    std::string record = "This is a test record.";
    bool result = writer.AddRecord(record);
    EXPECT_TRUE(result);  // 期望记录写入成功
}

// 测试当日志文件超过大小阈值时，是否正确回绕
TEST(WalWriterTest, LogOverflowTest) {
    WalWriter writer;
    Status status;

    // 设置较小的日志大小，以便测试环形缓冲区
    writer.Init("/tmp/", "test_wal_overflow.log", &status);
    writer.SetWalLogDefultSize(10);  // 设置日志大小为128字节

    // 写入几条超过128字节的日志
    std::string large_record(11, 'A');  // 生成一条150字节的日志记录
    bool result = writer.AddRecord(large_record);
    EXPECT_TRUE(result);  // 期望记录写入成功

    // 写入后应该回绕文件到开头
    std::string second_record(15, 'B');
    result = writer.AddRecord(second_record);
    EXPECT_TRUE(result);  // 期望第二次记录写入成功
}

TEST(WalLogTest, ReadTest) {
    kdb::WalWriter writer;
    kdb::Status status;

    // 初始化WalWriter
    writer.Init("/tmp/", "test_wal_read.log", &status);

    // 写入测试记录
    std::string record = "Hello, WAL!";
    writer.AddRecord(record);

    // 读取日志内容
    char buffer[1024];
    int bytes_read = writer.ReadLog(buffer, sizeof(buffer));
    EXPECT_GT(bytes_read, 0);  // 期望读取成功

    std::string log_content(buffer, bytes_read);
    EXPECT_EQ(log_content.substr(0, record.size()), record);  // 期望读取的内容与写入内容相同
}
}