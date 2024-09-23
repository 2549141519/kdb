
#include "sstable.h"

#include <cstring>
#include <fmt/core.h> //sudo apt install libfmt-dev
#include <sys/mman.h>
#include <assert.h>

extern "C" {
#include <error.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
}

namespace kdb 
{
SSTable::SSTable() : isMmap_(false), mmapBasePtr_(nullptr), isOpen_(false) {}

SSTable::~SSTable() {
  if (isOpen_ == true) {
    Close();
  }
  if (true == isMmap_) {
    CloseMmap();
  }
}

void SSTable::Close() {
  if (true == isOpen_) {
    ::close(fd_);
    isOpen_ = false;
   }
}
void SSTable::Close() {
  if (true == isOpen_) {
    ::close(fd_);
    isOpen_ = false;
   }
}
void SSTable::CloseMmap() {
  ::munmap(static_cast<void*>(mmapBasePtr_), fileLength_);
}

// disk 虚拟内存 buffer pool 模仿page cache
// parser 词法分析语法分析
// 
int SSTable::getFd() { return fd_; }

uint32_t SSTable::getFileSize() {
  struct ::stat file_stat;
  if (-1 == ::fstat(fd_, &file_stat)) {
    assert(false);
  }
  return file_stat.st_size;
}

void SSTable::setLevel(const uint32_t n) { sstable_level_ = n; }

auto SSTable::getLevel() { return sstable_level_; }

void SSTable::setNumber(const uint32_t n) { sstable_number_ = n; }

uint32_t SSTable::getNumber() { return sstable_number_; }

char* SSTable::getMmapPtr() { return mmapBasePtr_; }

bool SSTable::Init(const std::string& path, const std::string& filename,
                   const uint32_t level, const uint32_t number) {
  fullfilename_ = fmt::format("{}/{}", path, filename);
  fd_ = ::open(fullfilename_.c_str(), O_RDWR | O_CREAT, 0655);
  if (fd_ < 0) {
    return false;
  }
  fileName_ = filename;
  filePath_ = path;
  sstable_number_ = number;
  sstable_level_ = level;
  isOpen_ = true;
  return true;
}

bool SSTable::InitMmap() {
  struct ::stat stat_buf;
  if (::stat(fullfilename_.c_str(), &stat_buf) != 0) [[unlikely]] {
    return false;
  }

  fileLength_ = stat_buf.st_size;
  mmapBasePtr_ = static_cast<char*>(
      ::mmap(nullptr, fileLength_, PROT_READ, MAP_SHARED, fd_, 0));
  if (mmapBasePtr_ == MAP_FAILED) {
    return false;
  }
  isMmap_ = true;
  return true;
}



    
}