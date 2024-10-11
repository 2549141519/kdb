#include "arena.h"

namespace kdb
{
static const int kBlockSize = 4096;//每一个块默认的大小
Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0) {}

Arena::~Arena() 
    {
        for(auto c_ptr:blocks_)
        {
            delete[] c_ptr; //释放和析构指针数组
        }
    }

char *Arena::AllocateFallback(std::size_t bytes)
    {
        //进入这个函数 说明已有的内存已经不够了
        //这里因为她所占用的内存量较大 我们要申请一个新块给他 同时不改变我们本身的指针指向
        if (bytes > kBlockSize / 4) 
        {
            char* result = AllocateNewBlock(bytes);
            return result;
        }
        alloc_ptr_ = AllocateNewBlock(kBlockSize);
        alloc_bytes_remaining_ = kBlockSize;

        char* result = alloc_ptr_;
        alloc_ptr_ += bytes;
        alloc_bytes_remaining_ -= bytes;
        return result;

    }

char* Arena::AllocateNewBlock(std::size_t block_bytes) 
    {
        char* result = new char[block_bytes];
        blocks_.push_back(result);
        memory_usage_.fetch_add(block_bytes + sizeof(char*),
                                std::memory_order_relaxed);
        //这里可以看到 memory usage是由指针加上blocksize大小决定的
        return result;
    }

char* Arena::AllocateAligned(std::size_t bytes) {
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  static_assert((align & (align - 1)) == 0,
                "Pointer size should be a power of 2");
  std::size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1);
  std::size_t slop = (current_mod == 0 ? 0 : align - current_mod);
  std::size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    // AllocateFallback always returned aligned memory
    result = AllocateFallback(bytes);
  }
  assert((reinterpret_cast<uintptr_t>(result) & (align - 1)) == 0);
  return result;
}


};