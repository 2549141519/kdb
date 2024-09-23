
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





    
}