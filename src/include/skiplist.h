//两个作用 一个是防止头文件被多次编译 第二个是通过宏定义 编译中定义这个宏 就不会编译这个头文件
#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
#define STORAGE_LEVELDB_DB_SKIPLIST_H_

#include<atomic>
#include<cassert>
#include<stdlib.h>
#include "../utils/arena.h"
#include "../utils/random.h"
// 线程安全
// -------------
//
// 写操作需要外部同步，最可能的是使用互斥锁。也就是说这个数据结构是线程不安全的
// 读操作需要保证在读操作进行时 SkipList 不会被销毁。除此之外，读操作可以在没有任何内部锁定或同步的情况下进行。
//
// 不变性：
//
// (1) 分配的节点在 SkipList 被销毁之前永远不会被删除。这在代码中得到了简单的保证，因为我们从不删除任何跳表节点。
//
// (2) 节点的内容（除了 next/prev 指针）在节点被链接到 SkipList 之后是不可变的。只有 Insert() 修改列表，并且它会小心地初始化节点并使用发布存储来在一个或多个列表中发布节点。
//
// ... prev 与 next 指针的顺序 ...

//模板类 key可以任意类型 同时给予一个Comparator 跳表本身是有序的 所以这个地方的比较器给予了比较大小的方法
//比如初始化的时候SkipList<string,[](string a,string b)->{return a > b;}> list;
namespace kdb{


template<typename Key,class Comparator>
class SkipList
{
private:
    struct Node;//前置声明 具体实现放在cpp文件里可以
public:
    //构造函数接受一个比较器还有一个应该是虚拟内存得分配
    explicit SkipList(Comparator cmp, Arena* arena);

    //删除拷贝构造和复制构造 右值的应该也被删了
    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    // Insert key into the list.
    // 跳表不允许重复值
    //引用传递 相当于传递指针 不需要复制 如果key是对象 const表示不能修改
    void Insert(const Key& key);

    //判断是否已经存在key
    bool Contains(const Key& key) const;

public:
    //内部迭代器 类似stl的实现
    class Iterator 
    {
    public:
        explicit Iterator(const SkipList* list); //构造函数指定是指定跳表

        //判断迭代器指向的第一个元素是否有效
        bool Valid() const;

        // Returns the key at the current position.
        // REQUIRES: Valid()
        const Key& key() const;
        void Next();
        void Prev();
        void SeekToFirst();
        void SeekToLast();
        void Seek(const Key& target);


    private:
        const SkipList* list_; //构造函数维护的跳表结构
        Node *node_;    //跳表第一个节点 链接起来整个跳表 node的next的获取修改 是线程安全的 参考node接口        
    };

private:
    //private 包含了skiplist内部的变量 （原子类型） 同时包含了一些给public调用的接口
    enum {KMaxHeight = 12}; //跳表最多层数

    inline int GetMaxHeight() const
    {
        return max_height_.load(std::memory_order_acquire);
    }

    //跳表中新添加节点以及层数
    Node* NewNode(const Key &key,int height);

    Arena* const arena_; 

    //随机产生层数
    int RandomHeight();

    bool Equal(const Key& a,const Key& b) const{return compare_(a,b) == 0;}

    // Return the latest node with a key < key.
    // Return head_ if there is no such node.
    Node* FindLessThan(const Key& key) const;

    Node* FindLast()const;

    Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

    bool KeyIsAfterNode(const Key& key, Node* n) const;

private:
    std::atomic<int> max_height_;  // Height of the entire list

    // 比较器
    Comparator const compare_;

    //头节点 不存key
    Node* const head_;

    Random rnd_;

};

//后置实现
template <typename Key, class Comparator>
struct kdb::SkipList<Key, Comparator>::Node 
{
public:
    //构造函数 传入一个key去构造 
    explicit Node(const Key& k):key(k){}

    //通过将链接的访问和修改操作封装在方法中，可以方便地在这些方法中添加内存屏障，以确保多线程环境下的正确性和一致性。例如，在读取节点指针时，可以使用 memory_order_acquire，而在写入节点指针时，可以使用 memory_order_release。
    //n是跳表自下而上的第几层
    Node* Next(int n)
    {
        assert(n >= 0);
    // 读操作用acquire_load 保证之后的对该变量的操作不会被重拍在这个读操作之前
    // 返回层数
        return next_[n].load(std::memory_order_acquire);
    }
    void SetNext(int n,Node *x)
    {
        //原理同上
        assert(n >= 0);
        next_[n].store(x, std::memory_order_release);
    }

