#ifndef SRC_UTIL_FILE_WAL_LOG_H_
#define SRC_UTIL_FILE_WAL_LOG_H_

#include <cstdio>


#include <string>
#include <string_view>

extern "C" {
#include <fcntl.h>
}

namespace kdb 
{



class WalLog {
public:
  WalLog() = default;
  
  ~WalLog();
  
  bool Init(const std::string& path, const std::string& filename);

  void AddRecord(const char* data, const size_t data_size);

  void Close();

  uint32_t getFileSize();
  // 回溯到文件开头
  void SeekBegin();

  int Read(char* buf, const size_t size);
private:
  std::string fullFilename_;
  std::string_view fileName_;
  std::string_view path_;

  int fd_;
};

}  //namespace kdb


#endif