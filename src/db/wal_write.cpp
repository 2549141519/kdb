#include "wal_write.h"

#include <cstdint>

extern "C" {
#include <assert.h>
}

#include "request.h"
#include "status.h"
#include "../utils/crc32.h"

namespace kdb
{

WalWriter::WalWriter()
    :current_log_size_(0), wal_log_size_(kDefaultWalLogSize) {}   

bool WalWriter::Init(const std::string& path, const std::string& filename,
                     Status* status) { 
  assert(nullptr != status);

  auto ue = wal_file_.Init(path, filename);
  if (true == ue) {  
    status->setCode(StatusCode::kOk);
    current_log_size_ += wal_file_.getFileSize();
    // 添加返回true;
    return true;
    } else {
        status->setCode(StatusCode::kIoError);
    }
  return false;
}

void WalWriter::SetWalLogDefultSize(const uint32_t log_size) {
  wal_log_size_ = log_size;
}

int WalWriter::ReadLog(char* data, const size_t size) {
  return wal_file_.Read(data, size);
}

WalWriter::~WalWriter() { wal_file_.Close(); }

bool WalWriter::AddRecord(const std::string_view& record) {
  current_log_size_ += record.size();
  // 如果该文件超过阈值, 那么就 seek 到文件最开始的地方, 环形缓冲区
  if (current_log_size_ >= wal_log_size_) {
    wal_file_.SeekBegin();
    current_log_size_ = 0;
  }
  wal_file_.AddRecord(record.data(), record.size());
  return true;
}
}