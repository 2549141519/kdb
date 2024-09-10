#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include"../include/cache.h"
#include "hash.h"


namespace kdb
{
Cache::~Cache(){} //先重写虚构

// LRU 缓存实现
//
// 缓存条目具有一个表示是否在缓存中的布尔值 "in_cache"。如果这个值变为 false，而条目没有被传递给它的 "deleter"，
// 只有几种可能的方式：通过 Erase()，通过 Insert() 当插入一个具有重复键的元素时，或者在缓存销毁时。
//
// 缓存维护了两个链接列表来存储缓存中的条目。所有缓存中的条目都在其中一个列表中，且永远不会同时出现在两个列表中。
// 被客户端引用但从缓存中删除的条目不在这两个列表中。这两个列表是：
// - in-use：包含当前被客户端引用的条目，顺序不限。（此列表用于不变量检查。如果我们移除检查，本来应该在这个列表中的元素可以被保留为断开的单节点列表。）
// - LRU：包含当前未被客户端引用的条目，按 LRU 顺序排列。
// 元素在这些列表之间的移动由 Ref() 和 Unref() 方法完成，当它们检测到缓存中的元素获取或失去其唯一的外部引用时进行操作。
//
// 一个条目是一个可变长度的堆分配结构。条目保存在一个按访问时间排序的循环双向链表中。

namespace
{

//先做了一个双向链表 表示一个lruhandler
struct LRUHandle
{
    void *value;
    void (*deleter)(const Slice&,void *value);
    LRUHandle *next_hash;
    LRUHandle *next;    
    LRUHandle *prev;
    std::size_t charage;
    std::size_t key_length;
    bool in_cache;
    uint32_t refs;
    uint32_t hash;    //key对应的hash
    char keydata[1]; //key
    Slice key() const {
    //当一个lruhandle为空的时候 即他不在链表中 会出现next == this的情况
    assert(next != this);

    return Slice(keydata, key_length);
  }
};

//又做了一个handletable
class HandleTable
{
public:
    HandleTable() : length_(0), elems_(0), list_(nullptr) { Resize(); }
    ~HandleTable() { delete[] list_; }

    LRUHandle* Lookup(const Slice& key, uint32_t hash) {
        return *FindPointer(key, hash);
    }

    LRUHandle* Insert(LRUHandle* h) {
        LRUHandle** ptr = FindPointer(h->key(), h->hash);
        LRUHandle* old = *ptr;
        h->next_hash = (old == nullptr ? nullptr : old->next_hash);
        *ptr = h;
        if (old == nullptr) {
        ++elems_;
        if (elems_ > length_) {
            // Since each cache entry is fairly large, we aim for a small
            // average linked list length (<= 1).
            Resize();
        }
        }
        return old;
    }

    LRUHandle* Remove(const Slice& key, uint32_t hash) {
        LRUHandle** ptr = FindPointer(key, hash);
        LRUHandle* result = *ptr;
        if (result != nullptr) {
        *ptr = result->next_hash;
        --elems_;
        }
        return result;
    }
private:
  // The table consists of an array of buckets where each bucket is
  // a linked list of cache entries that hash into the bucket.
    uint32_t length_; //哈希表的长度 这个长度是某个key的数量
    uint32_t elems_;  //哈希表中bucket不为空的bucket个数
    LRUHandle** list_;  //bucket

    LRUHandle** FindPointer(const Slice& key, uint32_t hash) {
        LRUHandle** ptr = &list_[hash & (length_ - 1)];
        while (*ptr != nullptr && ((*ptr)->hash != hash || key != (*ptr)->key())) {
        ptr = &(*ptr)->next_hash;
        }
        return ptr;
    }

    void Resize() {
        uint32_t new_length = 4;
        while (new_length < elems_) {
        new_length *= 2;
        }
        LRUHandle** new_list = new LRUHandle*[new_length];
        memset(new_list, 0, sizeof(new_list[0]) * new_length);
        uint32_t count = 0;
        for (uint32_t i = 0; i < length_; i++) {
        LRUHandle* h = list_[i];
        while (h != nullptr) {
            LRUHandle* next = h->next_hash;
            uint32_t hash = h->hash;
            LRUHandle** ptr = &new_list[hash & (new_length - 1)];
            h->next_hash = *ptr;
            *ptr = h;
            h = next;
            count++;
        }
        }
        assert(elems_ == count);
        delete[] list_;
        list_ = new_list;
        length_ = new_length;
    }
};

class LRUCache
{
public:

