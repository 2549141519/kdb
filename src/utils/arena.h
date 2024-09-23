#pragma once
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
//可以理解成自己的内存池 但是他没有销毁 只能不停得分配和创造


namespace kdb
{
class Arena
{
public:
    Arena();
    //删除拷贝构造和复制构造
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    //两个留给用户的接口 分别是申请内存对齐和默认的起始地址
    char *Allocate(std::size_t bytes);
    //char* AllocateAligned(std::size_t bytes);

    char* AllocateAligned(std::size_t bytes);
    
    //arena这个内存管理器已经开辟了多少内存了
    std::size_t MemoryUsage() const {
    return memory_usage_.load(std::memory_order_relaxed);
  }
    ~Arena();
private:
    char* AllocateFallback(std::size_t bytes);
    char* AllocateNewBlock(std::size_t block_bytes);
private:
    //内存块集合 这里也是分成大块和小块
    std::vector<char *> blocks_;

    //起始指针 以及剩余的空间大小
    char *alloc_ptr_;
    std::size_t alloc_bytes_remaining_;

    //总共的内存开销 
    //这里还没看 但我怀疑是 这个变量是多线程访问的 其他变量都是单线程访问的

    std::atomic<std::size_t> memory_usage_;
    
};

inline char* Arena::Allocate(std::size_t bytes) {
  //这里禁止了0字节分配
  //保持代码逻辑清晰和简单
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) 
  {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return AllocateFallback(bytes);
}
};