    Node* NoBarrier_Next(int n) 
    {
        assert(n >= 0);
        return next_[n].load(std::memory_order_relaxed);
    }
    void NoBarrier_SetNext(int n, Node* x) 
    {
        assert(n >= 0);
        next_[n].store(x, std::memory_order_relaxed);
    }

public:
    Key const key;
private:
    //指针数组 跳表的核心实现原理
    std::atomic<Node*> next_[1]; //这个地方 就是无锁结构 整个跳表只有next指针用的atomic 用内存序做保证
};



template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindLessThan(const Key& key) const 
{
    Node* n = head_;
    int level = GetMaxHeight() - 1;
    while(true)
    {
        assert(n == head_ || compare_(n->key, key) < 0);//头节点用0构造 永远最小
        Node* next = n->next_(level);

        //如果下个节点比key大 或者下个节点不存在 我应该去下一层
        if(next == nullptr || compare_(next->key,key) >= 0)
        {
            if(level == 0)
                return n;
            else level--;
        }
        else
            n = next;
    }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindLast()
    const {
  Node* n = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = n->Next(level);
    if (next == nullptr) {
      if (level == 0) {
        return n;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      n = next;
    }
  }
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
  // n不等于nullptr 并且key的值小于node的值
  return (n != nullptr) && (compare_(n->key, key) < 0);
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindGreaterOrEqual(const Key& key,
    Node** prev) const 
    {
        Node* n = head_;    
        int level = GetMaxHeight() - 1;
        while(true)
        {
            Node* next = n->Next(level);
            //这里是发现下一个元素的key < key 还要往后
            if(KeyIsAfterNode(key, next))
                n = next;
            else
            {
                if(prev != nullptr) prev[level] = n;

                //不是最后一层接着找 是的化 直接返回就可以如果找不到这里返回nullptr
                if(level == 0) return next;
                else level--; 
            }
        }
    }


template <typename Key, class Comparator>
typename SkipList<Key,Comparator>::Node* SkipList<Key,Comparator>::NewNode
    (const Key &key,int height)
    {
        //node节点自带一层 所以这个地方申请的时候加上层数-1 这个接口再找到插入位置后调用
        char *const node_memory = arena_->AllocateAligned(sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1));
        return new(node_memory) Node(key); //定位new 指定位置调用构造函数
    } 

template <typename Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list) 
{
    list_ = list;
    node_ = nullptr;
}

template <typename Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::Valid() const {
  return node_ != nullptr;
}

template <typename Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Next()
{
    assert(Valid());
    node_ = node_->Next(0);
}


template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Prev() {
  //寻找上一个比当前节点小的值
  assert(Valid());
  node_ = list_->FindLessThan(node_->key);
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToLast() {
  node_ = list_->FindLast();
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, nullptr);
}

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp,Arena*  arena)
    : compare_(cmp),
      arena_(arena),
      head_(NewNode(0 /* any key will do */, KMaxHeight)),
      max_height_(1),
      rnd_(0xdeadbeef) {
  for (int i = 0; i < KMaxHeight; i++) {
    head_->SetNext(i, nullptr);
  }
}

template <typename Key, class Comparator>
int SkipList<Key, Comparator>::RandomHeight() {
  // Increase height with probability 1 in kBranching
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < KMaxHeight && rnd_.OneIn(kBranching)) {
    height++;
  }
  assert(height > 0);
  assert(height <= KMaxHeight);
  return height;
}

//这里传入的prev参数为0 他只会找到比当前key大于等于的第一个节点
template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::Contains(const Key& key) const {
  Node* x = FindGreaterOrEqual(key, nullptr);
  if (x != nullptr && Equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key& key) 
{
    //先用prev找到待插入的位置 每一层
    Node *prev[KMaxHeight];
    Node* n = FindGreaterOrEqual(key, prev);

    //没有比它大的 返回nullptr 插在头节点之后
    //这里会让pre指向他插入位置的下一个元素 
    assert(n != nullptr || !Equal(key, n->key));

    //这里获取插入的高度
    //如果高度比当前高度大 应该让pre的高处的部分指向head
    //因为此时他们为nullptr findgreateorequal只会去判断已有的层数
    int height = RandomHeight();
    if (height > GetMaxHeight()) {
    for (int i = GetMaxHeight(); i < height; i++) {
        prev[i] = head_;
        }
        max_height_.store(height, std::memory_order_relaxed);
    }

    n = NewNode(key, height);
    for (int i = 0; i < height; i++) 
    {
    // NoBarrier_SetNext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    n->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
    prev[i]->SetNext(i, n);
    //这里的pre存放的就是每一层要插入的上个节点
    }
}


};
#endif