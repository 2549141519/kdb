#ifndef SRC_DB_SSTABLE_H_
#define SRC_DB_SSTABLE_H_
#include <string>

namespace kdb
{

class SSTable
{
public:
  SSTable();
  ~SSTable();

  /*
    @brief : null
    @return : return sstable => fd
  */
  auto getFd() -> int;

  /*
    close the sstable.
  */
  void Close();

  // 获取当前的 Level
  auto getLevel();

  // 获取当前 SSTable Level
  void setLevel(const uint32_t n);

  // 获取当前的 sstable number
  uint32_t getNumber();
  
  // 获取当前的 number
  void setNumber(const uint32_t n);

  // 启动当前文件 mmap 映射
  bool InitMmap();

  void CloseMmap();

  char* getMmapPtr();

  // 获取文件大小
  uint32_t getFileSize();

  const std::string& getfileName() { return fullfilename_; }

  const std::string& getPath() { return filePath_; }

  

private:
  int fd_;
  
  // 当前文件的长度.
  size_t fileLength_;  

  // 是否打开
  bool isOpen_;

  // mmap 所使用的指针
  char* mmapBasePtr_;
    
  // 是否初始化 mmap
  bool isMmap_;

  uint32_t sstable_number_;
  uint32_t sstable_level_;
  std::string fullfilename_;
  // TODO, use std::string_view instead of std::string in Init.
  std::string fileName_;
  std::string filePath_;
  
};    




}

#endif