    LRUCache();
    ~LRUCache();
      // Separate from constructor so caller can easily make an array of LRUCache
    void SetCapacity(std::size_t capacity) { capacity_ = capacity; }

    // Like Cache methods, but with an extra "hash" parameter.
    Cache::Handle* Insert(const Slice& key, uint32_t hash, void* value,
                            std::size_t charge,
                            void (*deleter)(const Slice& key, void* value));
    Cache::Handle* Lookup(const Slice& key, uint32_t hash);
    void Release(Cache::Handle* handle);
    void Erase(const Slice& key, uint32_t hash);
    void Prune();
    std::size_t TotalCharge() const {
        mutex_.lock();
        return usage_;
    }

private:
 
    void LRU_Remove(LRUHandle* e);
    void LRU_Append(LRUHandle* list, LRUHandle* e);
    void Ref(LRUHandle* e);
    void Unref(LRUHandle* e);
    bool FinishErase(LRUHandle* e);
private:
    mutable std::mutex mutex_;
    //guarder_by 表明编译期间会检查是否获得了特定的锁
    //这个属于线程注解
    //handletable是一个hash表 里面存这历史访问过的lruhandle
    //lru是一条链表 prev表示最近被访问的 next表示很久之前被访问的
    //lru_中的元素ref = 1并且incahce = true 
    //inuse的ref >= 2 并且incache = true
    LRUHandle lru_;
    LRUHandle in_use_ ;
    HandleTable table_ ;

    std::size_t capacity_;
    std::size_t usage_ ;
};

void LRUCache::Ref(LRUHandle* e) 
{
    if (e->refs == 1 && e->in_cache) {  
        LRU_Remove(e);
        LRU_Append(&in_use_, e);
    }
    e->refs++;
}

void LRUCache::Unref(LRUHandle* e) {
    assert(e->refs > 0);
    e->refs--;
    if (e->refs == 0) {  // Deallocate.
        assert(!e->in_cache);
        (*e->deleter)(e->key(), e->value); //这里才是对cache中key value的删除
        //这回调函数写的真的好 这么看的话 lrutable就能把lru和cache联系起来。
        //table中的lruhandle和这里的lruhandle是一个节点（因为存储的是指针形式）
        free(e);
    } else if (e->in_cache && e->refs == 1) {
        
        LRU_Remove(e);
        LRU_Append(&lru_, e);
    }
}

LRUCache::LRUCache() : capacity_(0), usage_(0) {
  //初始化会让整个链表为空
    lru_.next = &lru_;
    lru_.prev = &lru_;
    in_use_.next = &in_use_;
    in_use_.prev = &in_use_;
}

//一下接口为内部接口 只涉及到插入删除替换（头插法 同时表达了最近访问次数）
void LRUCache::LRU_Remove(LRUHandle* e) 
{
    e->next->prev = e->prev;
    e->prev->next = e->next;
}

void LRUCache::LRU_Append(LRUHandle* list, LRUHandle* e) {
    
    e->next = list;
    e->prev = list->prev;
    e->prev->next = e;
    e->next->prev = e;
}

bool LRUCache::FinishErase(LRUHandle* e) {
  if (e != nullptr) {
    assert(e->in_cache);
    LRU_Remove(e);
    e->in_cache = false;
    usage_ -= e->charage;
    Unref(e);
  }
  return e != nullptr;
}

Cache::Handle* LRUCache::Insert(const Slice& key, uint32_t hash, void* value,
                                std::size_t charge,
                                void (*deleter)(const Slice& key,
                                                void* value)) {
  std::unique_lock<std::mutex> lock_(mutex_);

  LRUHandle* e =
      reinterpret_cast<LRUHandle*>(malloc(sizeof(LRUHandle) - 1 + key.size()));
  e->value = value;
  e->deleter = deleter;
  e->charage = charge;
  e->key_length = key.size();
  e->hash = hash;
  e->in_cache = false;
  e->refs = 1;  // for the returned handle.
  std::memcpy(e->keydata, key.data(), key.size());

  if (capacity_ > 0) {
    e->refs++;  // for the cache's reference.
    e->in_cache = true;
    LRU_Append(&in_use_, e);
    usage_ += charge;
    FinishErase(table_.Insert(e));
  } else {  //cap = 0 说明要清空整个lru了
    e->next = nullptr;
  }
  while (usage_ > capacity_ && lru_.next != &lru_) {
    LRUHandle* old = lru_.next;
    assert(old->refs == 1);
    bool erased = FinishErase(table_.Remove(old->key(), old->hash));
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }

  return reinterpret_cast<Cache::Handle*>(e);
}

Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash) {
  std::unique_lock<std::mutex> lock_(mutex_);
  LRUHandle* e = table_.Lookup(key, hash);
  if (e != nullptr) {
    Ref(e);
  }
  return reinterpret_cast<Cache::Handle*>(e);
}

void LRUCache::Release(Cache::Handle* handle) {
  std::unique_lock<std::mutex> lock_(mutex_);
  Unref(reinterpret_cast<LRUHandle*>(handle));
}

void LRUCache::Erase(const Slice& key, uint32_t hash) {
  std::unique_lock<std::mutex> lock_(mutex_);
  FinishErase(table_.Remove(key, hash));
  //erase会直接从表中清空 然后去链表中清空 同时调用unref
}

void LRUCache::Prune() {
  std::unique_lock<std::mutex> lock_(mutex_);
  while (lru_.next != &lru_) {
    LRUHandle* e = lru_.next;
    assert(e->refs == 1);
    bool erased = FinishErase(table_.Remove(e->key(), e->hash));
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }
}

LRUCache::~LRUCache() {
  assert(in_use_.next == &in_use_);  // Error if caller has an unreleased handle
  for (LRUHandle* e = lru_.next; e != &lru_;) {
    LRUHandle* next = e->next;
    assert(e->in_cache);
    e->in_cache = false;
    assert(e->refs == 1);  // Invariant of lru_ list.
    Unref(e);
    e = next;
  }
}

static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;


class ShardedLRUCache : public Cache 
{
private:
    LRUCache shard_[kNumShards];
    std::mutex id_mutex_;
    uint64_t last_id_;

    static inline uint32_t HashSlice(const Slice& s) {
    return Hash(s.data(), s.size(), 0);
  }

  static uint32_t Shard(uint32_t hash) { return hash >> (32 - kNumShardBits); }


public:
    Cache* NewLRUCache(std::size_t capacity) { return new ShardedLRUCache(capacity); }
    
    explicit ShardedLRUCache(std::size_t capacity) : last_id_(0) {
    const std::size_t per_shard = (capacity + (kNumShards - 1)) / kNumShards;
    for (int s = 0; s < kNumShards; s++) {
      shard_[s].SetCapacity(per_shard);
    }
  }

  ~ShardedLRUCache() override {}

  Handle* Insert(const Slice& key, void* value, std::size_t charge,
                 void (*deleter)(const Slice& key, void* value)) override {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
  }

  Handle* Lookup(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Lookup(key, hash);
  }

  void Release(Handle* handle) override {
    LRUHandle* h = reinterpret_cast<LRUHandle*>(handle);
    shard_[Shard(h->hash)].Release(handle);
  }

  void Erase(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    shard_[Shard(hash)].Erase(key, hash);
  }

  void* Value(Handle* handle) override {
    return reinterpret_cast<LRUHandle*>(handle)->value;
  }

  uint64_t NewId() override {
    std::unique_lock<std::mutex> lock_(id_mutex_);
    return ++(last_id_);
  }

  void Prune() override {
    for (int s = 0; s < kNumShards; s++) {
      shard_[s].Prune();
    }
  }

    std::size_t TotalCharge() const override {
    std::size_t total = 0;
    for (int s = 0; s < kNumShards; s++) {
      total += shard_[s].TotalCharge();
    }
    return total;
  }
};
};

}











